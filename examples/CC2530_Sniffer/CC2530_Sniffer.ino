/*
  CC2530_Sniffer - promiscuous 802.15.4 packet sniffer.

  Receives EVERY frame on the selected channel and prints it as hex with RSSI and
  CRC status - useful for watching Zigbee / Thread / custom 802.15.4 traffic.

  Wiring + flashing: see CC2530_Info, docs/WIRING.md and docs/FLASHING.md.
  Open the Serial Monitor (USB) at 115200.
*/
#include <CC2530Radio.h>

CC2530Radio radio;

// Called from radio.poll() for each received frame. psdu excludes the FCS.
void onFrame(const uint8_t* psdu, uint8_t len, int8_t rssi, uint8_t lqi) {
  Serial.print("len=");
  Serial.print(len);
  Serial.print(" rssi=");
  Serial.print(rssi);
  Serial.print("dBm crc=");
  Serial.print((lqi & 0x80) ? "OK " : "BAD");
  Serial.print(" :");
  for (uint8_t i = 0; i < len; i++) {
    Serial.print(' ');
    if (psdu[i] < 0x10) Serial.print('0');
    Serial.print(psdu[i], HEX);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  if (!radio.begin(11)) {           // change 11 to any channel 11..26
    Serial.println("CC2530 not found - check wiring/firmware.");
    while (true) delay(1000);
  }
  radio.setPromiscuous(true);       // receive all frames (no address filtering)
  radio.onReceive(onFrame);

  Serial.print("Sniffing 802.15.4 channel ");
  Serial.println(radio.channel());
}

void loop() {
  radio.poll();                     // delivers frames to onFrame()
}
