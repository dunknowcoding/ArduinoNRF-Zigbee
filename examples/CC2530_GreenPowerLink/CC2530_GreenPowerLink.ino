/*
  CC2530_GreenPowerLink - Green Power on air: a battery-less GPD controls a sink.

  Two roles, built with -DNIUS_ZIGBEE_GP_ROLE=1 (the GPD) or =2 (the sink):

    * GPD  - joins NO network. Every few seconds it builds a secured Green Power
             Data Frame (a Toggle command, encrypted under its GPD key with an
             incrementing frame counter) and broadcasts it as a raw 802.15.4
             frame. This is what a self-powered Zigbee wall switch does.
    * SINK - commissions the GPD (here with a pre-shared key for the demo),
             receives the raw frames, parses + decrypts the GPDF, rejects
             replays, and toggles its built-in LED on each Toggle.

  This puts the M8 Green Power tooling on the radio: no join, no network key,
  just secured GPDFs over a 1-hop broadcast. Flash one board =1 and another =2.
*/

#include <CC2530Radio.h>

#ifndef NIUS_ZIGBEE_GP_ROLE
#define NIUS_ZIGBEE_GP_ROLE 2
#endif
#define ROLE_GPD (NIUS_ZIGBEE_GP_ROLE == 1)

CC2530Radio radio;

static const uint8_t CHANNEL = 20;          // isolated from the ch15 demo mesh
static const uint16_t GP_PAN = 0x1A62;
static const uint32_t GPD_SRC_ID = 0x01234567;
static const uint8_t GPD_KEY[16] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                                    0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};

void printHex(uint8_t v) { if (v < 16) Serial.print('0'); Serial.print(v, HEX); }

#if ROLE_GPD
uint32_t gpdCounter = 1;
uint32_t nextTxAt = 0;
#else
GpSinkEntry sinkStorage[2];
ZigbeeGpSinkTable sink;
uint8_t ledOn = 0;
uint32_t accepted = 0;

void onMac(const MacDataFrame& f, int8_t rssi, uint8_t lqi) {
  (void)rssi; (void)lqi;
  GpdfFrame g;
  if (!ZigbeeGreenPower::parse(f.payload, f.payloadLen, g)) return;  // not a GPDF
  GpSinkEntry* e = sink.find(g.srcId);
  if (!e) { Serial.print("GPDF from uncommissioned 0x"); Serial.println(g.srcId, HEX); return; }

  GpdfFrame dec;
  if (!ZigbeeGreenPower::open(f.payload, f.payloadLen, e->key, dec)) {
    Serial.println("GPDF MIC FAIL");
    return;
  }
  if (!sink.checkAndUpdateCounter(dec.srcId, dec.frameCounter)) {
    Serial.print("GPDF replay fc="); Serial.println(dec.frameCounter);
    return;
  }
  if (dec.commandId == GPD_CMD_TOGGLE) {
    ledOn ^= 1;
    digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);
  } else if (dec.commandId == GPD_CMD_ON) {
    ledOn = 1; digitalWrite(LED_BUILTIN, HIGH);
  } else if (dec.commandId == GPD_CMD_OFF) {
    ledOn = 0; digitalWrite(LED_BUILTIN, LOW);
  }
  ++accepted;
  Serial.print("GP cmd 0x"); printHex(dec.commandId);
  Serial.print(" fc="); Serial.print(dec.frameCounter);
  Serial.print(" -> LED="); Serial.println(ledOn ? "ON" : "OFF");
}
#endif

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) {}
  if (!radio.begin(CHANNEL)) {
    Serial.println("CC2530 not found - check wiring/firmware.");
    while (true) delay(1000);
  }
  uint8_t ieee[8] = {0x01,0,0,0,0x5E,0x19,0x62,0x1A};

#if ROLE_GPD
  Serial.println("Green Power DEVICE (GPD): broadcasting secured Toggles on ch20");
  radio.setAddress(GP_PAN, 0xCCCC, ieee);
  radio.configureMac(CC2530Radio::kMacCcaTx, 3);
#else
  Serial.println("Green Power SINK: commission GPD, toggle LED on its frames (ch20)");
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  radio.setAddress(GP_PAN, 0x0000, ieee);
  radio.configureMac(CC2530Radio::kMacFilter | CC2530Radio::kMacAutoAck, 3);
  sink.begin(sinkStorage, 2);
  sink.commission(GPD_SRC_ID, /*deviceId=*/0x02, GPD_KEY);
  radio.onDataReceive(onMac);
  Serial.print("commissioned GPD 0x"); Serial.println(GPD_SRC_ID, HEX);
#endif
}

void loop() {
  radio.poll();
#if ROLE_GPD
  if ((int32_t)(millis() - nextTxAt) >= 0) {
    nextTxAt = millis() + 3000;
    uint8_t gpdf[24];
    uint8_t n = ZigbeeGreenPower::secure(gpdf, sizeof(gpdf), GPD_KEY, GPD_SRC_ID,
                                         gpdCounter++, GPD_CMD_TOGGLE, nullptr, 0);
    bool ok = n && radio.sendData(GP_PAN, 0xFFFF, 0xCCCC, gpdf, n, false);
    Serial.print("GPDF Toggle fc="); Serial.print(gpdCounter - 1);
    Serial.println(ok ? " broadcast" : " TX-FAIL");
  }
#endif
}
