/*
  CC2530_BeaconJoin - full Zigbee-style discovery, join, routing, and security
  demo, now scaling to a multi-hop A-B-C line.

  A joiner needs no preconfigured PAN/channel/coordinator: it active-scans a
  channel list with Beacon Requests, collects the Zigbee beacons that come
  back, picks the best parent (permit-join, capacity, stack profile, LQI,
  depth), MAC-associates, and broadcasts a ZDO Device_annce. Routers also
  answer beacon requests and accept children from their own address pool, so
  a device can join through a router instead of the coordinator.

  Joined routers run the neighbor-aging protocol (15 s Link Status with
  bidirectional link costs; neighbors silent for 3 periods age out, a stale
  parent triggers a rejoin), AODV route discovery (Route Request broadcast ->
  unicast Route Reply, reverse routes recorded, intermediate routers
  rebroadcast/relay), and NWK data forwarding (a unicast NWK frame whose
  destination is not us is relayed to the route's next hop with radius-1).

  All NWK traffic is protected with Zigbee NWK security (AES-CCM* ENC-MIC-32
  under a pre-shared network key, computed on the nRF52840 hardware AES
  block). Each hop verifies, decrypts, and - when forwarding - re-encrypts.

  ROLES (build with -DNIUS_ZIGBEE_THIS_NODE=...):
    0x0001  coordinator A  (nwk 0x0000)
    0x0002  router      B  (joins A, becomes a parent, relays)
    0x0003  end device  C  (joins through B)

  To force the A-B-C line on a bench where all three radios hear each other,
  each end node simulates the other being out of range: A ignores C's frames
  and C ignores A's, so C can only reach A through B. Set
  -DNIUS_ZIGBEE_NO_RANGE_SIM=1 to disable that and let the mesh self-organize.
*/

#include <CC2530Radio.h>
#include <EEPROM.h>   // ArduinoNRF core: wear-levelled flash key-value store
#include <stdio.h>   // snprintf

#ifndef NIUS_ZIGBEE_PAN_ID
#define NIUS_ZIGBEE_PAN_ID 0x1A62
#endif

#ifndef NIUS_ZIGBEE_THIS_NODE
#define NIUS_ZIGBEE_THIS_NODE 0x0001
#endif

// 4-node line topology A-B-C-D (-DNIUS_ZIGBEE_LINE_TOPO=1): A coordinator, B and
// C routers, D end device. The range-sim chains them so each node hears only its
// line-neighbors, forcing a genuine 3-hop path D->C->B->A (for multi-hop +
// source-route OTA tests). Without it, 0x0003/0x0004 are plain end devices.
#ifndef NIUS_ZIGBEE_LINE_TOPO
#define NIUS_ZIGBEE_LINE_TOPO 0
#endif

#if NIUS_ZIGBEE_LINE_TOPO
#define ROLE_COORD  (NIUS_ZIGBEE_THIS_NODE == 0x0001)
#define ROLE_ROUTER (NIUS_ZIGBEE_THIS_NODE == 0x0002 || NIUS_ZIGBEE_THIS_NODE == 0x0003)
#define ROLE_END    (NIUS_ZIGBEE_THIS_NODE == 0x0004)
#define ROLE_END_D  (NIUS_ZIGBEE_THIS_NODE == 0x0004)
#else
// Node 0x0004 ("D") is a second end device, so a 4th board (e.g. a nice!nano)
// can join the same mesh as another leaf.
#define ROLE_COORD  (NIUS_ZIGBEE_THIS_NODE == 0x0001)
#define ROLE_ROUTER (NIUS_ZIGBEE_THIS_NODE == 0x0002)
#define ROLE_END    (NIUS_ZIGBEE_THIS_NODE == 0x0003 || NIUS_ZIGBEE_THIS_NODE == 0x0004)
#define ROLE_END_D  (NIUS_ZIGBEE_THIS_NODE == 0x0004)
#endif

// Secure commissioning (Zigbee-3.0-style key transport). When enabled, the
// joiner starts with ONLY the default Trust Center link key "ZigBeeAlliance09"
// and does not know the network key; the Trust Center (coordinator) delivers
// the network key after association inside an APS Transport-Key command,
// encrypted at the APS layer under the key-transport key and sent NWK-
// unsecured. Demonstrated 1-hop (coordinator + end device); build all nodes
// with -DNIUS_ZIGBEE_SECURE_JOIN=1 -DNIUS_ZIGBEE_IGNORE_SAVED=1.
#ifndef NIUS_ZIGBEE_SECURE_JOIN
#define NIUS_ZIGBEE_SECURE_JOIN 0
#endif

// Group multicast demo. When enabled, every node joins a demo group and the
// coordinator periodically broadcasts a group-addressed ZCL On/Off Toggle (a
// group APS frame inside a NWK broadcast to all rx-on devices); each member
// toggles its built-in LED. Build all nodes with -DNIUS_ZIGBEE_GROUPCAST=1.
#ifndef NIUS_ZIGBEE_GROUPCAST
#define NIUS_ZIGBEE_GROUPCAST 0
#endif

// Source-route relaying: when enabled, a router that receives a source-routed
// NWK data frame not addressed to it forwards it to the next hop named in the
// frame's source-route subframe (ZigbeeNwk::sourceRouteAction), instead of a
// route-table lookup. The forwarding decision is self-tested in
// CC2530_SourceRouting; this is its on-air wiring. Build with
// -DNIUS_ZIGBEE_SOURCEROUTE=1.
#ifndef NIUS_ZIGBEE_SOURCEROUTE
#define NIUS_ZIGBEE_SOURCEROUTE 0
#endif

static const uint16_t PAN_ID = NIUS_ZIGBEE_PAN_ID;
static const uint64_t EXT_PAN_ID = 0x1A62195E00000000ULL;
static const uint8_t COORD_CHANNEL = 15;
static const uint16_t THIS_NODE = NIUS_ZIGBEE_THIS_NODE;
static const bool IS_COORDINATOR = ROLE_COORD;
static const bool IS_PARENT_CAPABLE = ROLE_COORD || ROLE_ROUTER;
static const uint64_t THIS_IEEE = 0x1A62195E00000000ULL | THIS_NODE;
// RFD (end device) vs FFD (router): both rx-on + allocate-address. Marking the
// end device as RFD keeps a parent from treating it as a router neighbor and
// aging it out (routers are expected to send Link Status; an end device does
// not), which otherwise forced a fresh address on every re-association.
static const uint8_t JOINER_CAPABILITY = ROLE_END ? 0x88 : 0x8A;

// Deterministic addresses so the range-sim ignore lists are simple:
//   A pool 0x0001..0x000F -> B gets 0x0001
//   B pool 0x0031..0x003F -> C gets 0x0031
static const uint16_t COORD_POOL_FIRST = 0x0001, COORD_POOL_LAST = 0x000F;
#if NIUS_ZIGBEE_LINE_TOPO && (NIUS_ZIGBEE_THIS_NODE == 0x0003)
static const uint16_t ROUTER_POOL_FIRST = 0x0061, ROUTER_POOL_LAST = 0x006F;  // C's pool
#else
static const uint16_t ROUTER_POOL_FIRST = 0x0031, ROUTER_POOL_LAST = 0x003F;  // B's pool
#endif
static const uint16_t EXPECTED_C_ADDR = 0x0031;

// Range-sim ignore lists: the short addresses + IEEEs of peers this node
// pretends not to hear, so a desired topology is forced on a bench where every
// radio is in range. NO_RANGE_SIM => effectively empty (the mesh self-organizes).
#if defined(NIUS_ZIGBEE_NO_RANGE_SIM)
static const uint16_t IGNORE_SHORTS[] = {0xFFFF};
static const uint64_t IGNORE_IEEES[] = {0xFFFFFFFFFFFFFFFFULL};
#elif NIUS_ZIGBEE_LINE_TOPO
// line A(0x0000)-B(0x0001)-C(0x0031)-D(0x0061): each hears only its neighbors.
#if ROLE_COORD                                   // A: ignore C, D
static const uint16_t IGNORE_SHORTS[] = {0x0031, 0x0061};
static const uint64_t IGNORE_IEEES[] = {0x1A62195E00000003ULL, 0x1A62195E00000004ULL};
#elif (NIUS_ZIGBEE_THIS_NODE == 0x0002)          // B: ignore D
static const uint16_t IGNORE_SHORTS[] = {0x0061};
static const uint64_t IGNORE_IEEES[] = {0x1A62195E00000004ULL};
#elif (NIUS_ZIGBEE_THIS_NODE == 0x0003)          // C: ignore A
static const uint16_t IGNORE_SHORTS[] = {0x0000};
static const uint64_t IGNORE_IEEES[] = {0x1A62195E00000001ULL};
#else                                            // D: ignore A, B
static const uint16_t IGNORE_SHORTS[] = {0x0000, 0x0001};
static const uint64_t IGNORE_IEEES[] = {0x1A62195E00000001ULL, 0x1A62195E00000002ULL};
#endif
#elif ROLE_COORD
static const uint16_t IGNORE_SHORTS[] = {EXPECTED_C_ADDR};          // ignore C
static const uint64_t IGNORE_IEEES[] = {0x1A62195E00000000ULL | 0x0003};
#elif ROLE_END
static const uint16_t IGNORE_SHORTS[] = {0x0000};                   // ignore A
static const uint64_t IGNORE_IEEES[] = {0x1A62195E00000000ULL | 0x0001};
#else
static const uint16_t IGNORE_SHORTS[] = {0xFFFF};
static const uint64_t IGNORE_IEEES[] = {0xFFFFFFFFFFFFFFFFULL};
#endif
static const uint8_t IGNORE_COUNT = sizeof(IGNORE_SHORTS) / sizeof(IGNORE_SHORTS[0]);

