# Wiring the CC2530 module to an ArduinoNRF board

This covers the common AliExpress CC2530 "Zigbee" module, the small blue board
with an SMA antenna, connected to an ArduinoNRF ProMicro nRF52840.

> **3.3 V only.** The CC2530 is not 5 V tolerant. Power it from the ProMicro
> `3V3` pad and share `GND`.

## Module Pinout

The common board uses two 6x2 headers:

| Left header |        | Right header |        |
|-------------|--------|--------------|--------|
| VCC         | GND    | P2.2 (DC)    | GND    |
| VCC         | RST    | P2.0 (CFG1)  | P2.1 (DD) |
| P0.1        | P0.0   | P1.6         | P1.7   |
| P0.3 (TX)   | P0.2 (RX) | P1.4      | P1.5   |
| P0.5        | P0.4   | P1.2         | P1.3   |
| P0.7        | P0.6   | P1.0         | P1.1   |

Pins used by this library:

- `VCC`, `GND`, `RST` - power and reset
- `P2.1 (DD)`, `P2.2 (DC)` - two-wire debug, used for flashing
- `P2.0 (CFG1)` - transport-select strap for Z-Stack compatibility
- `P0.2 (RX)`, `P0.3 (TX)` - runtime UART

## Flashing Link

| CC2530 | ProMicro nRF52840 |
|--------|-------------------|
| P2.1 (DD) | D8 |
| P2.2 (DC) | D9 |
| RST       | D10 |
| VCC       | 3V3 |
| GND       | GND |

These are the defaults used by `CCDebugger dbg(8, 9, 10);`.

## Runtime UART Link

| CC2530 | ProMicro nRF52840 |
|--------|-------------------|
| P0.2 (RX) | D0, Serial1 TX |
| P0.3 (TX) | D1, Serial1 RX |
| VCC       | 3V3 |
| GND       | GND |

D8/D9/D10 and D0/D1 are different pins, so you can leave both sets connected:
flash once, then run the runtime examples. Verified direction is
`D0 -> CC2530 P0.2 (RX)` and `D1 <- CC2530 P0.3 (TX)`.

## Notes

- `P2.0 (CFG1)`: the SDCC transceiver firmware configures UART directly, so CFG1
  can be left unconnected. TI Z-Stack ZNP firmware samples CFG1 at boot; for
  that path tie `P2.0 -> GND` to select UART with no flow control. Tying CFG1 to
  GND is harmless for the SDCC firmware.
- Hardware flow control is not used. Leave `P0.4/P0.5` unconnected for the SDCC
  firmware.
- Clone clock quirk: many clone CC2530 modules will not start their 32 MHz
  crystal via TI stock Z-Stack's startup sequence. This library's SDCC firmware
  starts the crystal through `CLKCONCMD`, which has worked on the tested clone
  modules.
- No level shifter is needed when both boards use 3.3 V logic.
