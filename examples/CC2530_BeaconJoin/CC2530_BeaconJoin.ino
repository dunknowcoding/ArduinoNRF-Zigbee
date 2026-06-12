/*
  CC2530_BeaconJoin - full Zigbee-style network discovery + join demo.

  The joiner no longer needs to know the PAN, channel, or coordinator in
  advance: it active-scans a channel list with Beacon Requests, collects the
  Zigbee beacons that come back, picks the best parent (permit-join, capacity,
  stack profile, LQI, depth), MAC-associates with it, and finally broadcasts a
  ZDO Device_annce. If association keeps failing it falls back to a fresh scan.

  Once joined, both routers run the Zigbee neighbor-aging protocol: every 15 s
  they broadcast a NWK Link Status carrying their router neighbors with
  incoming/outgoing link costs, and neighbors that miss 3 consecutive periods
  are aged out of the table (a stale PARENT instead triggers a rejoin).

  The joiner then exercises AODV-style route discovery: it broadcasts a Route
  Request for the coordinator, the coordinator answers with a unicast Route
  Reply (recording the reverse route as the request passes), both ends install
  ACTIVE routes, and a routed "route ping"/"route pong" NWK data exchange
  proves packets flow over the discovered next hops in both directions.

  All NWK traffic (link status, route discovery, routed data, ZDO announce)
  is protected with Zigbee NWK security: AES-CCM* ENC-MIC-32 under a
  pre-shared network key, computed host-side on the nRF52840's hardware AES
  block. Frames failing the MIC or replaying an old counter are dropped
  (see the security stats in the status lines). MAC association itself stays
  unsecured, as in real networks before key transport - standard Trust
  Center key delivery is future work.

  Flash this sketch to two ArduinoNRF+CC2530 setups. Build one with
  NIUS_ZIGBEE_THIS_NODE=0x0001 for the coordinator and the other with
  NIUS_ZIGBEE_THIS_NODE=0x0002 for the joining device. Only the coordinator
  needs network parameters; the joiner discovers them over the air.
*/

#include <CC2530Radio.h>

#ifndef NIUS_ZIGBEE_PAN_ID
#define NIUS_ZIGBEE_PAN_ID 0x1A62
#endif

#ifndef NIUS_ZIGBEE_THIS_NODE
#define NIUS_ZIGBEE_THIS_NODE 0x0001
#endif

static const uint16_t PAN_ID = NIUS_ZIGBEE_PAN_ID;          // coordinator only
static const uint64_t EXT_PAN_ID = 0x1A62195E00000000ULL;   // coordinator only
static const uint8_t COORD_CHANNEL = 15;                    // coordinator only
static const uint16_t THIS_NODE = NIUS_ZIGBEE_THIS_NODE;
static const bool IS_COORDINATOR = (THIS_NODE == 0x0001);
static const uint64_t THIS_IEEE = 0x1A62195E00000000ULL | THIS_NODE;
static const uint8_t JOINER_CAPABILITY = 0x8A;  // allocate addr, rx-on, FFD

// Pre-shared Zigbee network key - make your own for anything real!
// Build with -DNIUS_ZIGBEE_WRONG_KEY=1 to demonstrate MIC rejection.
static const uint8_t NETWORK_KEY[16] = {
#ifdef NIUS_ZIGBEE_WRONG_KEY
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
#else
    0x1A, 0x62, 0x19, 0x5E, 0x4B, 0x3C, 0x2D, 0x1E,
    0x0F, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07,
#endif
};

// Channels the joiner probes, in order. Includes channels with no network so
// the scan demonstrably skips them.
static const uint8_t SCAN_CHANNELS[] = {11, 15, 20, 25};
static const uint8_t SCAN_CHANNEL_COUNT =
    sizeof(SCAN_CHANNELS) / sizeof(SCAN_CHANNELS[0]);
static const uint16_t SCAN_DWELL_MS = 250;  // listen time per channel
static const uint8_t MAX_ASSOC_ATTEMPTS = 4;

