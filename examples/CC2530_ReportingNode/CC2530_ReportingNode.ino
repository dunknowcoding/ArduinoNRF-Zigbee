/*
  CC2530_ReportingNode - tiny Configure Reporting + Report Attributes demo.

  Flash this sketch to two ArduinoNRF+CC2530 setups. Change THIS_NODE on one
  board to 0x0001 and on the other to 0x0002. Each node configures the peer's
  OnOff.OnOff boolean attribute reporting, toggles the peer, and prints incoming
  Report Attributes commands.

  This is still not a complete Zigbee reporting engine. It demonstrates the
  reusable boolean report scheduler and the ZCL reporting payload helpers.
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
nzb::ZigbeeBoolReportScheduler onOffReporter;
uint16_t reportTarget = 0;
uint32_t nextAction = 0;
uint8_t actionStep = 0;
uint8_t localZclSeq = 0;

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

bool sendReportIfDue() {
  if (reportTarget == 0) return false;
  uint32_t now = millis();
  if (!onOffReporter.shouldReport(onOff.isOn(), now)) return false;

  uint8_t frame[nzb::ZigbeeZcl::kMaxFrame];
  uint8_t frameLen = onOffReporter.buildReportCommand(
      localZclSeq++, onOff.isOn(), frame, sizeof(frame));
  if (frameLen == 0) return false;
  bool ok = sendZclFrame(reportTarget, ENDPOINT, nzb::ZigbeeZcl::kClusterOnOff,
                         ENDPOINT, frame, frameLen);
  if (ok) {
    onOffReporter.markReported(onOff.isOn(), now);
  }
  Serial.println(ok ? "  Report Attributes sent" : "  Report Attributes FAILED");
  return ok;
}

void sendConfigureReportingResponse(const nzb::NwkDataFrame& nwk,
                                    const nzb::ApsDataFrame& aps,
                                    const nzb::ZclFrame& zcl,
                                    uint8_t status, uint16_t attrId) {
  uint8_t payload[4];
  uint8_t payloadLen = nzb::ZigbeeZcl::buildConfigureReportingResponsePayload(
      payload, sizeof(payload), status, nzb::ZCL_REPORT_DIRECTION_REPORTED,
      attrId);
  uint8_t frame[nzb::ZigbeeZcl::kMaxFrame];
  uint8_t frameLen = nzb::ZigbeeZcl::buildCommandFrame(
      frame, sizeof(frame), nzb::ZCL_FRAME_PROFILE_WIDE, zcl.sequence,
      nzb::ZCL_CMD_CONFIGURE_REPORTING_RESPONSE, payload, payloadLen,
      nzb::ZCL_DIRECTION_SERVER_TO_CLIENT);
  bool ok = sendZclFrame(nwk.srcShort, aps.srcEndpoint, aps.clusterId,
                         aps.dstEndpoint, frame, frameLen);
  Serial.println(ok ? "  Configure Reporting Response sent"
                    : "  Configure Reporting Response FAILED");
}

void handleConfigureReporting(const nzb::NwkDataFrame& nwk,
                              const nzb::ApsDataFrame& aps,
                              const nzb::ZclFrame& zcl) {
  uint16_t attrId = 0;
  uint16_t minInterval = 0;
  uint16_t maxInterval = 0;
  uint8_t status = nzb::ZCL_STATUS_UNREPORTABLE_ATTRIBUTE;
  if (aps.clusterId == nzb::ZigbeeZcl::kClusterOnOff &&
      nzb::ZigbeeZcl::parseConfigureReportingBoolPayload(
          zcl.payload, zcl.payloadLen, attrId, minInterval, maxInterval) &&
      attrId == nzb::ZigbeeZcl::kAttrOnOff) {
    onOffReporter.configure(attrId, minInterval, maxInterval, onOff.isOn(),
                            millis());
    reportTarget = nwk.srcShort;
    status = nzb::ZCL_STATUS_SUCCESS;
    Serial.print("  Reporting configured min=");
    Serial.print(minInterval);
    Serial.print("s max=");
    Serial.print(maxInterval);
    Serial.println("s");
  }
  sendConfigureReportingResponse(nwk, aps, zcl, status, attrId);
}

void printConfigureReportingResponse(const nzb::ZclFrame& zcl) {
  if (zcl.payloadLen == 0) {
    Serial.println("  Configure Reporting Response success");
    return;
  }
  uint8_t status = 0;
  uint8_t direction = 0;
  uint16_t attrId = 0;
  if (nzb::ZigbeeZcl::parseConfigureReportingStatusRecord(
          zcl.payload, zcl.payloadLen, status, direction, attrId)) {
    Serial.print("  Configure Reporting Response status=0x");
    printHex8(status);
    Serial.print(" attr=0x");
    printHex16(attrId);
    Serial.println();
  }
}

void printReportAttributes(const nzb::ZclFrame& zcl) {
  uint16_t attrId = 0;
  bool value = false;
  if (!nzb::ZigbeeZcl::parseReportBoolAttributePayload(
          zcl.payload, zcl.payloadLen, attrId, value)) {
    return;
  }
  Serial.print("  Report attr=0x");
  printHex16(attrId);
  Serial.print(" value=");
  Serial.println(value ? "ON" : "OFF");
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

  if (zcl.frameType == nzb::ZCL_FRAME_PROFILE_WIDE &&
      zcl.commandId == nzb::ZCL_CMD_CONFIGURE_REPORTING) {
    handleConfigureReporting(nwk, aps, zcl);
    return;
  }
  if (zcl.frameType == nzb::ZCL_FRAME_PROFILE_WIDE &&
      zcl.commandId == nzb::ZCL_CMD_CONFIGURE_REPORTING_RESPONSE) {
    printConfigureReportingResponse(zcl);
    return;
  }
  if (zcl.frameType == nzb::ZCL_FRAME_PROFILE_WIDE &&
      zcl.commandId == nzb::ZCL_CMD_REPORT_ATTRIBUTES) {
    printReportAttributes(zcl);
    return;
  }

  if (aps.clusterId == nzb::ZigbeeZcl::kClusterOnOff &&
      zcl.frameType == nzb::ZCL_FRAME_CLUSTER_SPECIFIC) {
    uint8_t response[nzb::ZigbeeZcl::kMaxFrame];
    uint8_t responseLen = onOff.handleFrame(zcl, response, sizeof(response));
    Serial.print("  OnOff state=");
    Serial.println(onOff.isOn() ? "ON" : "OFF");
    if (responseLen > 0) {
      sendZclFrame(nwk.srcShort, aps.srcEndpoint, aps.clusterId,
                   aps.dstEndpoint, response, responseLen);
    }
    sendReportIfDue();
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  if (!radio.begin(11)) {
    Serial.println("CC2530 not found - check wiring/firmware.");
    while (true) delay(1000);
  }
  radio.onZclReceive(onZclCommand);

  Serial.print("Reporting node up. PAN=0x");
  printHex16(PAN_ID);
  Serial.print(" this=0x");
  printHex16(THIS_NODE);
  Serial.print(" peer=0x");
  printHex16(PEER_NODE);
  Serial.println();

  nextAction = millis() + ((THIS_NODE == 0x0001) ? 1000 : 2500);
}

void loop() {
  radio.poll();
  sendReportIfDue();

  if ((int32_t)(millis() - nextAction) < 0) return;
  nextAction += 6000;

  uint8_t step = actionStep++ % 2;
  if (step == 0) {
    uint8_t payload[8];
    uint8_t payloadLen = nzb::ZigbeeZcl::buildConfigureReportingBoolPayload(
        payload, sizeof(payload), nzb::ZigbeeZcl::kAttrOnOff, 1, 6);
    bool ok = radio.sendZclCommand(
        PAN_ID, PEER_NODE, THIS_NODE, PEER_NODE, THIS_NODE, ENDPOINT,
        nzb::ZigbeeZcl::kClusterOnOff,
        nzb::ZigbeeAps::kProfileHomeAutomation, ENDPOINT,
        nzb::ZCL_CMD_CONFIGURE_REPORTING, payload, payloadLen,
        nzb::ZCL_FRAME_PROFILE_WIDE);
    Serial.println(ok ? "ZCL TX Configure Reporting ok"
                      : "ZCL TX Configure Reporting FAILED");
  } else {
    bool ok = radio.sendZclCommand(
        PAN_ID, PEER_NODE, THIS_NODE, PEER_NODE, THIS_NODE, ENDPOINT,
        nzb::ZigbeeZcl::kClusterOnOff,
        nzb::ZigbeeAps::kProfileHomeAutomation, ENDPOINT,
        nzb::ZCL_ON_OFF_CMD_TOGGLE);
    Serial.println(ok ? "ZCL TX OnOff Toggle ok"
                      : "ZCL TX OnOff Toggle FAILED");
  }
}
