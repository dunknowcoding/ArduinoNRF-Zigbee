/*
  CC2530_EnergyScan - IEEE 802.15.4 Energy-Detect (ED) scan across the band.

  Requires CC2530 firmware v0.9+ (the CMD_ED_SCAN primitive). For each channel
  11..26 the module tunes its receiver, samples the RSSI, and reports the PEAK
  energy; the host prints it in dBm (dBm = raw - 73). This is the MLME-SCAN
  energy-detect the official MAC uses to (a) pick the quietest channel when a
  coordinator forms a network and (b) detect a jammed/busy channel for frequency
  agility. A loud transmitter (Wi-Fi, another Zigbee net, a microwave) shows up as
  an elevated reading on the affected channels.

  Wiring: UART D0/D1 to the CC2530 (3.3 V). See docs/WIRING.md.
*/
#include <CC2530Radio.h>

CC2530Radio radio;

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis(); while (!Serial && millis() - t0 < 3000) {}
  if (!radio.begin(11)) {
    Serial.println("CC2530 not responding - check wiring/firmware");
  }
  Serial.print("CC2530 firmware 0x");
  Serial.println(radio.firmwareVersion(), HEX);
  Serial.println("Energy scan (dBm peak per channel):");
}

void loop() {
  int8_t quietest = 127; uint8_t quietestCh = 11;
  Serial.print("ED");
  for (uint8_t ch = 11; ch <= 26; ++ch) {
    int8_t peak = 0;
    if (radio.energyScan(ch, peak)) {
      int dbm = (int)peak - 73;
      Serial.print(" "); Serial.print(ch); Serial.print(":"); Serial.print(dbm);
      if (peak < quietest) { quietest = peak; quietestCh = ch; }
    } else {
      Serial.print(" "); Serial.print(ch); Serial.print(":ERR");
    }
  }
  radio.setChannel(11);  // restore an operating channel after the sweep
  Serial.print("  -> quietest ch "); Serial.print(quietestCh);
  Serial.print(" ("); Serial.print((int)quietest - 73); Serial.println(" dBm)");
  delay(2000);
}
