# CC2530 Z-Stack ZNP firmware

The `CC2530ZnpRadio` host driver talks to a CC2530 that runs **TI Z-Stack ZNP**
firmware (the certified Zigbee PRO stack runs on the module; the nRF host drives
it over the MT API). This is the alternate backend to the default raw-802.15.4
SDCC firmware in `../cc2530/`. Unlike the SDCC firmware, the ZNP image is built
with **IAR Embedded Workbench for 8051** - it is not buildable with SDCC.

## Shortcut: use the prebuilt image (no IAR build needed)

Z-Stack 3.0.2 **ships a complete, ready-to-flash CC2530 ZNP image** - you do not
have to build anything:

```text
<Z-Stack 3.0.2>\Projects\zstack\ZNP\CC253x\dev\CC2530ZNP-with-SBL.hex
```

It is a full-flash Intel-HEX image: address span 0x00000-0x3FFFF (the whole
256 KB CC2530 flash), ~242 KB of actual data = the ZNP application + the serial
bootloader. Flash it straight to the module with the built-in CCDebugger
(`CCDebugger::flashFirmware` already programs full 256 KB images across all 8
banks, ~90 s). Convert the Intel HEX to a gap-filled 256 KB raw binary first
(the flasher takes raw bytes starting at flash address 0).

> The prebuilt image is TI-copyrighted and is **not** vendored into this repo -
> install Z-Stack 3.0.2 from TI to obtain it.

**Note on building from source:** IAR EW 8051 8.0 compiles all ZNP sources fine,
but on a freshly-installed toolchain the **link step (`xlink`) can stall** on an
LMS license checkout (the compiler runs on a grace token; the linker blocks). If
you need to build from source, open the IAR IDE once to activate the license,
then `IarBuild.exe ...\CC2530.ewp -build ZNP-without-SBL`. The prebuilt image
above avoids this entirely.

## What to build (only if you want a from-source / without-SBL image)

The driver speaks the standard Z-Stack MT API, so any of TI's Z-Stack-CC2530
releases work (Z-Stack 3.0.2 or Z-Stack Home 1.2.2a both expose the SYS / ZDO /
AF / UTIL MT commands this driver uses). The ZNP application is shipped as a
ready sample - you build it as-is, no app code to write.

1. **Install** TI Z-Stack-CC2530 (the installer drops the tree under
   `C:\Texas Instruments\Z-Stack ...`) and **IAR EW for 8051** (8.30+).
2. **Open** the ZNP workspace in IAR:
   `Projects/zstack/ZNP/CC2530DB/znp.eww`.
3. **Pick the build configuration** for the part: `CC2530F256` (the common
   AliExpress module). Use a *secure* variant (e.g. `...ProSecure`) so AES is on.
4. **Confirm the MT transport + subsystems** in the project's predefined symbols
   (Project > Options > C/C++ Compiler > Preprocessor). The stock ZNP config
   already sets these; verify they are present because the driver depends on them:
   - `MT_TASK`, `MT_SYS_FUNC`, `MT_ZDO_FUNC`, `MT_ZDO_MGMT_FUNC`, `MT_AF_FUNC`,
     `MT_UTIL_FUNC` - serve the SYS/ZDO/AF/UTIL commands.
   - `ZAPP_P2` (UART transport to the app/host) and the default
     `MT_UART_DEFAULT_BAUDRATE = HAL_UART_BR_115200`, **no** flow control -
     matches `begin()`'s 115200 and the same wiring as the SDCC firmware (the nRF
     `Serial1` <-> CC2530 P0.2/P0.3).
5. **Build** (F7). The image lands in the configuration's `Exe/` folder.
6. **Copy** the resulting hex here as `cc2530znp.hex`.

> Tip: the ZNP sample also exposes `MT_UART` over the debug-UART pins; make sure
> the configuration routes MT to the same UART you have wired to the nRF.

## Flashing

Same path as the SDCC firmware: the ArduinoNRF board package's **built-in
CC-Debugger** flashes any TI CC253x image - no external TI programmer needed.
See `docs/FLASHING.md`. This only changes the **CC2530 module** image, never an
Arduino board bootloader. The full 256 KB with-SBL image flashes and verifies in
~90 s via `CCDebugger::flashFirmware` (it programs all 8 flash banks - there is no
small-image size cap).

## Required CFG wiring (UART, no flow control)

TI's stock ZNP binary reads two config pins at power-up and is compiled with
**mandatory hardware UART flow control**. To run it over the same plain RX/TX
wiring as the SDCC firmware (no RTS/CTS), add **two jumpers to GND**:

| CC2530 pin | Tie to | Why |
| --- | --- | --- |
| **P2.0** (CFG1) | **GND** | selects UART transport (high = SPI -> silent on UART at every baud) |
| **P0.4** (CT/CTS) | **GND** | satisfies the mandatory flow control so the ZNP transmits |

CFG pins are sampled at reset, so pulse the CC2530 RST (wired to **D10**, the
CCDebugger RST line) after adding the jumpers, or power-cycle.

## Serial-bootloader (SBL) startup delay

The prebuilt `...-with-SBL` image runs a serial bootloader that **waits
`SBL_WAIT_TIME` (~60 s)** for a host force-boot before it jumps to the ZNP app.
So the app is not responsive immediately after reset. The host driver's
`waitUntilResponsive(timeoutMs)` polls `SYS_PING` across that window; do not send
other `0xFE`-prefixed traffic during it (the SB protocol shares SOF `0xFE`). A
from-source *without-SBL* build boots the app immediately (no wait).

## Verified on hardware (board1, 2026-06-20)

`CC2530Znp_Info` over the MT API on the ZNP-flashed CC2530:

- `SYS_PING` -> capabilities **0x0779**
- `SYS_VERSION` -> **Z-Stack 2.7.2** (product = ZNP, transport rev 2)

This validates the host driver's MT transport/framing against real Z-Stack
firmware. (Higher-level `ZDO_STARTUP_FROM_APP` network formation needs follow-up
tuning - SYS/UTIL transport itself is confirmed.)