CC2530Radio radio;
ZigbeeNeighbor neighborStorage[8];
ZigbeeNeighborTable neighbors(neighborStorage, 8);
ZigbeeNetwork network;
ZigbeeRoute routeStorage[8];
ZigbeeRouteTable routes(routeStorage, 8);
ZigbeeRouting routing;
ZigbeeSecurity security;
uint8_t pendingRreqId = 0;
uint32_t nextRoutePingAt = 0;
uint32_t routePings = 0;
uint32_t routePongs = 0;
uint32_t nextDiscoveryAt = 0;

ZigbeeParentCandidate chosenParent;
uint8_t scanChannel = 0;       // channel currently being probed
uint32_t nextActionAt = 0;
uint32_t nextStatus = 0;
bool announced = false;
uint8_t zdoSequence = 0;

void printHex8(uint8_t v) {
  if (v < 0x10) Serial.print('0');
  Serial.print(v, HEX);
}

void printHex16(uint16_t v) {
  if (v < 0x1000) Serial.print('0');
  if (v < 0x0100) Serial.print('0');
  if (v < 0x0010) Serial.print('0');
  Serial.print(v, HEX);
}

void printHex64(uint64_t v) {
  for (int i = 7; i >= 0; --i) printHex8((uint8_t)(v >> (i * 8)));
}

void ieeeToBytes(uint64_t ieee, uint8_t* out) {
  for (uint8_t i = 0; i < 8; ++i) out[i] = (uint8_t)(ieee >> (8 * i));
}

bool applyAddress(uint16_t panId, uint16_t shortAddress) {
  uint8_t ieeeBytes[8];
  ieeeToBytes(THIS_IEEE, ieeeBytes);
  bool ok = radio.setAddress(panId, shortAddress, ieeeBytes);
  ok = ok && radio.configureMac(
      CC2530Radio::kMacFilter | CC2530Radio::kMacAutoAck |
      CC2530Radio::kMacCcaTx, 3);
  return ok;
}

// ---------------------------------------------------------------- coordinator

void sendOurBeacon() {
  uint8_t payload[ZigbeeNwk::kBeaconPayloadLen];
  uint8_t n = ZigbeeNwk::buildBeaconPayload(
      payload, sizeof(payload), network.info().extendedPanId,
      network.info().depth,
      /*routerCapacity=*/network.isJoiningPermitted(),
      /*endDeviceCapacity=*/network.isJoiningPermitted(),
      network.info().updateId);
  bool ok = n > 0 && radio.sendBeacon(network.info().panId,
                                      network.info().nwkAddress,
                                      network.isCoordinator(),
                                      network.isJoiningPermitted(),
                                      payload, n);
  Serial.println(ok ? "beacon sent" : "beacon FAILED");
}

void onMacCommand(const MacCommandFrame& frame, int8_t rssi, uint8_t lqi) {
  if (IS_COORDINATOR && frame.commandId == MAC_CMD_BEACON_REQUEST) {
    sendOurBeacon();
    return;
  }

  if (IS_COORDINATOR &&
      frame.commandId == MAC_CMD_ASSOCIATION_REQUEST &&
      frame.dstPanId == network.info().panId &&
      frame.dstShort == network.info().nwkAddress) {
    MacAssociationRequest request;
    if (!ZigbeeMac::parseAssociationRequest(frame, request)) return;

    ZigbeeAssociationDecision decision =
        network.handleAssociationRequest(frame.srcIeee, request, lqi & 0x7F);
    if (decision.accepted) {
      // A (re)joining device restarts its outgoing frame counter; drop our
      // replay history so its secured frames are accepted again.
      security.resetReplayTable();
    }
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

  if (!IS_COORDINATOR &&
      frame.commandId == MAC_CMD_ASSOCIATION_RESPONSE &&
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
      security.resetReplayTable();  // fresh network state: accept counters anew
      network.noteParent(chosenParent.shortAddress, 0,
                         chosenParent.depth == 0 ? ZB_DEVICE_COORDINATOR
                                                 : ZB_DEVICE_ROUTER,
                         chosenParent.depth, chosenParent.lqi, true);
      bool ok = applyAddress(chosenParent.panId, response.shortAddress);
      Serial.print("JOINED pan=0x");
      printHex16(chosenParent.panId);
      Serial.print(" ch=");
      Serial.print(chosenParent.channel);
      Serial.print(" addr=0x");
      printHex16(response.shortAddress);
      Serial.println(ok ? "" : " (MAC configure FAILED)");
      announced = false;
    }
  }
}

