/*
  CC2530_NwkCommandLink - two-node Zigbee NWK command-frame demo.

  Flash this sketch to two ArduinoNRF+CC2530 setups. Build one with
  NIUS_ZIGBEE_THIS_NODE=0x0001 and the other with 0x0002. The nodes exchange
  Route Request, Route Reply, and Network Status command frames.
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
static const uint16_t PEER_NODE = (THIS_NODE == 0x0001) ? 0x0002 : 0x0001;
static const uint64_t THIS_IEEE = 0x1A62195E00000000ULL | THIS_NODE;

CC2530Radio radio;
uint8_t routeRequestId = 1;
uint32_t nextAction = 0;
uint8_t actionStep = 0;

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

bool sendNwkCommand(uint16_t dstShort, uint8_t commandId,
                    const uint8_t* payload, uint8_t payloadLen) {
  return radio.sendNwkCommand(PAN_ID, dstShort, THIS_NODE, dstShort, THIS_NODE,
                              commandId, payload, payloadLen,
                              nzb::ZigbeeNwk::kDefaultRadius, true);
}

void onNwkCommand(const nzb::MacDataFrame& mac,
                  const nzb::NwkCommandFrame& nwk,
                  int8_t rssi, uint8_t lqi) {
  if (mac.dstPanId != PAN_ID) return;
  if (nwk.dstShort != THIS_NODE) return;

  Serial.print("NWK CMD RX src=0x");
  printHex16(nwk.srcShort);
  Serial.print(" cmd=0x");
  printHex8(nwk.commandId);
  Serial.print(" rssi=");
  Serial.print(rssi);
  Serial.print("dBm lqi=");
  Serial.println(lqi & 0x7F);

  if (nwk.commandId == nzb::NWK_CMD_ROUTE_REQUEST) {
    nzb::NwkRouteRequestCommand req;
    if (!nzb::ZigbeeNwk::parseRouteRequestPayload(
            nwk.payload, nwk.payloadLen, req)) {
      return;
    }
    Serial.print("  Route Request id=");
    Serial.print(req.routeRequestId);
    Serial.print(" dst=0x");
    printHex16(req.destination);
    Serial.print(" cost=");
    Serial.println(req.pathCost);

    uint8_t payload[16];
    uint8_t n = nzb::ZigbeeNwk::buildRouteReplyPayload(
        payload, sizeof(payload), req.routeRequestId, nwk.srcShort, THIS_NODE,
        (uint8_t)(req.pathCost + 1));
    bool ok = sendNwkCommand(nwk.srcShort, nzb::NWK_CMD_ROUTE_REPLY, payload, n);
    Serial.println(ok ? "  Route Reply sent" : "  Route Reply FAILED");
    return;
  }

  if (nwk.commandId == nzb::NWK_CMD_ROUTE_REPLY) {
    nzb::NwkRouteReplyCommand rsp;
    if (!nzb::ZigbeeNwk::parseRouteReplyPayload(
            nwk.payload, nwk.payloadLen, rsp)) {
      return;
    }
    Serial.print("  Route Reply id=");
    Serial.print(rsp.routeRequestId);
    Serial.print(" origin=0x");
    printHex16(rsp.originator);
    Serial.print(" responder=0x");
    printHex16(rsp.responder);
    Serial.print(" cost=");
    Serial.println(rsp.pathCost);
    return;
  }

  if (nwk.commandId == nzb::NWK_CMD_NETWORK_STATUS) {
    nzb::NwkNetworkStatusCommand status;
    if (!nzb::ZigbeeNwk::parseNetworkStatusPayload(
            nwk.payload, nwk.payloadLen, status)) {
      return;
    }
    Serial.print("  Network Status status=0x");
    printHex8(status.status);
    Serial.print(" dst=0x");
    printHex16(status.destination);
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {}

  if (!radio.begin(15)) {
    Serial.println("CC2530 not found - check wiring/firmware.");
    while (true) delay(1000);
  }

  uint8_t ieeeBytes[8];
  for (uint8_t i = 0; i < 8; ++i) ieeeBytes[i] = (uint8_t)(THIS_IEEE >> (8 * i));
  bool ok = radio.setAddress(PAN_ID, THIS_NODE, ieeeBytes);
  ok = ok && radio.configureMac(
      CC2530Radio::kMacFilter | CC2530Radio::kMacAutoAck | CC2530Radio::kMacCcaTx,
      2);
  radio.onNwkCommandReceive(onNwkCommand);

  Serial.print("NWK command node up. PAN=0x");
  printHex16(PAN_ID);
  Serial.print(" this=0x");
  printHex16(THIS_NODE);
  Serial.print(" peer=0x");
  printHex16(PEER_NODE);
  Serial.println(ok ? " mac=filtered" : " mac=configure FAILED");

  nextAction = millis() + ((THIS_NODE == 0x0001) ? 1000 : 2800);
}

void loop() {
  radio.poll();

  if ((int32_t)(millis() - nextAction) < 0) return;
  nextAction += ((THIS_NODE == 0x0001) ? 5200 : 6900);

  uint8_t payload[16];
  uint8_t n = 0;
  uint8_t commandId = 0;
  if ((actionStep++ & 1) == 0) {
    n = nzb::ZigbeeNwk::buildRouteRequestPayload(
        payload, sizeof(payload), routeRequestId++, PEER_NODE, 0);
    commandId = nzb::NWK_CMD_ROUTE_REQUEST;
    Serial.print("NWK CMD TX Route Request ");
  } else {
    n = nzb::ZigbeeNwk::buildNetworkStatusPayload(
        payload, sizeof(payload), nzb::NWK_STATUS_VALIDATE_ROUTE, PEER_NODE);
    commandId = nzb::NWK_CMD_NETWORK_STATUS;
    Serial.print("NWK CMD TX Network Status ");
  }

  bool ok = (n > 0) && sendNwkCommand(PEER_NODE, commandId, payload, n);
  Serial.println(ok ? "ok" : "FAILED");
}
