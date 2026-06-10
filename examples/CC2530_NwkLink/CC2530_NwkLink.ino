/*
  CC2530_NwkLink - two-node Zigbee NWK data-frame link.

  Flash this sketch to two ArduinoNRF+CC2530 setups. Change THIS_NODE on one
  board to 0x0001 and on the other to 0x0002. This example sends a simple
  Zigbee NWK data frame inside the short-address 802.15.4 MAC frame.

  This is still not a full Zigbee PRO device: it does not join a network, run
  ZDO, route through neighbors, or enable Zigbee security. It demonstrates the
  next reusable frame layer above CC2530_MacLink.
*/
#include <stdio.h>
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

CC2530Radio radio;
uint32_t lastTx = 0;
uint8_t appSeq = 0;

void printHex16(uint16_t v) {
  if (v < 0x1000) Serial.print('0');
  if (v < 0x0100) Serial.print('0');
  if (v < 0x0010) Serial.print('0');
  Serial.print(v, HEX);
}

void onNwkData(const nzb::MacDataFrame& mac, const nzb::NwkDataFrame& nwk,
               int8_t rssi, uint8_t lqi) {
  if (mac.dstPanId != PAN_ID) return;
  if (mac.dstShort != THIS_NODE && mac.dstShort != nzb::ZigbeeMac::kBroadcastShort) {
    return;
  }
  if (nwk.dstShort != THIS_NODE && nwk.dstShort != nzb::ZigbeeNwk::kBroadcastRxOnWhenIdle) {
    return;
  }

  Serial.print("NWK RX macSeq=");
  Serial.print(mac.sequence);
  Serial.print(" nwkSeq=");
  Serial.print(nwk.sequence);
  Serial.print(" src=0x");
  printHex16(nwk.srcShort);
  Serial.print(" dst=0x");
  printHex16(nwk.dstShort);
  Serial.print(" radius=");
  Serial.print(nwk.radius);
  Serial.print(" rssi=");
  Serial.print(rssi);
  Serial.print("dBm lqi=");
  Serial.print(lqi & 0x7F);
  Serial.print(" payload=\"");
  for (uint8_t i = 0; i < nwk.payloadLen; ++i) {
    char c = (char)nwk.payload[i];
    Serial.write((c >= 32 && c < 127) ? c : '.');
  }
  Serial.println('"');
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  if (!radio.begin(11)) {
    Serial.println("CC2530 not found - check wiring/firmware.");
    while (true) delay(1000);
  }
  radio.onNwkReceive(onNwkData);

  Serial.print("NWK link up. PAN=0x");
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
    snprintf(msg, sizeof(msg), "nwk hello %u", appSeq++);
    bool ok = radio.sendNwkData(PAN_ID, PEER_NODE, THIS_NODE,
                                PEER_NODE, THIS_NODE,
                                (const uint8_t*)msg, (uint8_t)strlen(msg));
    Serial.print("NWK TX dst=0x");
    printHex16(PEER_NODE);
    Serial.print(" \"");
    Serial.print(msg);
    Serial.println(ok ? "\" ok" : "\" FAILED");
  }
}