#if NIUS_ZIGBEE_LINE_TOPO
// Each line node joins a parent at exactly this depth (B<-A depth 0, C<-B
// depth 1, D<-C depth 2), so the chain forms deterministically regardless of
// the pool-assigned short addresses. Paired with the IEEE association-ignore
// lists (A accepts only B, B only C, C only D), this gives a clean 3-hop line.
#if (NIUS_ZIGBEE_THIS_NODE == 0x0002)
static const uint8_t LINE_PARENT_DEPTH = 0;
#elif (NIUS_ZIGBEE_THIS_NODE == 0x0003)
static const uint8_t LINE_PARENT_DEPTH = 1;
#else  // 0x0004 (D)
static const uint8_t LINE_PARENT_DEPTH = 2;
#endif
#endif

#if NIUS_ZIGBEE_SECURE_JOIN
// Secure-join demo is 1-hop (joiner <-> Trust Center): scan only the
// coordinator's channel so the joiner associates with the TC directly rather
// than a router that cannot deliver the network key.
static const uint8_t SCAN_CHANNELS[] = {COORD_CHANNEL};
#else
static const uint8_t SCAN_CHANNELS[] = {11, 15, 20, 25};
#endif
static const uint8_t SCAN_CHANNEL_COUNT =
    sizeof(SCAN_CHANNELS) / sizeof(SCAN_CHANNELS[0]);
static const uint16_t SCAN_DWELL_MS = 250;
static const uint8_t MAX_ASSOC_ATTEMPTS = 4;

static const uint8_t NETWORK_KEY[16] = {
#ifdef NIUS_ZIGBEE_WRONG_KEY
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
#else
    0x1A, 0x62, 0x19, 0x5E, 0x4B, 0x3C, 0x2D, 0x1E,
    0x0F, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07,
#endif
};

CC2530Radio radio;
ZigbeeNeighbor neighborStorage[8];
ZigbeeNeighborTable neighbors(neighborStorage, 8);
ZigbeeNetwork network;
ZigbeeRoute routeStorage[8];
ZigbeeRouteTable routes(routeStorage, 8);
ZigbeeRouting routing;
ZigbeeSecurity security;

ZigbeeParentCandidate chosenParent;
uint8_t scanChannel = 0;
uint8_t pendingRreqId = 0;
uint32_t nextDiscoveryAt = 0, nextActionAt = 0, nextStatus = 0;
uint32_t forwarded = 0;
bool announced = false, parentReady = false;
uint8_t zdoSequence = 0;

// APS end-to-end acknowledged delivery (the data plane). The end device
// sends an acked APS data frame to the coordinator every few seconds; the
// coordinator answers with an APS ACK carrying the same APS counter, routed
// back over the mesh. Lost round trips are retransmitted until the ACK
// arrives or the retry budget runs out - so the delivered/queued ratio is
// the multi-hop reliability the raw ping lacked.
ZigbeeApsRetransmit apsRetx;
ApsPending apsPendingStorage[4];
ZigbeeApsDuplicateTable apsDupe;   // receiver side: reject reprocessed retransmits
ApsDupeEntry apsDupeStorage[6];
uint32_t apsDuplicates = 0;
static const uint8_t APS_ENDPOINT = 1;
static const uint16_t APS_CLUSTER = 0x1042;   // a private test cluster
static const uint16_t APS_PROFILE = 0x0104;   // Home Automation
uint8_t apsCounter = 0;
uint32_t apsSeq = 0, nextApsSendAt = 0;

#if NIUS_ZIGBEE_SECURE_JOIN
// Secure-join key transport. The key-transport command rides an APS data frame
// on a reserved endpoint/cluster, NWK-unsecured, with the APS payload being the
// ZigbeeApsSecurity envelope (aux header + ciphertext + MIC).
static const uint8_t KEY_XPORT_EP = 2;
static const uint16_t KEY_XPORT_CLUSTER = 0x0009;  // key-transport carrier
bool keyInstalled = false;          // joiner: network key received + installed?
uint32_t apsSecCounter = 1;         // TC: APS security frame counter
bool pendingKeyXport = false;       // TC: a joiner is awaiting its key
uint64_t keyXportIeee = 0;
uint16_t keyXportAddr = 0;
uint8_t keyXportTries = 0;
uint32_t keyXportAt = 0;
// Key-transport key derived ONCE at startup from the default TC link key, then
// reused by the TC's send and the joiner's receive. Both ends use the same
// constant link key, so it is identical on the TC and the joiner.
uint8_t gKtk[16];
#endif

#if NIUS_ZIGBEE_GROUPCAST
static const uint16_t DEMO_GROUP = 0x0001;
uint16_t groupStorage[4];
ZigbeeGroupTable groups(groupStorage, 4);
bool groupOnOff = false;             // local On/Off state, mirrored on the LED
uint8_t groupZclSeq = 0;
uint32_t nextGroupCastAt = 12000;    // coordinator broadcasts ~8 s apart
#endif

// Application-level reliable ZDO query: the Mgmt_Lqi_rsp is the acknowledgement,
// so re-send the request until it arrives (ZDO frames carry no APS ack here).
uint32_t nextMgmtLqiCycleAt = 20000;  // start a fresh query 20 s after boot
bool mgmtLqiPending = false;
uint32_t mgmtLqiRetryAt = 0;
uint8_t mgmtLqiSeq = 0, mgmtLqiTries = 0;

// Coordinator side: a received Mgmt_Lqi_req is answered from loop()
// (sendPendingMgmtLqiRsp), not the RX callback, so the long ZDO response is
// not transmitted while the CC2530 is still busy finishing the received
// request frame - sending it from the callback dropped it on air.
bool pendingLqiRsp = false;
uint16_t lqiRspTo = 0;
uint8_t lqiRspSeq = 0, lqiRspStart = 0;

// Persistence: every node saves its network state + outgoing security frame
// counter to flash so a power cycle restores the network identity (no
// re-scan) and never rewinds the counter (which would let old secured frames
// replay). The counter is stored with a +1024 margin so a crash between
// saves cannot reuse a value.
uint32_t nextSaveAt = 0;
static const uint32_t SAVE_PERIOD_MS = 20000;
static const uint32_t COUNTER_MARGIN = 1024;

const char* roleName() {
  switch (THIS_NODE) {
    case 0x0001: return "A/coordinator";
    case 0x0002: return "B/router";
    case 0x0003: return ROLE_ROUTER ? "C/router" : "C/end";
    case 0x0004: return "D/end";
    default: return "?/node";
  }
}

bool ignoredShort(uint16_t macSrc) {
  for (uint8_t i = 0; i < IGNORE_COUNT; ++i)
    if (macSrc == IGNORE_SHORTS[i]) return true;
  return false;
}
bool ignoredIeee(uint64_t ieee) {
  for (uint8_t i = 0; i < IGNORE_COUNT; ++i)
    if (ieee == IGNORE_IEEES[i]) return true;
  return false;
}

void printHex8(uint8_t v) { if (v < 0x10) Serial.print('0'); Serial.print(v, HEX); }
void printHex16(uint16_t v) {
  for (uint16_t m = 0x1000; m > 1 && v < m; m >>= 4) Serial.print('0');
  Serial.print(v, HEX);
}
void printHex64(uint64_t v) { for (int i = 7; i >= 0; --i) printHex8((uint8_t)(v >> (i * 8))); }
void ieeeToBytes(uint64_t ieee, uint8_t* out) { for (uint8_t i = 0; i < 8; ++i) out[i] = (uint8_t)(ieee >> (8 * i)); }

