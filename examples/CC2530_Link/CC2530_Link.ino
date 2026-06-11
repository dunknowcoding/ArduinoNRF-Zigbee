/*
  CC2530_Link - a simple two-node radio link.

  Flash this same sketch onto two ArduinoNRF+CC2530 setups on the same channel.
  Each node broadcasts a "hello" every 2 s and prints whatever it receives. This
  uses RAW 802.15.4 frames (arbitrary payload), which is fine for CC2530<->CC2530
  links; talking to real Zigbee devices needs a proper MAC header (MHR).

  Wiring + flashing: see CC2530_Info, docs/WIRING.md and docs/FLASHING.md.
  Open the Serial Monitor (USB) at 115200.
*/
#include <CC2530Radio.h>

CC2530Radio radio;
uint32_t lastTx = 0;
uint8_t seq = 0;

void onFrame(const uint8_t* psdu, uint8_t len, int8_t rssi, uint8_t lqi) {
  Serial.print("RX (");
  Serial.print(rssi);
  Serial.print(" dBm): ");
  for (uint8_t i = 0; i < len; i++) {
    char c = (char)psdu[i];
    Serial.write((c >= 32 && c < 127) ? c : '.');
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {}

  if (!radio.begin(11)) {
    Serial.println("CC2530 not found - check wiring/firmware.");
    while (true) delay(1000);
  }
  radio.onReceive(onFrame);
  Serial.println("Link up on channel 11. Broadcasting every 2 s.");
}

void loop() {
  radio.poll();

  if (millis() - lastTx >= 2000) {
    lastTx = millis();
    char msg[16] = "hello ";
    itoa(seq++, msg + 6, 10);          // append the sequence number
    uint8_t n = (uint8_t)strlen(msg);
    bool ok = radio.send((const uint8_t*)msg, n);
    Serial.print("TX \"");
    Serial.print(msg);
    Serial.println(ok ? "\" ok" : "\" FAILED");
  }
}
