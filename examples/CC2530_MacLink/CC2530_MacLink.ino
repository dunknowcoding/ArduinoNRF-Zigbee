/*
  CC2530_MacLink - two-node IEEE 802.15.4 MAC data-frame link.

  Flash this sketch to two ArduinoNRF+CC2530 setups. Change THIS_NODE on one
  board to 0x0001 and on the other to 0x0002. Unlike CC2530_Link, this example
  wraps each message in a real 802.15.4 data frame with PAN ID, source short
  address, destination short address, and sequence number.

  This is still below a full Zigbee PRO stack: it does not join a network or run
  NWK/APS/ZCL. It is the MAC envelope that those layers will build on.
*/
#include <stdio.h>
#include <CC2530Radio.h>

#ifndef NIUS_ZIGBEE_PAN_ID
#define NIUS_ZIGBEE_PAN_ID 0x1A62
#endif

#ifndef NIUS_ZIGBEE_THIS_NODE
#define NIUS_ZIGBEE_THIS_NODE 0x0001  // set the second board to 0x0002
#endif

static const uint16_t PAN_ID = NIUS_ZIGBEE_PAN_ID;
static const uint16_t THIS_NODE = NIUS_ZIGBEE_THIS_NODE;
static const uint16_t PEER_NODE = (THIS_NODE == 0x0001) ? 0x0002 : 0x0001;

CC2530Radio radio;
uint32_t lastTx = 0;
uint8_t appSeq = 0;

void printHex16(uint16_t v) {
  if (v < 0x1000) Serial.print('0');
  if (v < 0x0100) Serial.print('0');
  if (v < 0x0010) Serial.print('0');
  Serial.print(v, HEX);
}

void onMacData(const nzb::MacDataFrame& frame, int8_t rssi, uint8_t lqi) {
  if (frame.dstPanId != PAN_ID) return;
  if (frame.dstShort != THIS_NODE && frame.dstShort != nzb::ZigbeeMac::kBroadcastShort) {
    return;
  }

  Serial.print("RX seq=");
  Serial.print(frame.sequence);
  Serial.print(" pan=0x");
  printHex16(frame.dstPanId);
  Serial.print(" src=0x");
  printHex16(frame.srcShort);
  Serial.print(" dst=0x");
  printHex16(frame.dstShort);
  Serial.print(" rssi=");
  Serial.print(rssi);
  Serial.print("dBm lqi=");
  Serial.print(lqi & 0x7F);
  Serial.print(" payload=\"");
  for (uint8_t i = 0; i < frame.payloadLen; ++i) {
    char c = (char)frame.payload[i];
    Serial.write((c >= 32 && c < 127) ? c : '.');
  }
  Serial.println('"');
}

void setup() {
  Serial.begin(115200);
  uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {}

  if (!radio.begin(11)) {
    Serial.println("CC2530 not found - check wiring/firmware.");
    while (true) delay(1000);
  }
  radio.setPromiscuous(true);
  radio.onDataReceive(onMacData);

  Serial.print("MAC link up. PAN=0x");
  printHex16(PAN_ID);
  Serial.print(" this=0x");
  printHex16(THIS_NODE);
  Serial.print(" peer=0x");
  printHex16(PEER_NODE);
  Serial.print(" channel=");
  Serial.println(radio.channel());
}

void loop() {
  radio.poll();

  if (millis() - lastTx >= 2000) {
    lastTx = millis();
    char msg[24];
    snprintf(msg, sizeof(msg), "mac hello %u", appSeq++);
    bool ok = radio.sendData(PAN_ID, PEER_NODE, THIS_NODE,
                             (const uint8_t*)msg, (uint8_t)strlen(msg));
    Serial.print("TX dst=0x");
    printHex16(PEER_NODE);
    Serial.print(" \"");
    Serial.print(msg);
    Serial.println(ok ? "\" ok" : "\" FAILED");
  }
}