bool applyAddress(uint16_t panId, uint16_t shortAddress) {
  uint8_t ieeeBytes[8];
  ieeeToBytes(THIS_IEEE, ieeeBytes);
  bool ok = radio.setAddress(panId, shortAddress, ieeeBytes);
  ok = ok && radio.configureMac(
      CC2530Radio::kMacFilter | CC2530Radio::kMacAutoAck |
      CC2530Radio::kMacCcaTx, 3);
  return ok;
}

// Once a router has joined, turn it into a parent: open joining, give it an
// address pool disjoint from the coordinator's, and start answering beacons.
void becomeParent() {
  if (parentReady || !ROLE_ROUTER || !network.isJoined()) return;
  network.configureAddressPool(ROUTER_POOL_FIRST, ROUTER_POOL_LAST);
  network.permitJoining(0xFF);
  parentReady = true;
  Serial.println("router is now a parent (permit join open)");
}

void sendOurBeacon() {
  uint8_t payload[ZigbeeNwk::kBeaconPayloadLen];
  uint8_t n = ZigbeeNwk::buildBeaconPayload(
      payload, sizeof(payload), network.info().extendedPanId,
      network.info().depth, network.isJoiningPermitted(),
      network.isJoiningPermitted(), network.info().updateId);
  radio.sendBeacon(network.info().panId, network.info().nwkAddress,
                   network.isCoordinator(), network.isJoiningPermitted(),
                   payload, n);
}

// ---------------------------------------------------------------- MAC commands

void onMacCommand(const MacCommandFrame& frame, int8_t rssi, uint8_t lqi) {
  (void)rssi;

  if (frame.commandId == MAC_CMD_BEACON_REQUEST) {
    if (IS_PARENT_CAPABLE && network.isJoined() && network.isJoiningPermitted()) {
      sendOurBeacon();
    }
    return;
  }

  if (frame.commandId == MAC_CMD_ASSOCIATION_REQUEST &&
      IS_PARENT_CAPABLE && network.isJoined() &&
      frame.dstPanId == network.info().panId &&
      frame.dstShort == network.info().nwkAddress) {
    if (ignoredIeee(frame.srcIeee)) return;  // simulated out of range
    MacAssociationRequest request;
    if (!ZigbeeMac::parseAssociationRequest(frame, request)) return;

    ZigbeeAssociationDecision decision =
        network.handleAssociationRequest(frame.srcIeee, request, lqi & 0x7F);
    if (decision.accepted) security.resetReplayTable();
#if NIUS_ZIGBEE_SECURE_JOIN
    // Trust Center: schedule delivery of the network key to this joiner. A
    // short delay lets the joiner finish setting its MAC address filter so the
    // unicast key-transport is accepted.
    if (decision.accepted && IS_COORDINATOR) {
      pendingKeyXport = true;
      keyXportIeee = frame.srcIeee;
      keyXportAddr = decision.assignedAddress;
      keyXportTries = 0;
      keyXportAt = millis() + 350;
    }
#endif
    bool ok = radio.sendAssociationResponse(
        network.info().panId, frame.srcIeee, network.info().nwkAddress,
        decision.assignedAddress, decision.status, true);

    Serial.print("ASSOC req ieee=0x");
    printHex64(frame.srcIeee);
    Serial.print(" -> status=0x");
    printHex8(decision.status);
    Serial.print(" addr=0x");
    printHex16(decision.assignedAddress);
    Serial.println(ok ? " rsp=sent" : " rsp=FAILED");
    return;
  }

  if (frame.commandId == MAC_CMD_ASSOCIATION_RESPONSE && !IS_COORDINATOR &&
      frame.dstIeee == THIS_IEEE) {
    MacAssociationResponse response;
    if (!ZigbeeMac::parseAssociationResponse(frame, response)) return;

    Serial.print("ASSOC rsp status=0x");
    printHex8(response.status);
    Serial.print(" addr=0x");
    printHex16(response.shortAddress);
    Serial.println();

    if (response.status == MAC_ASSOC_SUCCESS &&
        network.completeJoin(response.shortAddress, chosenParent.shortAddress,
                             chosenParent.depth)) {
      network.resetJoinAttempts();
      security.resetReplayTable();
      network.noteParent(chosenParent.shortAddress, 0,
                         chosenParent.depth == 0 ? ZB_DEVICE_COORDINATOR
                                                 : ZB_DEVICE_ROUTER,
                         chosenParent.depth, chosenParent.lqi, true);
      // Parent is the next hop toward the coordinator until discovery refines it.
      routes.upsert(ZB_NWK_ADDR_COORDINATOR, chosenParent.shortAddress,
                    ZB_ROUTE_ACTIVE);
      applyAddress(chosenParent.panId, response.shortAddress);
      radio.setPromiscuous(false);  // join done: enable the MAC address filter
                                    // (also re-enables NWK decryption)
      Serial.print("JOINED via 0x");
      printHex16(chosenParent.shortAddress);
      Serial.print(" pan=0x");
      printHex16(chosenParent.panId);
      Serial.print(" ch=");
      Serial.print(chosenParent.channel);
      Serial.print(" addr=0x");
      printHex16(response.shortAddress);
      Serial.println();
      announced = false;
    }
  }
}

// --------------------------------------------------------------------- scan

void onBeacon(const MacBeaconFrame& beacon, int8_t rssi, uint8_t lqi) {
  if (IS_COORDINATOR || !network.isScanning()) return;
  if (ignoredShort(beacon.srcShort)) return;  // simulated out of range
  NwkBeaconPayload payload;
  if (!ZigbeeNwk::parseBeaconPayload(beacon.payload, beacon.payloadLen, payload)) return;
#if NIUS_ZIGBEE_LINE_TOPO
  if (payload.deviceDepth != LINE_PARENT_DEPTH) return;  // join only the line parent
#endif
  if (network.noteBeacon(scanChannel, beacon, payload, rssi, lqi & 0x7F)) {
    Serial.print("  beacon ch=");
    Serial.print(scanChannel);
    Serial.print(" pan=0x"); printHex16(beacon.srcPanId);
    Serial.print(" from=0x"); printHex16(beacon.srcShort);
    Serial.print(" depth="); Serial.print(payload.deviceDepth);
    Serial.print(" permit="); Serial.print(beacon.associationPermit ? "yes" : "no");
    Serial.print(" lqi="); Serial.println(lqi & 0x7F);
  }
}

#if NIUS_ZIGBEE_SECURE_JOIN
// Joiner side: an APS Transport-Key arrived (NWK-unsecured, APS-encrypted under
// the key-transport key). Decrypt it with our link key and install the network
// key so all later NWK traffic is secured.
void installNetworkKeyFrom(const uint8_t* blob, uint8_t blobLen) {
  uint8_t plain[40];
  uint8_t n = ZigbeeApsSecurity::openCommand(blob, blobLen, /*apsHeaderLen=*/1,
                                             gKtk, plain, sizeof(plain));
  if (n == 0) {
    Serial.println("key xport: APS decrypt/MIC FAILED");
    return;
  }
  ApsTransportKey t;
  if (!ZigbeeApsKey::parseTransportNetworkKey(plain, n, t)) {
    Serial.println("key xport: parse FAILED");
    return;
  }
  security.setNetworkKey(t.key, t.keySeqNumber);
  keyInstalled = true;
  Serial.print("secure join: NETWORK KEY installed (seq ");
  Serial.print(t.keySeqNumber); Serial.println(") - NWK security live");
}

// Trust Center side: deliver the network key to a freshly-joined device. The
// Transport-Key command is APS-encrypted under the key-transport key (derived
// from the default TC link key) and carried in an NWK-unsecured APS data frame,
// since the joiner cannot yet decrypt the NWK layer. Retried a few times until
// the joiner confirms by sending a (now secured) Device_annce.
void serviceKeyTransport() {
  if (!pendingKeyXport) return;
  if ((int32_t)(millis() - keyXportAt) < 0) return;
  if (keyXportTries >= 12) {
    pendingKeyXport = false;
    Serial.println("key xport: no confirmation after 12 tries");
    return;
  }
  ++keyXportTries;
  keyXportAt = millis() + 2000;

  ApsTransportKey t;
  t.keyType = APS_KEY_STANDARD_NETWORK;
  memcpy(t.key, NETWORK_KEY, 16);
  t.keySeqNumber = security.keySequence();
  t.destAddress = keyXportIeee;
  t.srcAddress = THIS_IEEE;
  uint8_t cmd[40];
  uint8_t cmdLen = ZigbeeApsKey::buildTransportNetworkKey(cmd, sizeof(cmd), t);
  if (cmdLen == 0) { pendingKeyXport = false; return; }

  const uint8_t apsHeader[1] = {0x21};  // 1-byte AAD prefix (key-transport)
  uint8_t secured[80];
  uint8_t securedLen = ZigbeeApsSecurity::secureCommand(
      apsHeader, sizeof(apsHeader), gKtk, APS_SEC_KEY_KEY_TRANSPORT, THIS_IEEE,
      apsSecCounter++, cmd, cmdLen, secured, sizeof(secured));
  if (securedLen == 0) { pendingKeyXport = false; return; }

  uint8_t apdu[ZigbeeAps::kMaxFrame];
  uint8_t apduLen = ZigbeeAps::buildDataFrame(
      apdu, sizeof(apdu), KEY_XPORT_EP, KEY_XPORT_CLUSTER, APS_PROFILE,
      KEY_XPORT_EP, apsCounter++, secured, securedLen, /*ackRequest=*/false);
  if (apduLen == 0) { pendingKeyXport = false; return; }

  bool ok = radio.sendNwkDataUnsecured(
      network.info().panId, keyXportAddr, network.info().nwkAddress,
      keyXportAddr, network.info().nwkAddress, apdu, apduLen,
      ZigbeeNwk::kDefaultRadius, /*ackRequest=*/true);
  Serial.print("key xport -> 0x"); printHex16(keyXportAddr);
  Serial.print(" try "); Serial.print(keyXportTries);
  Serial.println(ok ? " (encrypted netkey)" : " TX-FAIL");
}
#endif  // NIUS_ZIGBEE_SECURE_JOIN

