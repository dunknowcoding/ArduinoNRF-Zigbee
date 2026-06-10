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
   - **CC2530_Link** — a two-node radio link (flash onto two setups)
   - **CC2530_MacLink** — a two-node short-address MAC data-frame link
   - **CC2530_NwkLink** — a two-node Zigbee NWK data-frame link
   - **CC2530_ZclLink** — a two-node APS/ZCL On/Off command-frame link
   - **CC2530_OnOffCluster** — a tiny two-node On/Off cluster behavior demo
   - **CC2530_ClusterNode** — reusable Basic + On/Off cluster node demo
   - **CC2530_ReportingNode** — Configure Reporting + Report Attributes demo

> Board layout note: some nice!nano-compatible bootloaders report
> `SoftDevice: not found` in `INFO_UF2.TXT`. For those boards, select the
> ArduinoNRF no-SoftDevice bootloader option (`bootloader=promicroserialnosd`) so
> the sketch is linked at `0x1000`. A SoftDevice layout can upload successfully
> but never start.

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
| `setAddress(pan, short, ieee)` | program CC2530 PAN ID, short address, and IEEE address registers |
| `configureMac(flags, retries)` / `getMacInfo(info)` | control/read hardware filtering, Auto ACK, CCA TX, and retry count |
| `setTxPowerRaw(value)` | write the CC2530 TXPOWER register |
| `send(payload, len)` | transmit a raw 802.15.4 frame (radio adds FCS) |
| `sendWithRetries(payload, len, retries)` / `lastTxAttempts()` | transmit with per-call retry count and read attempt count |
| `sendData(pan, dst, src, payload, len)` | build + transmit a short-address 802.15.4 data frame |
| `sendNwkData(pan, macDst, macSrc, nwkDst, nwkSrc, payload, len)` | build + transmit a simple Zigbee NWK data frame |
| `sendNwkCommand(...)` | build + transmit a Zigbee NWK command frame |
| `sendApsData(...)` | build + transmit a simple unicast Zigbee APS data frame |
| `sendZdoCommand(...)` | build + transmit a Zigbee Device Profile command on endpoint 0 |
| `sendZclCommand(...)` | build + transmit a basic ZCL command frame |
| `onReceive(cb)` + `poll()` | deliver received frames to your callback |
| `onDataReceive(cb)` + `poll()` | parse and deliver short-address MAC data frames |
| `onNwkReceive(cb)` + `poll()` | parse and deliver simple Zigbee NWK data frames |
| `onNwkCommandReceive(cb)` + `poll()` | parse and deliver simple Zigbee NWK command frames |
| `onApsReceive(cb)` + `poll()` | parse and deliver simple Zigbee APS data frames |
| `onZdoReceive(cb)` + `poll()` | deliver endpoint 0 / profile 0 Zigbee Device Profile frames |
| `onZclReceive(cb)` + `poll()` | parse and deliver basic ZCL command frames |

`send()` carries **raw 802.15.4** payloads — perfect for CC2530↔CC2530 links and
sniffing. Talking to real Zigbee devices needs a proper MAC header (and, for full
Zigbee networking, the future Z‑Stack backend).

`sendData()` and `onDataReceive()` add the first reusable stack layer above raw
radio I/O: IEEE 802.15.4 data frames with PAN ID, 16-bit source/destination
addresses, sequence number, optional ACK request, and parsed RSSI/LQI metadata.
This is still not Zigbee PRO joining or ZCL control, but it is the MAC envelope
that future NWK / APS / ZCL code can build on.

`sendNwkData()` and `onNwkReceive()` add the next frame layer: a minimal Zigbee
NWK data frame with destination/source short address, radius, sequence number,
and payload. Optional NWK fields such as security, multicast, source routing,
and IEEE address extension are intentionally not accepted by this helper yet.

`sendNwkCommand()` and `onNwkCommandReceive()` add the first NWK command path:
Route Request, Route Reply, Network Status, Route Record, Leave, and Rejoin
payload builders/parsers. These are command-frame tools, not a full route
discovery or rejoin state machine yet.

`sendApsData()` / `sendZclCommand()` add unicast APS endpoint/profile/cluster
framing and basic ZCL command-frame construction. They are useful for exercising
frame layout and application-layer parsing between two library nodes; they do
not implement Zigbee device discovery, binding, reporting, attribute storage, or
cluster behavior.

`ZigbeeZdo` adds the first Zigbee Device Object payload helpers for endpoint 0:
NWK/IEEE address, Active Endpoint, Simple Descriptor, and Match Descriptor
requests/responses. `CC2530_ZdoDiscovery` uses those helpers with CC2530
hardware filtering and Auto ACK enabled, which is the next step from private
two-node APS/ZCL frames toward real Zigbee PRO discovery.

`ZigbeeDeviceObject` is a small static descriptor store that can answer those
ZDO requests from a sketch-provided endpoint table. It is the first reusable
piece of local device identity, ahead of later join state, binding tables, and
persistent network storage.