// --------------------------------------------------------------------- joiner

void onBeacon(const MacBeaconFrame& beacon, int8_t rssi, uint8_t lqi) {
  if (IS_COORDINATOR || !network.isScanning()) return;
  NwkBeaconPayload payload;
  if (!ZigbeeNwk::parseBeaconPayload(beacon.payload, beacon.payloadLen,
                                     payload)) {
    return;
  }
  if (network.noteBeacon(scanChannel, beacon, payload, rssi, lqi & 0x7F)) {
    Serial.print("  beacon ch=");
    Serial.print(scanChannel);
    Serial.print(" pan=0x");
    printHex16(beacon.srcPanId);
    Serial.print(" from=0x");
    printHex16(beacon.srcShort);
    Serial.print(" depth=");
    Serial.print(payload.deviceDepth);
    Serial.print(" permit=");
    Serial.print(beacon.associationPermit ? "yes" : "no");
    Serial.print(" lqi=");
    Serial.println(lqi & 0x7F);
  }
}

void runActiveScan() {
  Serial.println("scanning...");
  network.beginScan(ZB_DEVICE_ROUTER);
  radio.setPromiscuous(true);  // accept beacons from any PAN while scanning

  for (uint8_t i = 0; i < SCAN_CHANNEL_COUNT; ++i) {
    scanChannel = SCAN_CHANNELS[i];
    if (!radio.setChannel(scanChannel)) continue;
    radio.sendBeaconRequest();
    uint32_t until = millis() + SCAN_DWELL_MS;
    while ((int32_t)(millis() - until) < 0) {
      radio.poll();
    }
  }

  const ZigbeeParentCandidate* parent = network.selectParent();
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

  Serial.print("parent chosen: pan=0x");
  printHex16(chosenParent.panId);
  Serial.print(" addr=0x");
  printHex16(chosenParent.shortAddress);
  Serial.print(" ch=");
  Serial.print(chosenParent.channel);
  Serial.print(" lqi=");
  Serial.println(chosenParent.lqi);
  nextActionAt = millis();  // associate immediately
}

void sendDeviceAnnounce() {
  uint8_t payload[ZigbeeZdo::kMaxPayload];
  uint8_t n = ZigbeeZdo::buildDeviceAnnounce(
      payload, sizeof(payload), zdoSequence++, network.info().nwkAddress,
      THIS_IEEE, JOINER_CAPABILITY);
  bool ok = n > 0 && radio.sendZdoCommand(
      network.info().panId, ZigbeeMac::kBroadcastShort,
      network.info().nwkAddress, ZigbeeNwk::kBroadcastRxOnWhenIdle,
      network.info().nwkAddress, ZDO_DEVICE_ANNCE, payload, n,
      ZigbeeNwk::kDefaultRadius, false);
  Serial.println(ok ? "ZDO Device_annce sent" : "ZDO Device_annce FAILED");
}

// ------------------------------------------------- link status / aging