#if NIUS_ZIGBEE_GROUPCAST
// Coordinator: broadcast a group-addressed ZCL On/Off Toggle to the demo group.
// The group APS frame rides a NWK broadcast to all rx-on devices (NWK-secured);
// every member toggles its LED, non-members ignore it.
void serviceGroupCast() {
  if (!IS_COORDINATOR || !network.isJoined()) return;
  if ((int32_t)(millis() - nextGroupCastAt) < 0) return;
  nextGroupCastAt = millis() + 8000;

  uint8_t zcl[8];
  uint8_t zn = ZigbeeZcl::buildCommandFrame(zcl, sizeof(zcl),
                                            nzb::ZCL_FRAME_CLUSTER_SPECIFIC,
                                            groupZclSeq++,
                                            nzb::ZCL_ON_OFF_CMD_TOGGLE, nullptr, 0);
  uint8_t apdu[ZigbeeAps::kMaxFrame];
  uint8_t an = ZigbeeAps::buildGroupDataFrame(apdu, sizeof(apdu), DEMO_GROUP,
                                              ZigbeeZcl::kClusterOnOff, APS_PROFILE,
                                              APS_ENDPOINT, apsCounter++, zcl, zn);
  bool ok = radio.sendNwkData(network.info().panId, ZigbeeMac::kBroadcastShort,
                              network.info().nwkAddress,
                              ZigbeeNwk::kBroadcastRxOnWhenIdle,
                              network.info().nwkAddress, apdu, an,
                              ZigbeeNwk::kDefaultRadius, false);
  Serial.print("GROUPCAST -> group 0x"); printHex16(DEMO_GROUP);
  Serial.println(ok ? " Toggle" : " Toggle TX-FAIL");
}
#endif  // NIUS_ZIGBEE_GROUPCAST

void runActiveScan() {
  Serial.println("scanning...");
  network.beginScan(ROLE_END ? ZB_DEVICE_END_DEVICE : ZB_DEVICE_ROUTER);
  radio.setPromiscuous(true);
  for (uint8_t i = 0; i < SCAN_CHANNEL_COUNT; ++i) {
    scanChannel = SCAN_CHANNELS[i];
    if (!radio.setChannel(scanChannel)) continue;
    radio.sendBeaconRequest();
    uint32_t until = millis() + SCAN_DWELL_MS;
    while ((int32_t)(millis() - until) < 0) radio.poll();
  }

  const ZigbeeParentCandidate* parent = network.selectParent();
#if NIUS_ZIGBEE_SECURE_JOIN
  // Secure-join demo is 1-hop: only commission directly with the Trust Center
  // (the coordinator, depth 0). A router cannot deliver the network key here
  // (that needs an APS tunnel), so refuse it and re-scan.
  if (parent && parent->depth != 0) {
    Serial.println("secure join: ignoring non-TC parent, re-scanning");
    nextActionAt = millis() + 3000;
    return;
  }
#endif
  if (!parent) {
    Serial.print("scan done: ");
    Serial.print(network.candidateCount());
    Serial.println(" network(s), none joinable - retrying in 5 s");
    nextActionAt = millis() + 5000;
    return;
  }
  chosenParent = *parent;
  network.beginJoiningCandidate(chosenParent);
  radio.setChannel(chosenParent.channel);
  applyAddress(chosenParent.panId, ZigbeeMac::kBroadcastShort);
  Serial.print("parent chosen: pan=0x"); printHex16(chosenParent.panId);
  Serial.print(" addr=0x"); printHex16(chosenParent.shortAddress);
  Serial.print(" depth="); Serial.print(chosenParent.depth);
  Serial.print(" ch="); Serial.print(chosenParent.channel);
  Serial.print(" lqi="); Serial.println(chosenParent.lqi);
  nextActionAt = millis();
}

void sendDeviceAnnounce() {
  uint8_t payload[ZigbeeZdo::kMaxPayload];
  uint8_t n = ZigbeeZdo::buildDeviceAnnounce(
      payload, sizeof(payload), zdoSequence++, network.info().nwkAddress,
      THIS_IEEE, JOINER_CAPABILITY);
  radio.sendZdoCommand(network.info().panId, ZigbeeMac::kBroadcastShort,
                       network.info().nwkAddress,
                       ZigbeeNwk::kBroadcastRxOnWhenIdle,
                       network.info().nwkAddress, ZDO_DEVICE_ANNCE, payload, n,
                       ZigbeeNwk::kDefaultRadius, false);
  Serial.println("ZDO Device_annce sent");
}

// ------------------------------------------------- NWK commands (link/route)

void onNwkCommand(const MacDataFrame& mac, const NwkCommandFrame& nwk,
                  int8_t rssi, uint8_t lqi) {
  (void)rssi;
  if (ignoredShort(mac.srcShort)) return;
  uint8_t linkCost = ZigbeeNetwork::costFromLqi(lqi & 0x7F);

  if (nwk.commandId == NWK_CMD_LINK_STATUS) {
    NwkLinkStatusCommand command;
    if (!ZigbeeNwk::parseLinkStatusPayload(nwk.payload, nwk.payloadLen, command)) return;
    if (network.handleLinkStatus(nwk.srcShort, command, lqi & 0x7F)) {
      Serial.print("LINK STATUS from 0x"); printHex16(nwk.srcShort);
      Serial.print(" entries="); Serial.println(command.entryCount);
    }
    return;
  }

  if (nwk.commandId == NWK_CMD_ROUTE_REQUEST && network.isJoined()) {
    NwkRouteRequestCommand rreq;
    if (!ZigbeeNwk::parseRouteRequestPayload(nwk.payload, nwk.payloadLen, rreq)) return;
    ZigbeeRreqDecision d = routing.handleRouteRequest(
        network.info().nwkAddress, IS_PARENT_CAPABLE, mac.srcShort,
        nwk.srcShort, rreq, linkCost);
    if (d.duplicate) return;
    Serial.print("RREQ id="); Serial.print(rreq.routeRequestId);
    Serial.print(" for 0x"); printHex16(rreq.destination);
    Serial.print(" from 0x"); printHex16(nwk.srcShort);
    Serial.print(" via 0x"); printHex16(mac.srcShort);
    if (d.replyAsDestination) {
      uint8_t payload[16];
      uint8_t n = ZigbeeNwk::buildRouteReplyPayload(
          payload, sizeof(payload), rreq.routeRequestId, nwk.srcShort,
          network.info().nwkAddress, d.pathCost);
      bool ok = n > 0 && radio.sendNwkCommand(
          network.info().panId, d.replyTo, network.info().nwkAddress,
          nwk.srcShort, network.info().nwkAddress, NWK_CMD_ROUTE_REPLY,
          payload, n, ZigbeeNwk::kDefaultRadius, true);
      Serial.println(ok ? " -> RREP sent" : " -> RREP FAILED");
    } else if (d.rebroadcast) {
      uint8_t payload[16];
      uint8_t n = ZigbeeNwk::buildRouteRequestPayload(
          payload, sizeof(payload), rreq.routeRequestId, rreq.destination,
          d.pathCost, rreq.manyToOne,
          rreq.destinationIeeePresent ? &rreq.destinationIeee : nullptr,
          rreq.multicast);
      if (n > 0)
        radio.sendNwkCommand(network.info().panId, ZigbeeMac::kBroadcastShort,
                             network.info().nwkAddress,
                             ZigbeeNwk::kBroadcastAllRouters, nwk.srcShort,
                             NWK_CMD_ROUTE_REQUEST, payload, n,
                             nwk.radius > 1 ? nwk.radius - 1 : 1, false);
      Serial.println(" -> rebroadcast");
    } else {
      Serial.println(" (recorded)");
    }
    return;
  }

  if (nwk.commandId == NWK_CMD_ROUTE_REPLY && network.isJoined()) {
    NwkRouteReplyCommand rrep;
    if (!ZigbeeNwk::parseRouteReplyPayload(nwk.payload, nwk.payloadLen, rrep)) return;
    ZigbeeRrepDecision d = routing.handleRouteReply(
        network.info().nwkAddress, mac.srcShort, rrep, linkCost);
    if (d.routeInstalled) {
      Serial.print("RREP id="); Serial.print(rrep.routeRequestId);
      Serial.print(": route to 0x"); printHex16(rrep.responder);
      Serial.print(" via 0x"); printHex16(mac.srcShort);
      Serial.println(" ACTIVE");
    } else if (d.forward) {
      uint8_t payload[16];
      uint8_t n = ZigbeeNwk::buildRouteReplyPayload(
          payload, sizeof(payload), rrep.routeRequestId, rrep.originator,
          rrep.responder, d.pathCost);
      if (n > 0)
        radio.sendNwkCommand(network.info().panId, d.forwardTo,
                             network.info().nwkAddress, rrep.originator,
                             nwk.srcShort, NWK_CMD_ROUTE_REPLY, payload, n,
                             ZigbeeNwk::kDefaultRadius, true);
      Serial.println("RREP forwarded");
    }
    return;
  }
}

