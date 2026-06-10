/*
  CC2530_MacControl - configure the SDCC firmware's 802.15.4 MAC assists.

  Flash this sketch to one ArduinoNRF+CC2530 setup after updating the CC2530
  with CC2530_FlashFirmware. It verifies the v0.2 firmware commands that set
  hardware PAN/address filtering, Auto ACK, CCA transmit, and retry count.
*/

#include <CC2530Radio.h>

#ifndef NIUS_ZIGBEE_PAN_ID
#define NIUS_ZIGBEE_PAN_ID 0x1A62
#endif
#ifndef NIUS_ZIGBEE_NODE_ID
#define NIUS_ZIGBEE_NODE_ID 0x0001
#endif

CC2530Radio radio;
bool radioReady = false;

static const uint8_t IEEE_ADDR[8] = {
  0x01, 0x00, 0x00, 0x00, 0x5E, 0x19, 0x62, 0x1A
};

void printHex8(uint8_t v) {
  if (v < 0x10) Serial.print('0');
  Serial.print(v, HEX);
}

void printHex16(uint16_t v) {
  printHex8((uint8_t)(v >> 8));
  printHex8((uint8_t)v);
}

void printMacInfo(const CC2530MacInfo& info) {
  Serial.print("flags=0x");
  printHex8(info.flags);
  Serial.print(" retries=");
  Serial.print(info.retries);
  Serial.print(" pan=0x");
  printHex16(info.panId);
  Serial.print(" short=0x");
  printHex16(info.shortAddress);
  Serial.print(" ieee=");
  for (int i = 7; i >= 0; --i) {
    printHex8(info.ieeeAddress[i]);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  Serial.println("CC2530 MAC control");
  if (!radio.begin(15)) {
    Serial.println("CC2530 not found - check wiring/firmware.");
    return;
  }
  radioReady = true;

  Serial.print("Firmware v");
  Serial.print(radio.firmwareVersion() >> 8);
  Serial.print('.');
  Serial.println(radio.firmwareVersion() & 0xFF);

  bool ok = radio.setAddress(NIUS_ZIGBEE_PAN_ID, NIUS_ZIGBEE_NODE_ID, IEEE_ADDR);
  ok = ok && radio.configureMac(
      CC2530Radio::kMacFilter | CC2530Radio::kMacAutoAck | CC2530Radio::kMacCcaTx,
      3);
  ok = ok && radio.setTxPowerRaw(0xF5);
  Serial.println(ok ? "MAC assist configured" : "MAC assist configure failed");

  CC2530MacInfo info;
  if (radio.getMacInfo(info)) {
    printMacInfo(info);
  } else {
    Serial.println("MAC info read failed");
  }
}

void loop() {
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint >= 2000) {
    lastPrint = millis();
    if (!radioReady) {
      Serial.println("CC2530 not ready");
      return;
    }
    CC2530MacInfo info;
    if (radio.getMacInfo(info)) {
      printMacInfo(info);
    } else {
      Serial.println("MAC info read failed");
    }
  }
  if (radioReady) radio.poll();
}