void onNwkCommand(const MacDataFrame& mac, const NwkCommandFrame& nwk,
                  int8_t rssi, uint8_t lqi) {
  (void)rssi;
  uint8_t linkCost = ZigbeeNetwork::costFromLqi(lqi & 0x7F);

  if (nwk.commandId == NWK_CMD_LINK_STATUS) {
    NwkLinkStatusCommand command;
    if (!ZigbeeNwk::parseLinkStatusPayload(nwk.payload, nwk.payloadLen,
                                           command)) {
      return;
    }
    if (network.handleLinkStatus(nwk.srcShort, command, lqi & 0x7F)) {
      Serial.print("LINK STATUS from 0x");
      printHex16(nwk.srcShort);
      Serial.print(" entries=");
      Serial.println(command.entryCount);
    }
    return;
  }

  if (nwk.commandId == NWK_CMD_ROUTE_REQUEST && network.isJoined()) {
    NwkRouteRequestCommand rreq;
    if (!ZigbeeNwk::parseRouteRequestPayload(nwk.payload, nwk.payloadLen,
                                             rreq)) {
      return;
    }
    ZigbeeRreqDecision d = routing.handleRouteRequest(
        network.info().nwkAddress, network.isCoordinator() || network.isRouter(),
        mac.srcShort, nwk.srcShort, rreq, linkCost);
    if (d.duplicate) return;

    Serial.print("RREQ id=");
    Serial.print(rreq.routeRequestId);
    Serial.print(" for 0x");
    printHex16(rreq.destination);
    Serial.print(" from 0x");
    printHex16(nwk.srcShort);
    Serial.print(" via 0x");
    printHex16(mac.srcShort);

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
      if (n > 0) {
        radio.sendNwkCommand(network.info().panId, ZigbeeMac::kBroadcastShort,
                             network.info().nwkAddress,
                             ZigbeeNwk::kBroadcastAllRouters, nwk.srcShort,
                             NWK_CMD_ROUTE_REQUEST, payload, n,
                             nwk.radius > 1 ? nwk.radius - 1 : 1, false);
      }
      Serial.println(" -> rebroadcast");
    } else {
      Serial.println(" (recorded)");
    }
    return;
  }

  if (nwk.commandId == NWK_CMD_ROUTE_REPLY && network.isJoined()) {
    NwkRouteReplyCommand rrep;
    if (!ZigbeeNwk::parseRouteReplyPayload(nwk.payload, nwk.payloadLen,
                                           rrep)) {
      return;
    }
    ZigbeeRrepDecision d = routing.handleRouteReply(
        network.info().nwkAddress, mac.srcShort, rrep, linkCost);
    if (d.routeInstalled) {
      Serial.print("RREP id=");
      Serial.print(rrep.routeRequestId);
      Serial.print(": route to 0x");
      printHex16(rrep.responder);
      Serial.print(" via 0x");
      printHex16(mac.srcShort);
      Serial.println(" ACTIVE");
    } else if (d.forward) {
      uint8_t payload[16];
      uint8_t n = ZigbeeNwk::buildRouteReplyPayload(
          payload, sizeof(payload), rrep.routeRequestId, rrep.originator,
          rrep.responder, d.pathCost);
      if (n > 0) {
        radio.sendNwkCommand(network.info().panId, d.forwardTo,
                             network.info().nwkAddress, rrep.originator,
                             nwk.srcShort, NWK_CMD_ROUTE_REPLY, payload, n,
                             ZigbeeNwk::kDefaultRadius, true);
      }
      Serial.println("RREP forwarded");
    }
    return;
  }
}

// ------------------------------------------------- routed data ping/pong

void onNwkData(const MacDataFrame& mac, const NwkDataFrame& nwk, int8_t rssi,
               uint8_t lqi) {
  (void)mac;
  (void)rssi;
  (void)lqi;
  if (!network.isJoined() || nwk.dstShort != network.info().nwkAddress) return;

  if (nwk.payloadLen == 10 && memcmp(nwk.payload, "route ping", 10) == 0) {
    Serial.print("ROUTED ping from 0x");
    printHex16(nwk.srcShort);
    uint16_t nextHop = routing.nextHopFor(nwk.srcShort);
    if (nextHop == ZigbeeRouting::kNoNextHop) nextHop = nwk.srcShort;
    bool ok = radio.sendNwkData(network.info().panId, nextHop,
                                network.info().nwkAddress, nwk.srcShort,
                                network.info().nwkAddress,
                                (const uint8_t*)"route pong", 10,
                                ZigbeeNwk::kDefaultRadius, true);
    Serial.println(ok ? " -> pong via route" : " -> pong FAILED");
    return;
  }

  if (nwk.payloadLen == 10 && memcmp(nwk.payload, "route pong", 10) == 0) {
    ++routePongs;
    Serial.print("ROUTED pong from 0x");
    printHex16(nwk.srcShort);
    Serial.print(" - round trip ");
    Serial.print(routePongs);
    Serial.print("/");
    Serial.println(routePings);
  }
}

