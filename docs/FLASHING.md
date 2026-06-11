# Flashing the CC2530 with ArduinoNRF's built-in CC-Debugger

You do **not** need a TI CC-Debugger or SmartRF programmer. The ArduinoNRF board
package ships a `CCDebugger` library that turns the nRF52840 into a CC2530
programmer: it bit-bangs the chip's two-wire debug interface and writes flash via
the CC2530 DMA engine with read-back verification.

## One-Time Flash

1. Wire the debug link, as shown in [WIRING.md](WIRING.md):
   `P2.1 -> D8`, `P2.2 -> D9`, `RST -> D10`, `VCC -> 3V3`, `GND -> GND`.
   Tie `P2.0 (CFG1) -> GND` if you might also try Z-Stack later; it is harmless
   for the SDCC transceiver firmware.
2. In the Arduino IDE, open **File > Examples > ArduinoNRF-Zigbee >
   CC2530_FlashFirmware**.
3. Select your ArduinoNRF board and upload the sketch to the nRF52840.
4. Open Serial Monitor at 115200 baud. A successful flash looks like:

```text
Chip ID: 0xA524
Flashing transceiver firmware (2007 bytes)...
  flashing 100%
Flash complete and verified.
CC2530 is now running the transceiver firmware.
```

`Chip ID: 0xA5xx` confirms the debug link. "Verified" means read-back matched.
A full 256 KB Z-Stack image takes about 90 seconds; this small SDCC firmware is
near-instant.

## Runtime Check

Run `CC2530_Info` next. A healthy runtime UART link prints:

```text
CC2530 online. Firmware v0.3
Channel: 11
ping -> PONG
```

The host driver resynchronizes the CC2530 UART parser in `CC2530Radio.begin()`.
This handles the common case where the nRF board resets or re-enumerates during
upload while the CC2530 keeps running and may have seen partial UART bytes.

If the board uses a nice!nano-compatible bootloader with `SoftDevice: not found`
in `INFO_UF2.TXT`, select ArduinoNRF's no-SoftDevice bootloader option before
compiling. Otherwise the upload can succeed while the application is linked at
the wrong flash address and never starts.

## Debugger API

```cpp
#include <CCDebugger.h>
#include "my_firmware.h"  // const uint8_t FW[]; const unsigned int FW_LEN;

CCDebugger dbg(8, 9, 10); // DD, DC, RST

void setup() {
  dbg.begin();
  dbg.enterDebug();
  if ((dbg.chipID() >> 8) == 0xA5) {
    dbg.flashFirmware(FW, FW_LEN);
    dbg.run();
  }
}
```

| Method | Purpose |
|--------|---------|
| `CCDebugger(dd, dc, rst)` | Construct with Arduino pin numbers |
| `begin()` | Resolve pins to fast GPIO |
| `enterDebug()` | Reset the target into debug mode |
| `chipID()` | Read chip id; `0xA5xx` means CC2530 |
| `status()` | Read the debug status byte |
| `chipErase()` | Mass erase and clear debug lock |
| `flashFirmware(data, len, progress)` | Program and verify an image at flash address 0 |
| `run()` | Release debug pins and reset the chip |

## Custom Firmware

`flashFirmware()` takes any image that starts at flash address 0:

- An SDCC `.bin` like this library's transceiver. Convert it to a C array before
  embedding it in a sketch.
- A TI Z-Stack image. Convert the `.hex` to a flat binary first:
  `arm-none-eabi-objcopy -I ihex -O binary fw.hex fw.bin`.

Do not include the last flash page, `0x3F800..0x3FFFF`; it holds the lock bits.
Writing a debug-lock bit would lock the chip against re-flashing. Keep custom
images at or below `0x3F800`.

Some clone CC2530 modules will not boot stock Z-Stack because of clock/peripheral
startup quirks. This library's SDCC firmware is the reliable path on those; see
[WIRING.md](WIRING.md), "Clone clock quirk".
