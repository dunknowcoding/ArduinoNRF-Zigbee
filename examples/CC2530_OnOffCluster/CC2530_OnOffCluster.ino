/*
  CC2530_OnOffCluster - tiny two-node On/Off cluster behavior demo.

  Flash this sketch to two ArduinoNRF+CC2530 setups. Change THIS_NODE on one
  board to 0x0001 and on the other to 0x0002. Each node periodically sends a ZCL
  On/Off Toggle command to the peer. The receiver applies the command to a local
  boolean state, replies with a ZCL Default Response, and answers Read Attributes
  for the OnOff attribute.

  This is still not a real joined Zigbee device: there is no ZDO, binding table,
  reporting engine, Trust Center, or Zigbee security. It is a small cluster
  behavior layer on top of the frame helpers.
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
static const uint8_t ENDPOINT = 1;

CC2530Radio radio;
bool onState = false;
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

bool sendZclFrame(uint16_t dstShort, uint8_t dstEndpoint, uint16_t clusterId,
                  uint8_t srcEndpoint, const uint8_t* zcl, uint8_t zclLen) {
  return radio.sendApsData(PAN_ID, dstShort, THIS_NODE, dstShort, THIS_NODE,
                           dstEndpoint, clusterId,
                           nzb::ZigbeeAps::kProfileHomeAutomation,
                           srcEndpoint, zcl, zclLen);
}

void sendDefaultResponse(const nzb::NwkDataFrame& nwk,
                         const nzb::ApsDataFrame& aps,
                         const nzb::ZclFrame& zcl, uint8_t status) {
  uint8_t payload[2];
  uint8_t payloadLen = nzb::ZigbeeZcl::buildDefaultResponsePayload(
      payload, sizeof(payload), zcl.commandId, status);
  uint8_t frame[nzb::ZigbeeZcl::kMaxFrame];
  uint8_t frameLen = nzb::ZigbeeZcl::buildCommandFrame(
      frame, sizeof(frame), nzb::ZCL_FRAME_PROFILE_WIDE, zcl.sequence,
      nzb::ZCL_CMD_DEFAULT_RESPONSE, payload, payloadLen,
      nzb::ZCL_DIRECTION_SERVER_TO_CLIENT);
  bool ok = sendZclFrame(nwk.srcShort, aps.srcEndpoint, aps.clusterId,
                         aps.dstEndpoint, frame, frameLen);
  Serial.println(ok ? "  Default Response sent" : "  Default Response FAILED");
}

void sendReadAttributesResponse(const nzb::NwkDataFrame& nwk,
                                const nzb::ApsDataFrame& aps,
                                const nzb::ZclFrame& zcl) {
  uint8_t payload[16];
  uint8_t payloadLen = 0;
  uint16_t attrId = 0;
  if (nzb::ZigbeeZcl::getReadAttributeId(zcl.payload, zcl.payloadLen, 0, attrId) &&
      attrId == nzb::ZigbeeZcl::kAttrOnOff) {
    payloadLen = nzb::ZigbeeZcl::buildBoolAttributeRecord(
        payload, sizeof(payload), attrId, onState);
  } else {
    payloadLen = nzb::ZigbeeZcl::buildAttributeStatusRecord(
        payload, sizeof(payload), attrId, nzb::ZCL_STATUS_UNSUPPORTED_ATTRIBUTE);
  }

  uint8_t frame[nzb::ZigbeeZcl::kMaxFrame];
  uint8_t frameLen = nzb::ZigbeeZcl::buildCommandFrame(
      frame, sizeof(frame), nzb::ZCL_FRAME_PROFILE_WIDE, zcl.sequence,
      nzb::ZCL_CMD_READ_ATTRIBUTES_RESPONSE, payload, payloadLen,
      nzb::ZCL_DIRECTION_SERVER_TO_CLIENT);
  bool ok = sendZclFrame(nwk.srcShort, aps.srcEndpoint, aps.clusterId,
                         aps.dstEndpoint, frame, frameLen);
  Serial.println(ok ? "  Read Attributes Response sent"
                    : "  Read Attributes Response FAILED");
}

void onZclCommand(const nzb::MacDataFrame& mac, const nzb::NwkDataFrame& nwk,
                  const nzb::ApsDataFrame& aps, const nzb::ZclFrame& zcl,
                  int8_t rssi, uint8_t lqi) {
  if (mac.dstPanId != PAN_ID) return;
  if (nwk.dstShort != THIS_NODE) return;
  if (aps.dstEndpoint != ENDPOINT) return;
  if (aps.profileId != nzb::ZigbeeAps::kProfileHomeAutomation) return;
  if (aps.clusterId != nzb::ZigbeeZcl::kClusterOnOff) return;

  Serial.print("ZCL RX src=0x");
  printHex16(nwk.srcShort);
  Serial.print(" cmd=0x");
  printHex8(zcl.commandId);
  Serial.print(" seq=");
  Serial.print(zcl.sequence);
  Serial.print(" rssi=");
  Serial.print(rssi);
  Serial.print("dBm lqi=");
  Serial.println(lqi & 0x7F);

  if (zcl.frameType == nzb::ZCL_FRAME_CLUSTER_SPECIFIC &&
      zcl.direction == nzb::ZCL_DIRECTION_CLIENT_TO_SERVER) {
    bool handled = nzb::ZigbeeZcl::applyOnOffCommand(zcl.commandId, onState);
    Serial.print("  OnOff state=");
    Serial.println(onState ? "ON" : "OFF");
    sendDefaultResponse(nwk, aps, zcl,
                        handled ? nzb::ZCL_STATUS_SUCCESS
                                : nzb::ZCL_STATUS_UNSUPPORTED_CLUSTER_COMMAND);
    return;
  }

  if (zcl.frameType == nzb::ZCL_FRAME_PROFILE_WIDE &&
      zcl.commandId == nzb::ZCL_CMD_READ_ATTRIBUTES) {
    sendReadAttributesResponse(nwk, aps, zcl);
    return;
  }

  if (zcl.frameType == nzb::ZCL_FRAME_PROFILE_WIDE &&
      zcl.commandId == nzb::ZCL_CMD_DEFAULT_RESPONSE) {
    uint8_t originalCmd = 0;
    uint8_t status = 0;
    if (nzb::ZigbeeZcl::parseDefaultResponsePayload(
            zcl.payload, zcl.payloadLen, originalCmd, status)) {
      Serial.print("  Default Response for cmd=0x");
      printHex8(originalCmd);
      Serial.print(" status=0x");
      printHex8(status);
      Serial.println();
    }
  }
}

void setup() {
  Serial.begin(115200);
  uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {}

  if (!radio.begin(11)) {
    Serial.println("CC2530 not found - check wiring/firmware.");
    while (true) delay(1000);
  }
  radio.onZclReceive(onZclCommand);

  Serial.print("OnOff cluster up. PAN=0x");
  printHex16(PAN_ID);
  Serial.print(" this=0x");
  printHex16(THIS_NODE);
  Serial.print(" peer=0x");
  printHex16(PEER_NODE);
  Serial.print(" state=");
  Serial.println(onState ? "ON" : "OFF");

  nextAction = millis() + ((THIS_NODE == 0x0001) ? 1000 : 2800);
}

void loop() {
  radio.poll();

  if ((int32_t)(millis() - nextAction) >= 0) {
    nextAction += ((THIS_NODE == 0x0001) ? 4300 : 5900);
    if ((actionStep++ & 1) == 0) {
      bool ok = radio.sendZclCommand(
          PAN_ID, PEER_NODE, THIS_NODE, PEER_NODE, THIS_NODE, ENDPOINT,
          nzb::ZigbeeZcl::kClusterOnOff,
          nzb::ZigbeeAps::kProfileHomeAutomation, ENDPOINT,
          nzb::ZCL_ON_OFF_CMD_TOGGLE);
      Serial.println(ok ? "ZCL TX Toggle ok" : "ZCL TX Toggle FAILED");
    } else {
      uint16_t attrId = nzb::ZigbeeZcl::kAttrOnOff;
      uint8_t payload[2];
      uint8_t payloadLen = nzb::ZigbeeZcl::buildReadAttributesPayload(
          payload, sizeof(payload), &attrId, 1);
      bool ok = radio.sendZclCommand(
          PAN_ID, PEER_NODE, THIS_NODE, PEER_NODE, THIS_NODE, ENDPOINT,
          nzb::ZigbeeZcl::kClusterOnOff,
          nzb::ZigbeeAps::kProfileHomeAutomation, ENDPOINT,
          nzb::ZCL_CMD_READ_ATTRIBUTES, payload, payloadLen,
          nzb::ZCL_FRAME_PROFILE_WIDE);
      Serial.println(ok ? "ZCL TX Read OnOff ok" : "ZCL TX Read OnOff FAILED");
    }
  }
}