`ZigbeeNeighborTable` and `ZigbeeRouteTable` provide fixed-storage local network
tables for the next Zigbee PRO steps. They do not run routing by themselves, but
they give future association, parent selection, route discovery, and route aging
code a no-heap storage base.

`ZigbeeNetwork`, `ZigbeePermitJoin`, and `ZigbeeAddressAllocator` add the first
local join-state primitives: coordinator/joined-device identity, permit-join
timing, short-address allocation, child acceptance into the neighbor table, and
parent bookkeeping. The actual over-the-air association command exchange is a
later layer.

`ZigbeeZcl` also includes small helpers for Read Attributes payloads, Default
Response payloads, boolean/uint8 attribute records, boolean reports, and applying
On/Off cluster commands to a local state variable. `CC2530_OnOffCluster` shows
how to combine those helpers into a minimal behavior loop.

`ZigbeeOnOffCluster` and `ZigbeeBasicCluster` are the first reusable behavior
helpers. They can build ZCL responses for On/Off state changes, OnOff reads, and
Basic cluster reads such as ManufacturerName, ModelIdentifier, and PowerSource.

`ZigbeeBoolReportScheduler` adds the first small reporting helper: it stores the
configured min/max interval for a boolean attribute, detects changes, and builds
ZCL Report Attributes frames when a sketch should publish a report.

`begin()` also resynchronizes the CC2530 firmware's framed UART parser before
the first ping. This matters after host uploads/resets because the CC2530 may
keep running while the nRF resets, leaving the module mid-frame.

The bundled SDCC firmware is now a small MAC/PHY co-processor: the nRF can set
the CC2530 hardware PAN/short/IEEE address registers, enable frame filtering,
use the CC2530 Auto ACK path, request CCA transmit, and configure retry count.
That is still below full Zigbee PRO, but it removes the earlier all-promiscuous
assumption and gives the future join/routing/security work a real MAC base.

## Verified behavior

Hardware verified with two ArduinoNRF ProMicro nRF52840 boards, each wired to a
CC2530 module:

- `CC2530_FlashFirmware` detects `0xA5xx`, flashes the SDCC transceiver, and
  verifies read-back.
- `CC2530_Info` reports firmware `v0.2` and repeated `ping -> PONG`.
- `CC2530_MacControl` writes PAN/short/IEEE address, enables filtering +
  Auto ACK + CCA TX with three retries, and reads the settings back.
- `CC2530_Link` on two boards shows `TX "hello N" ok` and reciprocal
  `RX (... dBm): hello N` frames on channel 11.
- `CC2530_MacLink` builds standards-shaped short-address MAC data frames and
  filters received frames by PAN ID / destination short address in the sketch.
- `CC2530_NwkLink` wraps those MAC frames with simple Zigbee NWK data frames and
  shows reciprocal `NWK RX ... payload="nwk hello N"` traffic on channel 11.
- `CC2530_ZclLink` wraps ZCL On/Off Toggle command frames inside APS/NWK/MAC and
  shows reciprocal `ZCL RX ... cmd=0x02` traffic on channel 11.
- `CC2530_OnOffCluster` applies Toggle to a local OnOff state, replies with ZCL
  Default Response, and answers Read Attributes for the OnOff attribute.
- `CC2530_ClusterNode` dispatches incoming ZCL frames through reusable
  `ZigbeeOnOffCluster` / `ZigbeeBasicCluster` helpers and answers Basic +
  On/Off reads.
- `CC2530_ReportingNode` accepts Configure Reporting for OnOff.OnOff and emits
  Report Attributes on state change / max interval.
- `CC2530_ZdoDiscovery` compiles for two nodes and board1 uploads/runs with
  endpoint 0 discovery enabled through `ZigbeeDeviceObject`. Board2 was not
  present as a connected USB serial device during the final pass, so two-board
  ZDO RX/RSP validation should be rerun after reconnecting board2.
- Because the examples use promiscuous receive mode, unrelated 802.15.4 traffic
  on the channel can appear as noisy frames; the `hello N` payloads are the link
  confirmation.

## Current stack boundary

NiusZigbee currently implements an SDCC CC2530 MAC/PHY backend plus small
short-address MAC, Zigbee NWK data/command and APS data-frame helpers, ZDO
discovery payload helpers, static local Device Object descriptors, fixed
neighbor/route tables, basic ZCL command-frame helpers, local network-state
helpers, tiny reusable Basic / OnOff behavior helpers, and a boolean report scheduler,
not a full Zigbee PRO stack. Missing full-stack pieces include association and
over-the-air join state machines, actual neighbor aging/routing protocols, full
ZCL cluster libraries, binding/groups, persistent reporting tables, Trust Center
behavior, install codes, NWK/APS security, and Zigbee PRO route discovery/repair.
A future ZNP / Z-Stack backend can live beside the raw driver. See
[docs/STACK_ROADMAP.md](docs/STACK_ROADMAP.md).

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
