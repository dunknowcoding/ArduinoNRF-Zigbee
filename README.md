# NiusZigbee

Host-side drivers that let an **ArduinoNRF (nRF52840)** board drive external
**Zigbee / IEEE 802.15.4 radio modules** over a hardware UART. The companion to
the [ArduinoNRF](https://github.com/dunknowcoding/ArduinoNRF) board package —
kept as a **separate library** so the board package stays small.

Today it ships a complete, verified driver + firmware for the cheap
**AliExpress CC2530 module**, giving you raw 802.15.4 **send / receive / sniff**.
The architecture is built to grow to more modules and a future full‑Zigbee
(Z‑Stack) backend.

## How it works

```
   ArduinoNRF (nRF52840)                         CC2530 module
   ┌───────────────────┐    UART  115200    ┌────────────────────┐
   │ CC2530Radio (this │  D0 ─► P0.2 (RX)   │  SDCC 802.15.4     │
   │ library)          │  D1 ◄─ P0.3 (TX)   │  transceiver fw    │──))) 2.4 GHz
   │                   │                    │                    │
   │ CCDebugger (board │  D8 ─► P2.1 (DD)   │  (flashed once via │
   │ package) flashes  │  D9 ─► P2.2 (DC)   │   the debug port)  │
   │ the module        │  D10─► RST         │                    │
   └───────────────────┘                    └────────────────────┘
```

- **Flashing** the module needs no external programmer — the ArduinoNRF board
  package's built‑in **`CCDebugger`** does it over the 2‑wire debug port.
- **At runtime** the nRF talks to the module over UART with a small framed
  protocol; the CC2530 does the actual radio PHY/MAC.

## Install

1. Install the **ArduinoNRF board package** (it provides the board + the
   `CCDebugger` flasher library).
2. Install **this library**: *Sketch ▸ Include Library ▸ Add .ZIP Library…*, or
   clone into your Arduino `libraries/` folder, or via Library Manager once
   published.

In Arduino Library Manager it is published as **NiusZigbee**. The GitHub
repository keeps the historical `ArduinoNRF-Zigbee` name because it is developed
alongside the ArduinoNRF core.

## Quick start

1. **Wire it up** — debug pins (D8/D9/D10) and UART pins (D0/D1). See
   [docs/WIRING.md](docs/WIRING.md). 3.3 V only. **P2.0 (CFG1) is not used by this
   library's firmware** (it's a TI Z-Stack-only strap) — leave it floating or tie
   it to GND, either works. Grounding it is recommended only to future-proof for
   a later Z-Stack flash.
2. **Flash the module firmware once** — open *Examples ▸ ArduinoNRF-Zigbee ▸
   **CC2530_FlashFirmware*** and upload. It uses the built‑in CC‑Debugger; details
   in [docs/FLASHING.md](docs/FLASHING.md).
3. **Use it** — open one of:
   - **CC2530_Info** — confirm the link, read the firmware version
   - **CC2530_Sniffer** — promiscuous 802.15.4 packet sniffer
   - **CC2530_Link** — a two‑node radio link (flash onto two setups)

```cpp
#include <CC2530Radio.h>
CC2530Radio radio;                 // uses Serial1 (D0/D1)

void onFrame(const uint8_t* p, uint8_t n, int8_t rssi, uint8_t lqi) { /* ... */ }

void setup() {
  radio.begin(11);                 // 115200 UART, channel 11
  radio.onReceive(onFrame);
}
void loop() {
  radio.poll();
  radio.send((const uint8_t*)"hi", 2);
}
```

## API (`CC2530Radio`)

| Method | Purpose |
|--------|---------|
| `begin(channel=11, baud=115200)` | open the UART, ping, select channel |
| `ping()` / `firmwareVersion()` | liveness check / firmware version |
| `setChannel(11..26)` / `channel()` | select / read the 802.15.4 channel |
| `setPromiscuous(bool)` | receive all frames (sniffer) vs filtered |
| `send(payload, len)` | transmit a raw 802.15.4 frame (radio adds FCS) |
| `onReceive(cb)` + `poll()` | deliver received frames to your callback |

`send()` carries **raw 802.15.4** payloads — perfect for CC2530↔CC2530 links and
sniffing. Talking to real Zigbee devices needs a proper MAC header (and, for full
Zigbee networking, the future Z‑Stack backend).

## Extending to new modules

Drop a driver in `src/modules/<NAME>/`, its firmware in
`extras/firmware/<name>/`, a forwarder header in `src/`, and examples under
`examples/`. See [src/ZigbeeModule.h](src/ZigbeeModule.h). The built‑in
`CCDebugger` flashes any TI CC253x image (SDCC or Z‑Stack).

## Firmware

The CC2530 transceiver firmware source (SDCC) and prebuilt binary live in
[extras/firmware/cc2530/](extras/firmware/cc2530/). Build notes:
[extras/firmware/cc2530/BUILD.md](extras/firmware/cc2530/BUILD.md).

## License

Apache 2.0 — see [LICENSE](LICENSE). Author: **dunknowcoding** (YouTube:
*NiusRobotLab*). If you use it in a product, please credit the original author and
note your changes.
