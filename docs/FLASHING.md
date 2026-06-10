# Flashing the CC2530 with ArduinoNRF's built-in CC-Debugger

You do **not** need a TI CC-Debugger or SmartRF programmer. The ArduinoNRF board
package ships a **`CCDebugger` library** that turns the nRF52840 into a CC2530
programmer: it bit-bangs the chip's two-wire debug interface and writes flash via
the CC2530's DMA engine (read-back verified).

## One-time flash, step by step

1. **Wire the debug link** (see [WIRING.md](WIRING.md) §1): CC2530
   `P2.1→D8`, `P2.2→D9`, `RST→D10`, `VCC→3V3`, `GND→GND`. Tie `P2.0 (CFG1) → GND`
   if you might also try Z-Stack later (harmless for the SDCC firmware).
2. In the Arduino IDE, open **File ▸ Examples ▸ ArduinoNRF-Zigbee ▸
   CC2530_FlashFirmware**.
3. Select your ArduinoNRF board, **Upload** the sketch to the nRF52840.
4. Open **Serial Monitor @ 115200**. You should see:
   ```
   Chip ID: 0xA524
   Flashing transceiver firmware (1173 bytes)...
     flashing 100%
   Flash complete and verified.
   CC2530 is now running the transceiver firmware.
   ```
   `Chip ID: 0xA5xx` confirms the debug link; "verified" means the read-back
   checksum matched. (A full 256 KB image — e.g. a Z-Stack build — takes ~90 s;
   this small firmware is near-instant.)
5. Done. Load **CC2530_Info** (or Sniffer/Link) and use the module over UART.

## After flashing: runtime check

Run **CC2530_Info** next. A healthy runtime UART link prints:

```text
CC2530 online. Firmware v0.2
Channel: 11
ping -> PONG
```

The host driver resynchronizes the CC2530 UART parser in `CC2530Radio.begin()`.
This handles the common case where the nRF board resets or re-enumerates during
upload while the CC2530 keeps running and may have seen partial UART bytes.

If the board uses a nice!nano-compatible bootloader with `SoftDevice: not found`
in `INFO_UF2.TXT`, select ArduinoNRF's no-SoftDevice bootloader option
(`bootloader=promicroserialnosd`). Otherwise the upload can succeed while the
application is linked at the wrong flash address and never starts.

## Using the debugger API in your own sketch

```cpp
#include <CCDebugger.h>
#include "my_firmware.h"            // const uint8_t FW[]; const unsigned int FW_LEN;

CCDebugger dbg(8, 9, 10);           // DD, DC, RST  (Arduino pin numbers)

void setup() {
  dbg.begin();
  dbg.enterDebug();
  if ((dbg.chipID() >> 8) == 0xA5) {           // CC2530 present
    dbg.flashFirmware(FW, FW_LEN);             // erase + program + verify
    dbg.run();                                 // release debug, boot the chip
  }
}
```

### API summary (`CCDebugger`)
| Method | Purpose |
|--------|---------|
| `CCDebugger(dd, dc, rst)` | construct with the three Arduino pin numbers |
| `begin()` | resolve pins to fast GPIO; call once in `setup()` |
| `enterDebug()` | reset the target into debug mode |
| `chipID()` | read the chip id (`0xA5xx` = CC2530) |
| `status()` | read the debug status byte |
| `chipErase()` | mass-erase (also clears any debug lock) |
| `flashFirmware(data, len, progress=nullptr)` | erase + program an image that starts at flash address 0, then verify by read-back checksum; optional `progress(percent)` callback; returns `true` if verified |
| `run()` | release the debug pins and reset the chip so it runs its firmware |

## Flashing other firmware (incl. TI Z-Stack)

`flashFirmware()` takes any image that starts at flash address 0:
- An **SDCC `.bin`** (like this library's transceiver) — convert with `xxd -i`.
- A **TI Z-Stack** image — convert the `.hex` to a flat binary first
  (`arm-none-eabi-objcopy -I ihex -O binary fw.hex fw.bin`) and **do not include
  the last flash page** (0x3F800–0x3FFFF) — it holds the lock bits, and writing a
  debug-lock bit would lock the chip against re-flashing. Keep images ≤ 0x3F800.

> Note: some clone CC2530 modules will not boot stock Z-Stack (clock/peripheral
> quirks). This library's SDCC firmware is the reliable path on those; see
> [WIRING.md](WIRING.md) → *Clone clock quirk*.