void serviceRouteDiscovery() {
  if (IS_COORDINATOR || !network.isJoined()) return;

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
      Serial.print("RREQ id=");
      Serial.print(pendingRreqId);
      Serial.print(" for 0x");
      printHex16(target);
      Serial.println(ok ? " broadcast" : " FAILED");
    }
    return;
  }

  // Route is ACTIVE: ping over it every 10 s (occasional MAC-level
  // collisions self-heal on the next round).
  if ((int32_t)(millis() - nextRoutePingAt) >= 0) {
    nextRoutePingAt = millis() + 10000;
    ++routePings;
    uint16_t nextHop = routing.nextHopFor(target);
    bool ok = radio.sendNwkData(network.info().panId, nextHop,
                                network.info().nwkAddress, target,
                                network.info().nwkAddress,
                                (const uint8_t*)"route ping", 10,
                                ZigbeeNwk::kDefaultRadius, true);
    Serial.print("route ping ");
    Serial.print(routePings);
    Serial.print(" -> 0x");
    printHex16(target);
    Serial.print(" via 0x");
    printHex16(nextHop);
    Serial.println(ok ? "" : " (tx FAILED)");
  }
}

void serviceLinkStatusAndAging() {
  if (!network.isJoined()) return;
  if (!network.isCoordinator() && !network.isRouter()) return;

  if (network.linkStatusDue()) {
    network.markLinkStatusSent();
    NwkLinkStatusEntry entries[ZigbeeNetwork::kMaxLinkStatusEntries];
    uint8_t n = network.collectLinkStatusEntries(
        entries, ZigbeeNetwork::kMaxLinkStatusEntries);
    uint8_t payload[1 + 3 * ZigbeeNetwork::kMaxLinkStatusEntries];
    uint8_t len = ZigbeeNwk::buildLinkStatusPayload(payload, sizeof(payload),
                                                    entries, n);
    if (len > 0) {
      radio.sendNwkCommand(network.info().panId, ZigbeeMac::kBroadcastShort,
                           network.info().nwkAddress,
                           ZigbeeNwk::kBroadcastAllRouters,
                           network.info().nwkAddress, NWK_CMD_LINK_STATUS,
                           payload, len, /*radius=*/1, false);
    }

    ZigbeeNetwork::AgingResult aged = network.ageNeighbors();
    if (aged.removed > 0) {
      Serial.print("aged out ");
      Serial.print(aged.removed);
      Serial.println(" stale router neighbor(s)");
    }
    if (aged.parentLost) {
      Serial.println("parent lost - rejoining");
      if (network.rejoinParent()) {
        announced = false;
        nextActionAt = millis();
      }
    }

    // neighbor table dump with link costs
    for (uint8_t i = 0; i < neighbors.capacity(); ++i) {
      const ZigbeeNeighbor* nb = neighbors.slot(i);
      if (!nb || !nb->used) continue;
      Serial.print("  nb 0x");
      printHex16(nb->nwkAddress);
      Serial.print(" rel=");
      Serial.print(nb->relationship);
      Serial.print(" in=");
      Serial.print(nb->incomingCost);
      Serial.print(" out=");
      Serial.print(nb->outgoingCost);
      Serial.print(" age=");
      Serial.print((millis() - nb->lastSeenMs) / 1000);
      Serial.println("s");
    }
  }
}