// ------------------------------------------------- NWK data (forward + ping)

void onNwkData(const MacDataFrame& mac, const NwkDataFrame& nwk, int8_t rssi,
               uint8_t lqi) {
  (void)rssi; (void)lqi;
  if (!network.isJoined()) return;
  if (ignoredShort(mac.srcShort)) return;

  // Reverse-route learning: a frame from NWK source S that arrived via MAC
  // neighbor N means we can reach S by sending to N. This gives every relay
  // (and the destination) a return path without a second route discovery -
  // here it lets A answer C's ping back through B instead of trying the
  // simulated-out-of-range direct A->C link.
  if (nwk.srcShort != network.info().nwkAddress && mac.srcShort < 0xFFF8 &&
      nwk.srcShort < 0xFFF8) {
    routes.upsert(nwk.srcShort, mac.srcShort, ZB_ROUTE_ACTIVE);
  }

#if NIUS_ZIGBEE_SOURCEROUTE
  // Source-route relaying: if the frame carries a source-route subframe, follow
  // it (next hop named in the frame) rather than the route table. The frame is
  // rebuilt with the advanced relay index and forwarded unsecured.
  if (nwk.sourceRoute && nwk.dstShort != network.info().nwkAddress) {
    uint16_t nextHop = 0;
    uint8_t outIdx = 0;
    uint8_t act = ZigbeeNwk::sourceRouteAction(nwk, network.info().nwkAddress,
                                               nextHop, outIdx);
    if (act != NWK_SR_RELAY) return;  // DROP (off-path); DELIVER can't happen here
    uint16_t relays[8];
    uint8_t rc = nwk.srRelayCount < 8 ? nwk.srRelayCount : 8;
    for (uint8_t i = 0; i < rc; ++i) ZigbeeNwk::getDataFrameRelay(nwk, i, relays[i]);
    uint8_t payload[ZigbeeNwk::kMaxPayload];
    if (nwk.payloadLen > sizeof(payload)) return;
    memcpy(payload, nwk.payload, nwk.payloadLen);
    uint8_t out[ZigbeeNwk::kMaxFrame + 24];
    uint8_t n = ZigbeeNwk::buildDataFrameSourceRouted(
        out, sizeof(out), nwk.dstShort, nwk.srcShort,
        (uint8_t)(nwk.radius > 1 ? nwk.radius - 1 : 1), nwk.sequence, relays, rc,
        outIdx, payload, nwk.payloadLen);
    bool ok = n > 0 && radio.sendData(network.info().panId, nextHop,
                                      network.info().nwkAddress, out, n, true);
    if (ok) ++forwarded;
    Serial.print("SRCROUTE relay 0x"); printHex16(nwk.srcShort);
    Serial.print("->0x"); printHex16(nwk.dstShort);
    Serial.print(" via 0x"); printHex16(nextHop);
    Serial.println(ok ? "" : " FAILED");
    return;
  }
#endif
  // Forwarding: a unicast frame whose NWK destination is not us is relayed to
  // the route's next hop (radius-1, re-encrypted for the next hop). Copy the
  // payload out of the decrypt scratch before re-sending.
  if (nwk.dstShort != network.info().nwkAddress && nwk.dstShort < 0xFFF8) {
    uint16_t nextHop = routing.nextHopFor(nwk.dstShort);
    if (nextHop != ZigbeeRouting::kNoNextHop && nwk.radius > 1 &&
        IS_PARENT_CAPABLE) {
      uint8_t buf[ZigbeeNwk::kMaxPayload];
      if (nwk.payloadLen > sizeof(buf)) return;
      memcpy(buf, nwk.payload, nwk.payloadLen);
      bool ok = radio.sendNwkData(network.info().panId, nextHop,
                                  network.info().nwkAddress, nwk.dstShort,
                                  nwk.srcShort, buf, nwk.payloadLen,
                                  (uint8_t)(nwk.radius - 1), true);
      if (ok) ++forwarded;
      Serial.print("FORWARD 0x"); printHex16(nwk.srcShort);
      Serial.print("->0x"); printHex16(nwk.dstShort);
      Serial.print(" via 0x"); printHex16(nextHop);
      Serial.println(ok ? "" : " FAILED");
    }
    return;
  }

  // Frames addressed to us are handled by the APS callbacks (onApsData /
  // onApsAck); onNwkData's job here is forwarding + reverse-route learning.
}

// Route an APS frame toward `dst` over the mesh (next hop from the route
// table, falling back to the address itself for a direct neighbor).
bool sendApsRouted(uint16_t dst, const uint8_t* apdu, uint8_t apduLen) {
  uint16_t nextHop = routing.nextHopFor(dst);
  if (nextHop == ZigbeeRouting::kNoNextHop) nextHop = dst;
  return radio.sendNwkData(network.info().panId, nextHop,
                           network.info().nwkAddress, dst,
                           network.info().nwkAddress, apdu, apduLen,
                           ZigbeeNwk::kDefaultRadius, true);
}

// Coordinator/parent side: a received acked APS data frame is answered with
// an APS ACK routed back to the sender (the reverse route was just learned
// in onNwkData).
void onApsData(const MacDataFrame& mac, const NwkDataFrame& nwk,
               const ApsDataFrame& aps, int8_t rssi, uint8_t lqi) {
  (void)rssi; (void)lqi;
#if NIUS_ZIGBEE_SECURE_JOIN
  // Joiner: the network key arrives here (NWK-unsecured because we have no key
  // yet) on the reserved key-transport cluster, APS-encrypted under our link key.
  if (aps.clusterId == KEY_XPORT_CLUSTER && !IS_COORDINATOR && !keyInstalled) {
    installNetworkKeyFrom(aps.payload, aps.payloadLen);
    return;
  }
#endif
#if NIUS_ZIGBEE_GROUPCAST
  // Group multicast: a group-addressed On/Off command we are a member of toggles
  // the LED. (Non-members and other groups are ignored.)
  if (aps.deliveryMode == APS_DELIVERY_GROUP &&
      aps.clusterId == ZigbeeZcl::kClusterOnOff) {
    if (groups.isMember(aps.groupAddress)) {
      ZclFrame zcl;
      if (ZigbeeZcl::parseFrame(aps.payload, aps.payloadLen, zcl)) {
        ZigbeeZcl::applyOnOffCommand(zcl.commandId, groupOnOff);
        digitalWrite(LED_BUILTIN, groupOnOff ? HIGH : LOW);
        Serial.print("GROUPCAST rx group 0x"); printHex16(aps.groupAddress);
        Serial.print(" -> LED="); Serial.println(groupOnOff ? "ON" : "OFF");
      }
    }
    return;
  }
#endif
  if (aps.clusterId != APS_CLUSTER) return;  // leave ZDO etc. alone

  // A retransmit that reached us must STILL be acked (the sender lost the
  // previous ACK), but the duplicate must not be processed by the app twice.
  bool isNew = apsDupe.checkAndRecord(nwk.srcShort, aps.srcEndpoint,
                                      aps.counter, millis());

  if (aps.ackRequest) {
    uint8_t ack[ZigbeeAps::kBaseHeaderLen];
    uint8_t n = ZigbeeAps::buildAckFrame(ack, sizeof(ack), aps.srcEndpoint,
                                         aps.clusterId, aps.profileId,
                                         aps.dstEndpoint, aps.counter);
    uint16_t nh = routing.nextHopFor(nwk.srcShort);
    if (nh == ZigbeeRouting::kNoNextHop) nh = mac.srcShort;
    radio.sendNwkData(network.info().panId, nh, network.info().nwkAddress,
                      nwk.srcShort, network.info().nwkAddress, ack, n,
                      ZigbeeNwk::kDefaultRadius, true);
  }

  if (!isNew) {
    ++apsDuplicates;
    Serial.print("APS dup cnt="); Serial.print(aps.counter);
    Serial.print(" from 0x"); printHex16(nwk.srcShort);
    Serial.println(" (re-acked, not reprocessed)");
    return;
  }

  Serial.print("APS data from 0x"); printHex16(nwk.srcShort);
  Serial.print(" cnt="); Serial.print(aps.counter);
  Serial.print(" \"");
  for (uint8_t i = 0; i < aps.payloadLen; ++i) Serial.print((char)aps.payload[i]);
  Serial.println("\" -> ACK");
}

