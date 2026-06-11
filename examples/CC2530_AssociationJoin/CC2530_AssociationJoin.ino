/*
  CC2530_AssociationJoin - two-node MAC association + ZDO Device_annce demo.

  Flash this sketch to two ArduinoNRF+CC2530 setups. Build one with
  NIUS_ZIGBEE_THIS_NODE=0x0001 for the coordinator and the other with
  NIUS_ZIGBEE_THIS_NODE=0x0002 for the joining device.
*/

#include <CC2530Radio.h>

#ifndef NIUS_ZIGBEE_PAN_ID
#define NIUS_ZIGBEE_PAN_ID 0x1A62
#endif

#ifndef NIUS_ZIGBEE_THIS_NODE
#define NIUS_ZIGBEE_THIS_NODE 0x0001
#endif

static const uint16_t PAN_ID = NIUS_ZIGBEE_PAN_ID;
static const uint16_t THIS_NODE = NIUS_ZIGBEE_THIS_NODE;
static const bool IS_COORDINATOR = (THIS_NODE == 0x0001);
static const uint64_t THIS_IEEE = 0x1A62195E00000000ULL | THIS_NODE;
static const uint8_t CHANNEL = 15;
static const uint8_t JOINER_CAPABILITY = 0x8A;  // allocate addr, rx-on, FFD

CC2530Radio radio;
ZigbeeNeighbor neighborStorage[8];
ZigbeeNeighborTable neighbors(neighborStorage, 8);
ZigbeeNetwork network;
uint32_t nextJoinAttempt = 0;
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

bool applyAddress(uint16_t shortAddress) {
  uint8_t ieeeBytes[8];
  ieeeToBytes(THIS_IEEE, ieeeBytes);
  bool ok = radio.setAddress(PAN_ID, shortAddress, ieeeBytes);
  ok = ok && radio.configureMac(
      CC2530Radio::kMacFilter | CC2530Radio::kMacAutoAck |
      CC2530Radio::kMacCcaTx, 3);
  return ok;
}

void sendDeviceAnnounce() {
  uint8_t payload[ZigbeeZdo::kMaxPayload];
  uint8_t n = ZigbeeZdo::buildDeviceAnnounce(
      payload, sizeof(payload), zdoSequence++, network.info().nwkAddress,
      THIS_IEEE, JOINER_CAPABILITY);
  bool ok = n > 0 && radio.sendZdoCommand(
      PAN_ID, ZigbeeMac::kBroadcastShort, network.info().nwkAddress,
      ZigbeeNwk::kBroadcastRxOnWhenIdle, network.info().nwkAddress,
      ZDO_DEVICE_ANNCE, payload, n, ZigbeeNwk::kDefaultRadius, false);
  Serial.println(ok ? "ZDO Device_annce sent" : "ZDO Device_annce FAILED");
}

void onMacCommand(const MacCommandFrame& frame, int8_t rssi, uint8_t lqi) {
  if (IS_COORDINATOR &&
      frame.commandId == MAC_CMD_ASSOCIATION_REQUEST &&
      frame.dstPanId == PAN_ID &&
      frame.dstShort == ZB_NWK_ADDR_COORDINATOR) {
    MacAssociationRequest request;
    if (!ZigbeeMac::parseAssociationRequest(frame, request)) return;

    ZigbeeAssociationDecision decision =
        network.handleAssociationRequest(frame.srcIeee, request, lqi & 0x7F);
    bool ok = radio.sendAssociationResponse(
        PAN_ID, frame.srcIeee, ZB_NWK_ADDR_COORDINATOR,
        decision.assignedAddress, decision.status, true);

    Serial.print("ASSOC req ieee=0x");
    printHex64(frame.srcIeee);
    Serial.print(" cap=0x");
    printHex8(request.capability);
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
    Serial.print(" rssi=");
    Serial.print(rssi);
    Serial.print("dBm lqi=");
    Serial.println(lqi & 0x7F);

    if (response.status == MAC_ASSOC_SUCCESS &&
        network.completeJoin(response.shortAddress, ZB_NWK_ADDR_COORDINATOR)) {
      bool ok = applyAddress(response.shortAddress);
      Serial.println(ok ? "Join complete; MAC address programmed" :
                          "Join complete; MAC address configure FAILED");
      announced = false;
    }
  }
}

void onZdoFrame(const MacDataFrame& mac, const NwkDataFrame& nwk,
                const ApsDataFrame& aps, int8_t rssi, uint8_t lqi) {
  (void)mac;
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
  Serial.print(" cap=0x");
  printHex8(announce.capability);
  Serial.print(" src=0x");
  printHex16(nwk.srcShort);
  Serial.print(" rssi=");
  Serial.print(rssi);
  Serial.println("dBm");
}

void setup() {
  Serial.begin(115200);
  uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {}

  network.attachNeighborTable(neighbors);

  if (!radio.begin(CHANNEL)) {
    Serial.println("CC2530 not found - check wiring/firmware.");
    while (true) delay(1000);
  }

  if (IS_COORDINATOR) {
    network.beginCoordinator(PAN_ID, 0x1A62195E00000000ULL, CHANNEL);
    network.configureAddressPool(0x0001, 0x00FE);
    network.permitJoining(0xFF);
    bool ok = applyAddress(ZB_NWK_ADDR_COORDINATOR);
    radio.onMacCommandReceive(onMacCommand);
    radio.onZdoReceive(onZdoFrame);
    Serial.print("Coordinator up. PAN=0x");
    printHex16(PAN_ID);
    Serial.println(ok ? " permit=255s" : " MAC configure FAILED");
  } else {
    network.beginJoining(ZB_DEVICE_ROUTER, PAN_ID, 0x1A62195E00000000ULL,
                         CHANNEL, ZB_NWK_ADDR_COORDINATOR);
    bool ok = applyAddress(ZigbeeMac::kBroadcastShort);
    radio.onMacCommandReceive(onMacCommand);
    Serial.print("Joiner up. ieee=0x");
    printHex64(THIS_IEEE);
    Serial.println(ok ? " waiting for association" : " MAC configure FAILED");
    nextJoinAttempt = millis() + 1000;
  }
}

void loop() {
  radio.poll();

  if ((int32_t)(millis() - nextStatus) >= 0) {
    nextStatus = millis() + 10000;
    if (IS_COORDINATOR) {
      Serial.print("Coordinator status children=");
      Serial.print(neighbors.count());
      Serial.print(" permit=");
      Serial.println(network.joiningSecondsRemaining());
    } else {
      Serial.print("Joiner status state=");
      Serial.print(network.info().state);
      Serial.print(" joined=");
      Serial.print(network.isJoined() ? "yes" : "no");
      Serial.print(" addr=0x");
      printHex16(network.info().nwkAddress);
      Serial.println();
    }
  }

  if (IS_COORDINATOR) return;

  if (network.isJoining() &&
      (int32_t)(millis() - nextJoinAttempt) >= 0) {
    nextJoinAttempt = millis() + 3000;
    bool ok = radio.sendAssociationRequest(
        PAN_ID, ZB_NWK_ADDR_COORDINATOR, THIS_IEEE, JOINER_CAPABILITY, true);
    Serial.println(ok ? "ASSOC req sent" : "ASSOC req FAILED");
  }

  if (network.isJoined() && !announced) {
    announced = true;
    delay(80);
    sendDeviceAnnounce();
  }
}