void onZdoFrame(const MacDataFrame& mac, const NwkDataFrame& nwk,
                const ApsDataFrame& aps, int8_t rssi, uint8_t lqi) {
  (void)mac;
  (void)rssi;
  if (!IS_COORDINATOR || aps.clusterId != ZDO_DEVICE_ANNCE) return;
  ZdoDeviceAnnounce announce;
  if (!ZigbeeZdo::parseDeviceAnnounce(aps.payload, aps.payloadLen, announce)) {
    return;
  }
  neighbors.upsert(announce.nwkAddress, announce.ieeeAddress, ZB_DEVICE_ROUTER,
                   ZB_REL_CHILD, 1, lqi & 0x7F, true, false);
  Serial.print("ZDO Device_annce nwk=0x");
  printHex16(announce.nwkAddress);
  Serial.print(" ieee=0x");
  printHex64(announce.ieeeAddress);
  Serial.print(" src=0x");
  printHex16(nwk.srcShort);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {}

  network.attachNeighborTable(neighbors);

  if (!radio.begin(IS_COORDINATOR ? COORD_CHANNEL : SCAN_CHANNELS[0])) {
    Serial.println("CC2530 not found - check wiring/firmware.");
    while (true) delay(1000);
  }

  radio.onMacCommandReceive(onMacCommand);
  radio.onNwkCommandReceive(onNwkCommand);
  radio.onNwkReceive(onNwkData);
  routing.attachRouteTable(routes);
  security.setNetworkKey(NETWORK_KEY);
  radio.attachSecurity(security, THIS_IEEE);

  if (IS_COORDINATOR) {
    network.beginCoordinator(PAN_ID, EXT_PAN_ID, COORD_CHANNEL);
    network.configureAddressPool(0x0001, 0x00FE);
    network.permitJoining(0xFF);
    bool ok = applyAddress(PAN_ID, ZB_NWK_ADDR_COORDINATOR);
    radio.onZdoReceive(onZdoFrame);
    Serial.print("Coordinator up. PAN=0x");
    printHex16(PAN_ID);
    Serial.print(" ch=");
    Serial.print(COORD_CHANNEL);
    Serial.println(ok ? " answering beacon requests" : " MAC configure FAILED");
  } else {
    radio.onBeaconReceive(onBeacon);
    Serial.println("Joiner up. No network parameters - will scan.");
    runActiveScan();
  }
}

void loop() {
  radio.poll();
  serviceLinkStatusAndAging();
  serviceRouteDiscovery();

  if ((int32_t)(millis() - nextStatus) >= 0) {
    nextStatus = millis() + 10000;
    if (IS_COORDINATOR) {
      Serial.print("Coordinator status children=");
      Serial.print(neighbors.count());
      Serial.print(" permit=");
      Serial.print(network.joiningSecondsRemaining());
      Serial.print(" sec[tx=");
      Serial.print(security.stats().secured);
      Serial.print(" rx=");
      Serial.print(security.stats().opened);
      Serial.print(" mic=");
      Serial.print(security.stats().micFailures);
      Serial.print(" rpl=");
      Serial.print(security.stats().replays);
      Serial.println("]");
    } else {
      Serial.print("Joiner status state=");
      Serial.print(network.info().state);
      Serial.print(" joined=");
      Serial.print(network.isJoined() ? "yes" : "no");
      Serial.print(" addr=0x");
      printHex16(network.info().nwkAddress);
      Serial.print(" attempts=");
      Serial.println(network.joinAttempts());
    }
  }

  if (IS_COORDINATOR) return;

  // Joiner state machine: scan retry, association attempts with backoff,
  // re-scan after too many failures, Device_annce once joined.
  if (network.info().state == ZB_NWK_STATE_IDLE &&
      (int32_t)(millis() - nextActionAt) >= 0) {
    runActiveScan();
    return;
  }

  if (network.isScanning() && (int32_t)(millis() - nextActionAt) >= 0) {
    runActiveScan();  // previous scan found nothing joinable
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
    Serial.print("ASSOC req (attempt ");
    Serial.print(network.joinAttempts());
    Serial.println(ok ? ") sent" : ") FAILED");
  }

  if (network.isJoined() && !announced) {
    announced = true;
    delay(80);
    sendDeviceAnnounce();
  }
}