// Sender side: an APS ACK clears the matching pending entry.
void onApsAck(const MacDataFrame& mac, const NwkDataFrame& nwk,
              const ApsAckFrame& ack, int8_t rssi, uint8_t lqi) {
  (void)mac; (void)nwk; (void)rssi; (void)lqi;
  // Matches any pending entry by APS counter + endpoint: both the demo APS
  // data plane (cluster APS_CLUSTER, endpoint APS_ENDPOINT) and the acked
  // Mgmt_Lqi_rsp (cluster 0x8031, ZDO endpoint 0) clear here.
  if (apsRetx.onAck(ack.counter, ack.dstEndpoint)) {
    Serial.print("APS ACK ok cnt="); Serial.print(ack.counter);
    Serial.print(" (delivered "); Serial.print(apsRetx.stats().delivered);
    Serial.print("/"); Serial.print(apsRetx.stats().queued);
    Serial.println(")");
  }
}

void serviceRouteDiscovery() {
  // Only the end device originates the demo route to the coordinator. Routers
  // forward/relay but don't originate (keeps the bench output readable).
  if (!ROLE_END || !network.isJoined()) return;
#if NIUS_ZIGBEE_SECURE_JOIN
  if (!keyInstalled) return;  // no secured traffic until the network key arrives
#endif
  routing.expire();

  uint16_t target = ZB_NWK_ADDR_COORDINATOR;
  if (!routing.routeIsActive(target)) {
    if ((int32_t)(millis() - nextDiscoveryAt) >= 0) {
      nextDiscoveryAt = millis() + 8000;
      pendingRreqId = routing.originateDiscovery(target);
      uint8_t payload[16];
      uint8_t n = ZigbeeNwk::buildRouteRequestPayload(payload, sizeof(payload),
                                                      pendingRreqId, target);
      bool ok = n > 0 && radio.sendNwkCommand(
          network.info().panId, ZigbeeMac::kBroadcastShort,
          network.info().nwkAddress, ZigbeeNwk::kBroadcastAllRouters,
          network.info().nwkAddress, NWK_CMD_ROUTE_REQUEST, payload, n,
          ZigbeeNwk::kDefaultRadius, false);
      Serial.print("RREQ id="); Serial.print(pendingRreqId);
      Serial.print(" for 0x"); printHex16(target);
      Serial.println(ok ? " broadcast" : " FAILED");
    }
    return;
  }

  // Route is ACTIVE: send a new acked APS frame every 10 s and service any
  // retransmits that have come due in between.
  if ((int32_t)(millis() - nextApsSendAt) >= 0) {
    nextApsSendAt = millis() + 10000;
    char msg[16];
    int len = snprintf(msg, sizeof(msg), "aps %lu", (unsigned long)++apsSeq);
    uint8_t apdu[ZigbeeAps::kMaxFrame];
    uint8_t n = ZigbeeAps::buildDataFrame(apdu, sizeof(apdu), APS_ENDPOINT,
                                          APS_CLUSTER, APS_PROFILE,
                                          APS_ENDPOINT, apsCounter,
                                          (const uint8_t*)msg, (uint8_t)len,
                                          /*ackRequest=*/true);
    bool ok = sendApsRouted(target, apdu, n);
    apsRetx.add(target, apsCounter, APS_ENDPOINT, apdu, n, /*maxRetries=*/3,
                /*intervalMs=*/1500, millis());
    Serial.print("APS send seq="); Serial.print(apsSeq);
    Serial.print(" cnt="); Serial.print(apsCounter);
    Serial.print(" -> 0x"); printHex16(target);
    Serial.println(ok ? "" : " (tx FAILED)");
    ++apsCounter;
  }

  // Periodically map the network: ask the coordinator for its neighbor table
  // (standard Mgmt_Lqi_req), re-sending until the rsp comes back.
  if (!mgmtLqiPending && (int32_t)(millis() - nextMgmtLqiCycleAt) >= 0) {
    mgmtLqiPending = true;
    mgmtLqiTries = 0;
    mgmtLqiSeq = zdoSequence++;
    mgmtLqiRetryAt = millis();
    nextMgmtLqiCycleAt = millis() + 30000;
  }
  if (mgmtLqiPending && (int32_t)(millis() - mgmtLqiRetryAt) >= 0) {
    if (mgmtLqiTries >= 6) {
      mgmtLqiPending = false;
      Serial.println("Mgmt_Lqi: no rsp after 6 tries, giving up");
    } else {
      ++mgmtLqiTries;
      // Retry slowly (2.5 s): the responder answers once and retransmits its
      // rsp on its own budget, so a fast req retry only adds channel traffic.
      mgmtLqiRetryAt = millis() + 2500;
      uint8_t payload[2];
      uint8_t n = ZigbeeZdo::buildMgmtLqiRequest(payload, sizeof(payload),
                                                 mgmtLqiSeq, 0);
      uint16_t nh = routing.nextHopFor(target);
      if (nh == ZigbeeRouting::kNoNextHop) nh = target;
      radio.sendZdoCommand(network.info().panId, nh, network.info().nwkAddress,
                           target, network.info().nwkAddress, ZDO_MGMT_LQI_REQ,
                           payload, n, ZigbeeNwk::kDefaultRadius,
                           /*macAck=*/true);  // per-hop MAC ack for the req
      Serial.print("Mgmt_Lqi_req try "); Serial.print(mgmtLqiTries);
      Serial.print(" -> 0x"); printHex16(target); Serial.println();
    }
  }
}

// Re-send any APS frame whose ACK is overdue. Runs for ALL roles: the end
// device retransmits its data-plane frames to the coordinator, and the
// coordinator (and any responder) retransmits the acked Mgmt_Lqi_rsp until the
// requester ACKs it. Pending entries are matched/cleared by onApsAck.
void serviceApsRetx() {
  ApsPending* due = apsRetx.due(millis());
  if (!due) return;
  sendApsRouted(due->dstShort, due->apdu, due->apduLen);
  apsRetx.markAttempted(due, millis());
  Serial.print("APS retransmit cnt="); Serial.print(due->apsCounter);
  Serial.print(" -> 0x"); printHex16(due->dstShort); Serial.println();
}

void serviceLinkStatusAndAging() {
  if (!network.isJoined() || !IS_PARENT_CAPABLE) return;
  if (!network.linkStatusDue()) return;
  network.markLinkStatusSent();

  NwkLinkStatusEntry entries[ZigbeeNetwork::kMaxLinkStatusEntries];
  uint8_t n = network.collectLinkStatusEntries(entries, ZigbeeNetwork::kMaxLinkStatusEntries);
  uint8_t payload[1 + 3 * ZigbeeNetwork::kMaxLinkStatusEntries];
  uint8_t len = ZigbeeNwk::buildLinkStatusPayload(payload, sizeof(payload), entries, n);
  if (len > 0)
    radio.sendNwkCommand(network.info().panId, ZigbeeMac::kBroadcastShort,
                         network.info().nwkAddress, ZigbeeNwk::kBroadcastAllRouters,
                         network.info().nwkAddress, NWK_CMD_LINK_STATUS,
                         payload, len, 1, false);

  ZigbeeNetwork::AgingResult aged = network.ageNeighbors();
  if (aged.removed > 0) {
    Serial.print("aged out "); Serial.print(aged.removed);
    Serial.println(" stale router neighbor(s)");
  }
  if (aged.parentLost) {
    Serial.println("parent lost - rejoining");
    if (network.rejoinParent()) { announced = false; nextActionAt = millis(); }
  }
}

