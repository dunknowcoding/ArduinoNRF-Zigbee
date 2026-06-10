/*
  CC2530_ClusterNode - tiny reusable Basic + On/Off cluster node demo.

  Flash this sketch to two ArduinoNRF+CC2530 setups. Change THIS_NODE on one
  board to 0x0001 and on the other to 0x0002. Each node periodically sends:

    * On/Off Toggle
    * Read Attributes for OnOff.OnOff
    * Read Attributes for Basic.ManufacturerName / ModelIdentifier / PowerSource

  The receiver dispatches incoming ZCL frames to ZigbeeOnOffCluster or
  ZigbeeBasicCluster, then sends the generated response frame back.
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
nzb::ZigbeeOnOffCluster onOff(false);
nzb::ZigbeeBasicCluster basic;
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

void printDefaultResponse(const nzb::ZclFrame& zcl) {
  uint8_t originalCmd = 0;
  uint8_t status = 0;
  if (!nzb::ZigbeeZcl::parseDefaultResponsePayload(
          zcl.payload, zcl.payloadLen, originalCmd, status)) {
    return;
  }
  Serial.print("  Default Response cmd=0x");
  printHex8(originalCmd);
  Serial.print(" status=0x");
  printHex8(status);
  Serial.println();
}

void onZclCommand(const nzb::MacDataFrame& mac, const nzb::NwkDataFrame& nwk,
                  const nzb::ApsDataFrame& aps, const nzb::ZclFrame& zcl,
                  int8_t rssi, uint8_t lqi) {
  if (mac.dstPanId != PAN_ID) return;
  if (nwk.dstShort != THIS_NODE) return;
  if (aps.dstEndpoint != ENDPOINT) return;
  if (aps.profileId != nzb::ZigbeeAps::kProfileHomeAutomation) return;

  Serial.print("ZCL RX src=0x");
  printHex16(nwk.srcShort);
  Serial.print(" cluster=0x");
  printHex16(aps.clusterId);
  Serial.print(" cmd=0x");
  printHex8(zcl.commandId);
  Serial.print(" rssi=");
  Serial.print(rssi);
  Serial.print("dBm lqi=");
  Serial.println(lqi & 0x7F);

  if (zcl.commandId == nzb::ZCL_CMD_DEFAULT_RESPONSE) {
    printDefaultResponse(zcl);
    return;
  }

  uint8_t response[nzb::ZigbeeZcl::kMaxFrame];
  uint8_t responseLen = 0;
  if (aps.clusterId == nzb::ZigbeeZcl::kClusterOnOff) {
    responseLen = onOff.handleFrame(zcl, response, sizeof(response));
    Serial.print("  OnOff state=");
    Serial.println(onOff.isOn() ? "ON" : "OFF");
  } else if (aps.clusterId == nzb::ZigbeeZcl::kClusterBasic) {
    responseLen = basic.handleFrame(zcl, response, sizeof(response));
  }

  if (responseLen > 0) {
    bool ok = sendZclFrame(nwk.srcShort, aps.srcEndpoint, aps.clusterId,
                           aps.dstEndpoint, response, responseLen);
    Serial.println(ok ? "  Cluster response sent" : "  Cluster response FAILED");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  basic.setIdentity("NiusRobotLab", "ArduinoNRF-Zigbee", "2026-06-10");
  basic.setPowerSource(0x03);

  if (!radio.begin(11)) {
    Serial.println("CC2530 not found - check wiring/firmware.");
    while (true) delay(1000);
  }
  radio.onZclReceive(onZclCommand);

  Serial.print("Cluster node up. PAN=0x");
  printHex16(PAN_ID);
  Serial.print(" this=0x");
  printHex16(THIS_NODE);
  Serial.print(" peer=0x");
  printHex16(PEER_NODE);
  Serial.print(" state=");
  Serial.println(onOff.isOn() ? "ON" : "OFF");

  nextAction = millis() + ((THIS_NODE == 0x0001) ? 1000 : 2500);
}

void loop() {
  radio.poll();

  if ((int32_t)(millis() - nextAction) < 0) return;
  nextAction += 5000;

  uint8_t step = actionStep++ % 3;
  if (step == 0) {
    bool ok = radio.sendZclCommand(
        PAN_ID, PEER_NODE, THIS_NODE, PEER_NODE, THIS_NODE, ENDPOINT,
        nzb::ZigbeeZcl::kClusterOnOff,
        nzb::ZigbeeAps::kProfileHomeAutomation, ENDPOINT,
        nzb::ZCL_ON_OFF_CMD_TOGGLE);
    Serial.println(ok ? "ZCL TX OnOff Toggle ok"
                      : "ZCL TX OnOff Toggle FAILED");
    return;
  }

  uint16_t attrs[3];
  uint8_t attrCount = 0;
  uint16_t clusterId = 0;
  if (step == 1) {
    clusterId = nzb::ZigbeeZcl::kClusterOnOff;
    attrs[attrCount++] = nzb::ZigbeeZcl::kAttrOnOff;
  } else {
    clusterId = nzb::ZigbeeZcl::kClusterBasic;
    attrs[attrCount++] = nzb::ZigbeeZcl::kAttrBasicManufacturerName;
    attrs[attrCount++] = nzb::ZigbeeZcl::kAttrBasicModelIdentifier;
    attrs[attrCount++] = nzb::ZigbeeZcl::kAttrBasicPowerSource;
  }

  uint8_t payload[8];
  uint8_t payloadLen = nzb::ZigbeeZcl::buildReadAttributesPayload(
      payload, sizeof(payload), attrs, attrCount);
  bool ok = radio.sendZclCommand(
      PAN_ID, PEER_NODE, THIS_NODE, PEER_NODE, THIS_NODE, ENDPOINT, clusterId,
      nzb::ZigbeeAps::kProfileHomeAutomation, ENDPOINT,
      nzb::ZCL_CMD_READ_ATTRIBUTES, payload, payloadLen,
      nzb::ZCL_FRAME_PROFILE_WIDE);
  Serial.println(ok ? "ZCL TX Read Attributes ok"
                    : "ZCL TX Read Attributes FAILED");
}
