# CC2530 Z-Stack ZNP firmware

The `CC2530ZnpRadio` host driver talks to a CC2530 that runs **TI Z-Stack ZNP**
firmware (the certified Zigbee PRO stack runs on the module; the nRF host drives
it over the MT API). This is the alternate backend to the default raw-802.15.4
SDCC firmware in `../cc2530/`. Unlike the SDCC firmware, the ZNP image is built
with **IAR Embedded Workbench for 8051** - it is not buildable with SDCC.

## What to build

Use TI's Z-Stack for CC2530 (Z-Stack 3.0.x / Home Automation 1.2.x both expose
the same MT API this driver uses). The ZNP application is the `znp` sample:

- Project: `Projects/zstack/ZNP/CC2530DB/znp.eww`
- Configuration: `CC2530-ProSecure` (or `CC2530F256`), UART transport.
- Confirm these MT subsystems are enabled (they back this driver):
  `MT_SYS_FUNC`, `MT_ZDO_FUNC`, `MT_AF_FUNC`, `MT_UTIL_FUNC`, and
  `MT_TASK` / `ZAPP_P1` so the MT API is served over the serial port.
- Transport: **UART**, 115200 baud, no flow control - matches `begin()`'s
  default and the wiring used by the SDCC firmware (Serial1).

Build in IAR and export the result as `cc2530znp.hex` next to this file.

## Flashing

Same path as the SDCC firmware: the ArduinoNRF board package's **built-in
CC-Debugger** flashes any TI CC253x image - no external TI programmer needed.
See `docs/FLASHING.md`. This only changes the **CC2530 module** image, never an
Arduino board bootloader.

## Verifying

Flash, wire the module to the nRF UART, and run `examples/CC2530Znp_Info`. It
should print the ping capabilities, the ZNP version, and a `ZDO state change -> 9`
as the coordinator forms the network.

## Status

The host driver (`src/modules/cc2530znp/`) is complete and compile-clean. On-air
bring-up is pending an IAR build of the ZNP image; once `cc2530znp.hex` is here
and flashed, `CC2530Znp_Info` is the first smoke test.