// Answer a Mgmt_Lqi_req with our neighbor table (a standard Zigbee
// network-management query - this is what a coordinator/mapping tool uses to
// walk the mesh).
// Record a Mgmt_Lqi_req for deferred answering from loop().
void replyMgmtLqi(const NwkDataFrame& nwk, const ApsDataFrame& aps) {
  ZdoMgmtRequest req;
  if (!ZigbeeZdo::parseMgmtRequest(aps.payload, aps.payloadLen, req)) return;
  pendingLqiRsp = true;
  lqiRspTo = nwk.srcShort;
  lqiRspSeq = req.sequence;
  lqiRspStart = req.startIndex;
}

// Build and send the pending Mgmt_Lqi_rsp from the neighbor table. Called
// from loop() when the radio is idle, not from the RX callback.
void sendPendingMgmtLqiRsp() {
  if (!pendingLqiRsp) return;
  pendingLqiRsp = false;

  // A requester re-sends its Mgmt_Lqi_req every couple of seconds until it gets
  // the answer. If we still have an unacked rsp in flight to it, DON'T queue a
  // second one - the existing entry's retransmits already cover delivery.
  // Queueing one per request retry is what caused a retransmit storm that
  // congested the half-duplex channel so badly the (longer) rsp never won air.
  if (apsRetx.hasPendingFor(lqiRspTo, ZigbeeZdo::kEndpoint)) return;

  ZdoNeighborListEntry entries[4];
  uint8_t total = 0, listCount = 0;
  for (uint8_t i = 0; i < neighbors.capacity(); ++i) {
    const ZigbeeNeighbor* nb = neighbors.slot(i);
    if (!nb || !nb->used) continue;
    ++total;
    if (total <= lqiRspStart || listCount >= 4) continue;
    ZdoNeighborListEntry& e = entries[listCount++];
    e.extendedPanId = 0;  // not tracked per-neighbor here
    e.extendedAddress = nb->ieeeAddress;
    e.nwkAddress = nb->nwkAddress;
    e.deviceType = nb->deviceType;
    e.rxOnWhenIdle = nb->rxOnWhenIdle ? 1 : 0;
    e.relationship = nb->relationship;
    e.permitJoining = nb->permitJoining ? 1 : 0;
    e.depth = nb->depth;
    e.lqi = nb->lqi;
  }

  uint8_t payload[ZigbeeZdo::kMaxPayload];
  uint8_t n = ZigbeeZdo::buildMgmtLqiResponse(payload, sizeof(payload),
                                              lqiRspSeq, ZDO_STATUS_SUCCESS,
                                              total, lqiRspStart, entries,
                                              listCount);
  if (n == 0) return;

  // The Mgmt_Lqi_rsp is a long ZDO frame (~60 B encrypted). A single-shot send
  // over the multi-hop A->B->C path was lost every time, while short APS
  // data/ack with end-to-end retransmit got ~95%. So carry the rsp the same
  // way: wrap it as an APS data frame with the APS ack-request bit set, send it
  // over the route, and register it for retransmit. The requester ACKs it (see
  // onZdoFrame), which clears the pending entry; until then loop()'s
  // serviceApsRetx() re-sends it. Use the ZDO endpoint/profile so it still
  // dispatches to onZdoFrame at the far end.
  uint8_t apdu[ZigbeeAps::kMaxFrame];
  uint8_t an = ZigbeeAps::buildDataFrame(
      apdu, sizeof(apdu), ZigbeeZdo::kEndpoint, ZDO_MGMT_LQI_RSP,
      ZigbeeAps::kProfileZigbeeDevice, ZigbeeZdo::kEndpoint, apsCounter, payload,
      n, /*ackRequest=*/true);
  if (an == 0) return;
  bool ok = sendApsRouted(lqiRspTo, apdu, an);
  apsRetx.add(lqiRspTo, apsCounter, ZigbeeZdo::kEndpoint, apdu, an,
              /*maxRetries=*/3, /*intervalMs=*/2000, millis());
  Serial.print("Mgmt_Lqi rsp -> 0x"); printHex16(lqiRspTo);
  Serial.print(" ("); Serial.print(listCount); Serial.print("/");
  Serial.print(total); Serial.print(" nb, acked cnt=");
  Serial.print(apsCounter); Serial.print(")");
  Serial.println(ok ? "" : " TX-FAIL");
  ++apsCounter;
}

// Print the neighbor table a Mgmt_Lqi_rsp brought back.
void printMgmtLqiRsp(const NwkDataFrame& nwk, const ApsDataFrame& aps) {
  ZdoMgmtLqiResponse rsp;
  if (!ZigbeeZdo::parseMgmtLqiResponse(aps.payload, aps.payloadLen, rsp)) return;
  if (rsp.sequence == mgmtLqiSeq) mgmtLqiPending = false;  // the rsp is the ack
  Serial.print("Mgmt_Lqi_rsp from 0x"); printHex16(nwk.srcShort);
  Serial.print(": "); Serial.print(rsp.neighborTableEntries);
  Serial.println(" neighbor(s) total");
  for (uint8_t i = 0; i < rsp.listCount; ++i) {
    ZdoNeighborListEntry e;
    if (!ZigbeeZdo::getNeighborListEntry(rsp, i, e)) break;
    Serial.print("  0x"); printHex16(e.nwkAddress);
    Serial.print(" type="); Serial.print(e.deviceType);
    Serial.print(" rel="); Serial.print(e.relationship);
    Serial.print(" depth="); Serial.print(e.depth);
    Serial.print(" lqi="); Serial.println(e.lqi);
  }
}

void onZdoFrame(const MacDataFrame& mac, const NwkDataFrame& nwk,
                const ApsDataFrame& aps, int8_t rssi, uint8_t lqi) {
  (void)rssi;

  // Device_annce is a broadcast every parent records. The Mgmt_* unicasts are
  // only acted on by their addressed node - otherwise a relay (which also
  // sees the frame as it forwards) would answer a request meant for someone
  // else, as an intermediate router was observed doing.
  bool forUs = (nwk.dstShort == network.info().nwkAddress);

  if (aps.clusterId == ZDO_DEVICE_ANNCE && IS_PARENT_CAPABLE) {
    ZdoDeviceAnnounce announce;
    if (!ZigbeeZdo::parseDeviceAnnounce(aps.payload, aps.payloadLen, announce)) return;
    neighbors.upsert(announce.nwkAddress, announce.ieeeAddress, ZB_DEVICE_ROUTER,
                     ZB_REL_CHILD, network.info().depth + 1, lqi & 0x7F, true, false);
    Serial.print("ZDO Device_annce nwk=0x"); printHex16(announce.nwkAddress);
    Serial.print(" ieee=0x"); printHex64(announce.ieeeAddress);
    Serial.print(" src=0x"); printHex16(nwk.srcShort);
    Serial.println();
#if NIUS_ZIGBEE_SECURE_JOIN
    // A secured Device_annce from the device we just keyed confirms it
    // installed the network key (it could not have secured this frame
    // otherwise): the key transport is complete.
    if (pendingKeyXport && announce.nwkAddress == keyXportAddr) {
      pendingKeyXport = false;
      Serial.print("secure join COMPLETE: 0x"); printHex16(keyXportAddr);
      Serial.println(" keyed + announced (secured)");
    }
#endif
  } else if (aps.clusterId == ZDO_MGMT_LQI_REQ && forUs) {
    replyMgmtLqi(nwk, aps);
  } else if (aps.clusterId == ZDO_MGMT_LQI_RSP && forUs) {
    printMgmtLqiRsp(nwk, aps);
    // The rsp is sent acked + retransmitted (it is too long to survive a
    // single-shot multi-hop send). Return an APS ACK over the reverse route so
    // the responder stops retransmitting. Duplicates (a retransmit that beat
    // our ACK back) are harmless here: parse/print are idempotent.
    if (aps.ackRequest) {
      uint8_t ack[ZigbeeAps::kBaseHeaderLen];
      uint8_t an = ZigbeeAps::buildAckFrame(ack, sizeof(ack), aps.srcEndpoint,
                                            aps.clusterId, aps.profileId,
                                            aps.dstEndpoint, aps.counter);
      uint16_t nh = routing.nextHopFor(nwk.srcShort);
      if (nh == ZigbeeRouting::kNoNextHop) nh = mac.srcShort;
      radio.sendNwkData(network.info().panId, nh, network.info().nwkAddress,
                        nwk.srcShort, network.info().nwkAddress, ack, an,
                        ZigbeeNwk::kDefaultRadius, true);
    }
  }
}

void saveState() {
  ZigbeePersistentState s;
  s.panId = network.info().panId;
  s.extendedPanId = network.info().extendedPanId;
  s.channel = network.info().channel;
  s.nwkAddress = network.info().nwkAddress;
  s.parentAddress = network.info().parentAddress;
  s.depth = network.info().depth;
  s.deviceType = network.info().deviceType;
  s.ieeeAddress = THIS_IEEE;
  s.outgoingFrameCounter = radio.securityFrameCounter() + COUNTER_MARGIN;
  s.keySequence = security.keySequence();

  uint8_t blob[ZigbeePersistence::kBlobSize];
  if (ZigbeePersistence::serialize(s, blob, sizeof(blob)) == 0) return;
  for (uint8_t i = 0; i < sizeof(blob); ++i) EEPROM.write(i, blob[i]);
  EEPROM.commit();
}

