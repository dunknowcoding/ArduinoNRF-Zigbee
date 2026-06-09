# Wiring the CC2530 module to an ArduinoNRF board

This covers the common **AliExpress CC2530 "Zigbee" module** (the small blue board
with an SMA antenna) connected to an **ArduinoNRF ProMicro nRF52840**.

> ⚠️ **3.3 V only.** The CC2530 is **not** 5 V tolerant. Power it from the
> ProMicro's **3V3** pad and share **GND**.

## Module pinout (the two 6×2 headers)

| Left header |        | Right header |        |
|-------------|--------|--------------|--------|
| VCC         | GND    | P2.2 (DC)    | GND    |
| VCC         | **RST**| P2.0 (CFG1)  | P2.1 (DD) |
| P0.1        | P0.0   | P1.6         | P1.7   |
| P0.3 (TX)   | P0.2 (RX) | P1.4      | P1.5   |
| P0.5        | P0.4   | P1.2         | P1.3   |
| P0.7        | P0.6   | P1.0         | P1.1   |

The pins this library uses:
- **VCC / GND / RST** – power and reset
- **P2.1 (DD)**, **P2.2 (DC)** – two-wire debug (flashing only)
- **P2.0 (CFG1)** – transport-select strap (see notes)
- **P0.2 (RX)**, **P0.3 (TX)** – UART for runtime control

## Two link sets (you can wire both at once)

### 1) Flashing link — used once, by ArduinoNRF's built-in CC-Debugger
| CC2530 | ProMicro nRF52840 |
|--------|-------------------|
| P2.1 (DD)  | **D8**  |
| P2.2 (DC)  | **D9**  |
| RST        | **D10** |
| VCC        | 3V3 |
| GND        | GND |

(These are the defaults used by `CCDebugger dbg(8, 9, 10);` and every flashing
example. Any free GPIOs work — just match them in the sketch.)

### 2) Runtime UART link — used by CC2530_Info / Sniffer / Link
| CC2530 | ProMicro nRF52840 |
|--------|-------------------|
| P0.2 (RX) | **D0** (Serial1 **TX**) |
| P0.3 (TX) | **D1** (Serial1 **RX**) |
| VCC       | 3V3 |
| GND       | GND |

D8/D9/D10 (debug) and D0/D1 (UART) are different pins, so **leave both sets
connected** — flash once, then the runtime examples just work.

The ProMicro silk-screen labels are used here: **D0 is Serial1 TX** and **D1 is
Serial1 RX** in the ArduinoNRF ProMicro variant. Verified runtime direction:
`D0 -> CC2530 P0.2 (RX)` and `D1 <- CC2530 P0.3 (TX)`.

## Important notes

- **P2.0 (CFG1):** the SDCC transceiver firmware in this library configures its
  UART directly, so CFG1 can be left unconnected. **TI Z-Stack ZNP firmware**,
  however, samples CFG1 at boot — for that you must tie **P2.0 → GND** to select
  "UART, no flow control". Tying P2.0 → GND is harmless either way, so do it if
  unsure.
- **Hardware flow control:** not used. Leave P0.4/P0.5 (CTS/RTS) unconnected for
  the SDCC firmware.
- **Clone clock quirk:** many clone CC2530 modules won't start their 32 MHz
  crystal via the sequence TI's stock Z-Stack uses, so stock Z-Stack can hang at
  boot on them. This library's SDCC firmware starts the crystal a different way
  (via `CLKCONCMD`) that works on those clones — which is why it runs where stock
  Z-Stack may not.
- **No level shifter** needed: both sides are 3.3 V logic.
