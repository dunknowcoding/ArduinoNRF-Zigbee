# CC2530 Z-Stack ZNP firmware

The `CC2530ZnpRadio` host driver talks to a CC2530 that runs **TI Z-Stack ZNP**
firmware (the certified Zigbee PRO stack runs on the module; the nRF host drives
it over the MT API). This is the alternate backend to the default raw-802.15.4
SDCC firmware in `../cc2530/`.

This page covers how to **obtain, flash, and run** the ZNP firmware. The
ArduinoNRF-Zigbee working artifacts are the host driver (`src/modules/cc2530znp/`)
and the `CC2530Znp_Info` / `CC2530Znp_Mt` examples - those ship in this repo. The
ZNP *firmware image itself* is TI's and is obtained from TI, below.

## Get the firmware image (prebuilt, from TI)

TI's **Z-Stack 3.0.2** ships a complete, ready-to-flash CC2530 ZNP image - no
toolchain needed. After installing Z-Stack 3.0.2 from TI, take:

```text
<Z-Stack 3.0.2>\Projects\zstack\ZNP\CC253x\dev\CC2530ZNP-with-SBL.hex
```

It is a full-flash Intel-HEX image: address span 0x00000-0x3FFFF (the whole
256 KB CC2530 flash), ~242 KB of data = the ZNP application + serial bootloader.

> The image is **TI-copyrighted and is not redistributed in this repo** - get it
> from TI's Z-Stack package.

## Flashing

Same path as the SDCC firmware: the ArduinoNRF board package's **built-in
CC-Debugger** flashes any TI CC253x image - no external TI programmer needed.
See `docs/FLASHING.md`. This only changes the **CC2530 module** image, never an
Arduino board bootloader.

`CCDebugger::flashFirmware(data, len)` takes raw bytes starting at flash address 0,
so first convert the Intel-HEX into a **gap-filled 256 KB raw image** (fill unused
addresses with 0xFF), then flash it exactly like `examples/CC2530_FlashFirmware`
does for the SDCC image. The full 256 KB programs and verifies in ~90 s across all
8 flash banks - there is no small-image size cap.

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
other `0xFE`-prefixed traffic during it (the SB protocol shares SOF `0xFE`).

## Verified on hardware (board1, 2026-06-20)

`CC2530Znp_Info` over the MT API on the ZNP-flashed CC2530:

- `SYS_PING` -> capabilities **0x0779**
- `SYS_VERSION` -> **Z-Stack 2.7.2** (product = ZNP, transport rev 2)

This validates the host driver's MT transport/framing against real Z-Stack
firmware. (Higher-level `ZDO_STARTUP_FROM_APP` network formation needs follow-up
tuning - SYS/UTIL transport itself is confirmed.)