// Read the saved blob; returns true and fills `s` when valid and ours.
bool loadState(ZigbeePersistentState& s) {
#if defined(NIUS_ZIGBEE_IGNORE_SAVED) && NIUS_ZIGBEE_IGNORE_SAVED
  // Force a fresh scan/join, ignoring any persisted identity. Used to bring the
  // whole bench up from a consistent state when boards have been reflashed
  // separately and their saved addresses/routes no longer agree.
  (void)s;
  return false;
#else
  uint8_t blob[ZigbeePersistence::kBlobSize];
  for (uint8_t i = 0; i < sizeof(blob); ++i) blob[i] = EEPROM.read(i);
  if (!ZigbeePersistence::deserialize(blob, sizeof(blob), s)) return false;
  return s.ieeeAddress == THIS_IEEE;
#endif
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) {}
  EEPROM.begin(64);

  network.attachNeighborTable(neighbors);
  routing.attachRouteTable(routes);
#if NIUS_ZIGBEE_SECURE_JOIN
  // Only the Trust Center starts with the network key; a joiner receives it via
  // the APS Transport-Key after association (see serviceKeyTransport / onApsData).
  if (IS_COORDINATOR) security.setNetworkKey(NETWORK_KEY);
  // Derive the key-transport key once at startup (both TC and joiner).
  ZigbeeApsSecurity::deriveKeyTransportKey(ZigbeeApsKey::defaultTcLinkKey(), gKtk);
#else
  security.setNetworkKey(NETWORK_KEY);  // pre-shared network key
#endif

  if (!radio.begin(IS_COORDINATOR ? COORD_CHANNEL : SCAN_CHANNELS[0])) {
    Serial.println("CC2530 not found - check wiring/firmware.");
    while (true) delay(1000);
  }

  radio.onMacCommandReceive(onMacCommand);
  radio.onNwkCommandReceive(onNwkCommand);
  radio.onNwkReceive(onNwkData);
  radio.onZdoReceive(onZdoFrame);
  radio.onApsReceive(onApsData);
  radio.onApsAckReceive(onApsAck);
  radio.attachSecurity(security, THIS_IEEE);
  apsRetx.begin(apsPendingStorage, 4);
  apsDupe.begin(apsDupeStorage, 6);

#if NIUS_ZIGBEE_GROUPCAST
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  groups.add(DEMO_GROUP);
  Serial.print("GROUPCAST: joined group 0x"); printHex16(DEMO_GROUP);
  Serial.println();
#endif

  Serial.print("Node "); Serial.print(roleName());
  Serial.print(" ieee=0x"); printHex64(THIS_IEEE); Serial.println();

  ZigbeePersistentState saved;
  bool haveSaved = loadState(saved);

  if (IS_COORDINATOR) {
    network.beginCoordinator(PAN_ID, EXT_PAN_ID, COORD_CHANNEL);
    network.configureAddressPool(COORD_POOL_FIRST, COORD_POOL_LAST);
    network.permitJoining(0xFF);
    applyAddress(PAN_ID, ZB_NWK_ADDR_COORDINATOR);
    if (haveSaved) {  // identity is fixed; only the counter must not rewind
      radio.setSecurityFrameCounter(saved.outgoingFrameCounter);
      Serial.print("Coordinator up (counter restored to ");
      Serial.print(saved.outgoingFrameCounter); Serial.println(")");
    } else {
      Serial.print("Coordinator up. PAN=0x"); printHex16(PAN_ID);
      Serial.print(" ch="); Serial.println(COORD_CHANNEL);
    }
  } else if (haveSaved) {
    // Restore the joined identity - no scan / association needed.
    network.beginJoinedDevice(saved.deviceType, saved.panId,
                              saved.extendedPanId, saved.channel,
                              saved.nwkAddress, saved.parentAddress,
                              saved.depth);
    // Restore the default route to the coordinator via the parent - otherwise
    // the route table is empty after a reboot and unicast frames fall back to
    // sending direct (which the range-sim cuts), so nothing reaches the mesh.
    routes.upsert(ZB_NWK_ADDR_COORDINATOR, saved.parentAddress, ZB_ROUTE_ACTIVE);
    radio.setSecurityFrameCounter(saved.outgoingFrameCounter);
    radio.setChannel(saved.channel);
    applyAddress(saved.panId, saved.nwkAddress);
    radio.setPromiscuous(false);
    if (ROLE_ROUTER) becomeParent();
    announced = true;  // already announced in a previous life
    Serial.print("RESTORED from flash: addr=0x"); printHex16(saved.nwkAddress);
    Serial.print(" parent=0x"); printHex16(saved.parentAddress);
    Serial.print(" counter="); Serial.print(saved.outgoingFrameCounter);
    Serial.println(" - skipping scan");
  } else {
    radio.onBeaconReceive(onBeacon);
    Serial.println("Joiner up. No saved state - will scan.");
    runActiveScan();
  }
}

void loop() {
  radio.poll();
  serviceLinkStatusAndAging();
  serviceRouteDiscovery();
  sendPendingMgmtLqiRsp();   // answer a recorded Mgmt_Lqi_req while radio idle
  serviceApsRetx();          // retransmit any acked frame whose ACK is overdue
#if NIUS_ZIGBEE_SECURE_JOIN
  serviceKeyTransport();     // TC: deliver the network key to a new joiner
#endif
#if NIUS_ZIGBEE_GROUPCAST
  serviceGroupCast();        // TC: broadcast a group On/Off toggle periodically
#endif
  if (ROLE_ROUTER && network.isJoined()) becomeParent();

  // Persist network state + frame counter periodically so a reboot restores
  // the identity and never rewinds the counter.
  if ((network.isJoined() || IS_COORDINATOR) &&
      (int32_t)(millis() - nextSaveAt) >= 0) {
    nextSaveAt = millis() + SAVE_PERIOD_MS;
    saveState();
  }

  if ((int32_t)(millis() - nextStatus) >= 0) {
    nextStatus = millis() + 10000;
    Serial.print("status "); Serial.print(roleName());
    Serial.print(" addr=0x"); printHex16(network.info().nwkAddress);
    Serial.print(" joined="); Serial.print(network.isJoined() ? "y" : "n");
    Serial.print(" children="); Serial.print(neighbors.count());
    Serial.print(" fwd="); Serial.print(forwarded);
    Serial.print(" sec[tx="); Serial.print(security.stats().secured);
    Serial.print(" rx="); Serial.print(security.stats().opened);
    Serial.print(" mic="); Serial.print(security.stats().micFailures);
    Serial.print(" rpl="); Serial.print(security.stats().replays);
    Serial.print("]");
    if (IS_PARENT_CAPABLE) {
      Serial.print(" dup="); Serial.print(apsDuplicates);
    }
    {
      const ApsRetransmitStats& a = apsRetx.stats();
      Serial.print(" aps[q="); Serial.print(a.queued);
      Serial.print(" ok="); Serial.print(a.delivered);
      Serial.print(" rtx="); Serial.print(a.retransmits);
      Serial.print(" drop="); Serial.print(a.dropped);
      Serial.print("]");
    }
    Serial.println();
  }

  if (IS_COORDINATOR) return;

  if ((network.info().state == ZB_NWK_STATE_IDLE || network.isScanning()) &&
      (int32_t)(millis() - nextActionAt) >= 0) {
    runActiveScan();
    return;
  }

  if (network.isJoining() && (int32_t)(millis() - nextActionAt) >= 0) {
    if (network.joinAttempts() >= MAX_ASSOC_ATTEMPTS) {
      Serial.println("association keeps failing - rescanning");
      network.resetJoinAttempts();
      runActiveScan();
      return;
    }
    network.noteJoinAttempt();
    nextActionAt = millis() + 3000;
    bool ok = radio.sendAssociationRequest(
        chosenParent.panId, chosenParent.shortAddress, THIS_IEEE,
        JOINER_CAPABILITY, true);
    Serial.print("ASSOC req (attempt "); Serial.print(network.joinAttempts());
    Serial.println(ok ? ") sent" : ") FAILED");
  }

  if (network.isJoined() && !announced
#if NIUS_ZIGBEE_SECURE_JOIN
      // Defer the announce until the network key is installed - otherwise we
      // could not secure it, and it is the TC's confirmation of the key transfer.
      && keyInstalled
#endif
  ) {
    announced = true;
    delay(80);
    sendDeviceAnnounce();
  }
}
