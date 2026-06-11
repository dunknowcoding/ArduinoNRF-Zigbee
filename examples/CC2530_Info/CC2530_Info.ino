/*
  CC2530_Info - confirm the CC2530 radio module is alive and talking.

  The CC2530 must already be flashed with this library's transceiver firmware
  (run the CC2530_FlashFirmware example once, or see docs/FLASHING.md).

  Runtime UART wiring (ArduinoNRF ProMicro <-> CC2530), 3.3 V logic, no level
  shifter:
      ProMicro TX (D0) --> CC2530 P0.2 (RX)
      ProMicro RX (D1) <-- CC2530 P0.3 (TX)
      3V3 --> VCC          GND --> GND
  P2.0 (CFG1) is NOT used by this firmware - leave it as-is. (Grounding it is
  harmless and is only required if you later flash TI Z-Stack; see docs/WIRING.md.)

  Open the Serial Monitor (USB) at 115200 to read the report.
*/
#include <CC2530Radio.h>

CC2530Radio radio;          // talks to the module on Serial1 (D0/D1)

void setup() {
  Serial.begin(115200);     // USB CDC to the PC (use usbcdc=enabled)
  uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {}

  if (!radio.begin(11)) {   // 115200 UART link, 802.15.4 channel 11
    Serial.println("CC2530 not responding - check wiring and that the module");
    Serial.println("is flashed with the ArduinoNRF-Zigbee firmware.");
    while (true) delay(1000);
  }

  Serial.print("CC2530 online. Firmware v");
  Serial.print(radio.firmwareVersion() >> 8);
  Serial.print('.');
  Serial.println(radio.firmwareVersion() & 0xFF);
  Serial.print("Channel: ");
  Serial.println(radio.channel());
}

void loop() {
  Serial.println(radio.ping() ? "ping -> PONG" : "ping -> no reply");
  delay(1000);
}
