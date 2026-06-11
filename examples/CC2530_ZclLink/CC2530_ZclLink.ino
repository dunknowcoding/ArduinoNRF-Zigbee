/*
  CC2530_ZclLink - two-node ZCL command-frame link.

  Flash this sketch to two ArduinoNRF+CC2530 setups. Change THIS_NODE on one
  board to 0x0001 and on the other to 0x0002. It sends a ZCL On/Off Toggle
  command through APS, NWK, and short-address MAC data frames.

  This is not a joined Zigbee Home Automation device. It is a frame-level
  exercise path for the APS/ZCL helpers.
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
uint32_t nextTx = 0;

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
  Serial.print(" apsCounter=");
  Serial.print(aps.counter);
  Serial.print(" zclSeq=");
  Serial.print(zcl.sequence);
  Serial.print(" cluster=0x");
  printHex16(aps.clusterId);
  Serial.print(" cmd=0x");
  printHex8(zcl.commandId);
  Serial.print(" rssi=");
  Serial.print(rssi);
  Serial.print("dBm lqi=");
  Serial.println(lqi & 0x7F);
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

  Serial.print("ZCL link up. PAN=0x");
  printHex16(PAN_ID);
  Serial.print(" this=0x");
  printHex16(THIS_NODE);
  Serial.print(" peer=0x");
  printHex16(PEER_NODE);
  Serial.print(" endpoint=");
  Serial.print(ENDPOINT);
  Serial.print(" channel=");
  Serial.println(radio.channel());

  nextTx = millis() + ((THIS_NODE == 0x0001) ? 1000 : 2000);
}

void loop() {
  radio.poll();

  if ((int32_t)(millis() - nextTx) >= 0) {
    nextTx += 2000;
    bool ok = radio.sendZclCommand(
        PAN_ID, PEER_NODE, THIS_NODE, PEER_NODE, THIS_NODE,
        ENDPOINT, nzb::ZigbeeZcl::kClusterOnOff,
        nzb::ZigbeeAps::kProfileHomeAutomation, ENDPOINT,
        nzb::ZCL_ON_OFF_CMD_TOGGLE);
    Serial.print("ZCL TX dst=0x");
    printHex16(PEER_NODE);
    Serial.print(" cmd=0x");
    printHex8(nzb::ZCL_ON_OFF_CMD_TOGGLE);
    Serial.println(ok ? " ok" : " FAILED");
  }
}
