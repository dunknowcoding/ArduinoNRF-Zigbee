/*
  CC2530_FlashFirmware - flash the CC2530 transceiver firmware using ArduinoNRF's
  built-in CC-Debugger (no external TI programmer needed).

  Run this ONCE to load the firmware that the other examples (CC2530_Info,
  CC2530_Sniffer, CC2530_Link) drive over UART. After it succeeds, you can leave
  the debug wires connected or remove them.

  DEBUG WIRING (for flashing) - CC2530 <-> ArduinoNRF, 3.3 V:
      CC2530 P2.1 (DD)  <-> D8
      CC2530 P2.2 (DC)  <-> D9
      CC2530 RST        <-> D10
      CC2530 VCC -> 3V3     GND -> GND
  The runtime UART wires (CC2530 P0.2/P0.3 <-> D0/D1) may stay connected too.
  See docs/WIRING.md and docs/FLASHING.md for the full picture.

  Requires the CCDebugger library bundled with the ArduinoNRF board package.
  Open the Serial Monitor (USB) at 115200 to watch progress.
*/
#include <CCDebugger.h>
#include "cc2530_radio_fw.h"     // FW[], FW_LEN - this library's SDCC transceiver

CCDebugger dbg(8, 9, 10);        // DD=D8, DC=D9, RST=D10

void onProgress(uint8_t percent) {
  Serial.print("  flashing ");
  Serial.print(percent);
  Serial.println('%');
}

void setup() {
  Serial.begin(115200);
  uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {}

  dbg.begin();
  dbg.enterDebug();
  uint16_t id = dbg.chipID();
  Serial.print("Chip ID: 0x");
  Serial.println(id, HEX);
  if ((id >> 8) != 0xA5) {
    Serial.println("No CC2530 found - check DD/DC/RST wiring, 3.3 V and GND.");
    while (true) delay(1000);
  }

  Serial.print("Flashing transceiver firmware (");
  Serial.print(FW_LEN);
  Serial.println(" bytes)...");
  bool ok = dbg.flashFirmware(FW, FW_LEN, onProgress);
  Serial.println(ok ? "Flash complete and verified." : "FLASH/VERIFY FAILED.");

  if (ok) {
    dbg.run();                   // release debug, boot the firmware
    Serial.println("CC2530 is now running the transceiver firmware.");
    Serial.println("Load CC2530_Info / CC2530_Sniffer / CC2530_Link next.");
  }
}

void loop() {}
