# NiusZigbee

Host-side drivers that let an **ArduinoNRF (nRF52840)** board drive external
**Zigbee / IEEE 802.15.4 radio modules** over a hardware UART. The companion to
the [ArduinoNRF](https://github.com/dunknowcoding/ArduinoNRF) board package —
kept as a **separate library** so the board package stays small.

It ships a complete, verified driver + firmware for the cheap **AliExpress
CC2530 module** (raw 802.15.4 **send / receive / sniff**) and, on top of it, a
growing **host-side Zigbee PRO stack** built natively on the nRF52840 — active
scan + join, AODV route discovery, multi-hop forwarding, AES-CCM* NWK **and**
APS security, end-to-end acked delivery, fragmentation, binding, persistence,
Trust-Center key-transport tooling, sleepy-end-device support, and network
management (Mgmt_Lqi, broadcast transaction table, PAN-ID conflict). No TI
Z-Stack required; a future ZNP backend can still live beside the raw driver.

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
   - **CC2530_BeaconJoin** — the full mesh demo: scan/join, AODV routing,
     multi-hop forwarding, NWK security, acked APS, Mgmt_Lqi, persistence
     (build the roles with `-DNIUS_ZIGBEE_THIS_NODE=0x0001/0x0002/0x0003`)

   Protocol self-tests (run on one board, e.g. board1 over a J-Link):
   - **CC2530_ApsSecurity** — APS key-transport crypto (AES-MMO, HMAC,
     specialized keys, CCM* Transport-Key envelope), 25/25
   - **CC2530_Fragmentation** / **CC2530_KeyTransport** / **CC2530_Binding** —
     APS fragment reassembly, TC key-transport frames, source binding table
   - **CC2530_BroadcastTable** — broadcast dedup + passive-ack table, 22/22
   - **CC2530_IndirectQueue** — sleepy-child Data Request + parent queue, 27/27
   - **CC2530_EndDeviceTimeout** — SED keep-alive negotiation, 18/18
   - **CC2530_PanIdConflict** — PAN-ID conflict detect + Network Report/Update, 16/16

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
parent bookkeeping. `CC2530_AssociationJoin` now exercises the first
over-the-air join path: MAC Association Request/Response followed by ZDO
Device_annce.

On top of that, `ZigbeeNetwork` now runs a real **active scan**: the joiner
broadcasts Beacon Requests across a channel list, coordinators/routers answer
with 802.15.4 beacons carrying the 15-byte Zigbee beacon payload
(`ZigbeeNwk::buildBeaconPayload`), received beacons land in a fixed candidate
table (`noteBeacon`), and `selectParent()` picks the best joinable parent by
permit-join, capacity, stack profile, LQI, and depth. `CC2530_BeaconJoin`
demonstrates the full flow — the joiner needs **no preconfigured PAN, channel,
or coordinator address** — with association retry, re-scan fallback, and a
`rejoinParent()` primitive for later parent-loss recovery.

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

## Zigbee PRO networking & security

Layered on the MAC base, these host-side pieces bring the stack a long way
toward Zigbee PRO. Each ships with a hardware self-test example.

- **Network security (NWK).** `ZigbeeSecurity` protects every NWK frame with
  AES-CCM* ENC-MIC-32 (Zigbee aux header, on-air level zeroing, frame-counter
  replay table), computed on the nRF52840 hardware AES block. The CCM* core is
  shared (`ZigbeeCcmStar.h`) with the APS layer.
- **APS reliability.** `ZigbeeApsRetransmit` + `ZigbeeApsDuplicateTable` give
  end-to-end acked delivery with retransmit and duplicate rejection;
  `ZigbeeApsFragment` splits/reassembles payloads too large for one frame.
- **Trust-Center key transport.** `ZigbeeApsKey` builds the APS Transport-Key /
  Request-Key / Switch-Key commands; `ZigbeeApsSecurity` encrypts them at the
  APS layer — AES-MMO hash, HMAC-MMO, the specialized key derivation
  (key-transport / key-load keys from the link key), and an APS CCM* envelope.
  `CC2530_BeaconJoin -DNIUS_ZIGBEE_SECURE_JOIN=1` wires it into the join so a
  joiner that holds only the default link key "ZigBeeAlliance09" is given the
  network key after associating — HW-verified end to end (the joiner decrypts
  the key, installs it, and runs the secured data plane with `mic=0`). This
  needs CC2530 firmware v0.4, which fixes a clone RXFIFO underrun that corrupted
  the tail of large (~80 B) received frames.
- **Routing & forwarding.** `ZigbeeRouting` runs AODV route discovery
  (RREQ/RREP, reverse routes) and the example forwards multi-hop unicast with
  per-hop re-encryption.
- **Network management.** ZDO `Mgmt_Lqi` / `Mgmt_Rtg`, the `ZigbeeBroadcastTable`
  (broadcast dedup + passive-ack), and `ZigbeePanIdConflict` (PAN-ID conflict
  detection + Network Report/Update).
- **Sleepy end devices.** `ZigbeeMac::buildDataRequest` + `ZigbeeIndirectQueue`
  (parent-side buffered-frame store) + `ZigbeeEndDeviceTimeout` (keep-alive
  negotiation). Setting the ack frame-pending bit on air needs CC2530 firmware
  support; the host-side queue/keep-alive logic is done.
- **Persistence & binding.** `ZigbeePersistence` serializes the network identity
  + frame counter (anti-replay across reboots); `ZigbeeBindingTable` stores
  source bindings with ZDO Bind/Unbind frames.

## Verified behavior

Hardware verified with two ArduinoNRF ProMicro nRF52840 boards, each wired to a
CC2530 module:

- `CC2530_FlashFirmware` detects `0xA5xx`, flashes the SDCC transceiver, and
  verifies read-back.
- `CC2530_Info` reports firmware `v0.4` and repeated `ping -> PONG`.
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
- `CC2530_NwkCommandLink` exchanges Route Request/Reply and Network Status under
  hardware filtering + Auto ACK + CCA TX.
- `CC2530_ZdoDiscovery` exchanges IEEE/NWK address, Active Endpoint, Simple
  Descriptor, and Match Descriptor requests/responses between two boards under
  hardware filtering + Auto ACK + CCA TX.
- `CC2530_AssociationJoin` runs board1 as a coordinator with permit join open,
  accepts board2's MAC Association Request, assigns a short address, and receives
  board2's ZDO Device_annce.
- `CC2530_BeaconJoin` runs a parameterless joiner that scans channels 11/15/20/25
  with Beacon Requests, hears the coordinator's Zigbee beacon on channel 15
  (`pan=0x1A62 depth=0 permit=yes`), selects it as parent, associates on the
  first attempt (addr `0x0001`), and announces — while the coordinator answers
  beacon requests under hardware filtering. The scan also met two foreign
  production Zigbee networks on channel 25 (`permit=no`) and correctly skipped
  them as parents.
- After joining, both nodes broadcast NWK Link Status every 15 s: both ends
  settle at `in=1 out=1` link costs (outgoing cost read back from the entry
  naming our own address in the neighbor's report), and silencing one node
  makes the other print `aged out 1 stale router neighbor(s)` after three
  missed periods. A stale parent instead reports parent-loss and re-enters
  joining via `rejoinParent()`.
- **Multi-hop, three boards (A coordinator, B router, C end).** With A and C
  set to ignore each other's frames (a bench stand-in for being out of radio
  range), C scans, finds only router B's beacon, and joins **through** B from
  B's own address pool (`addr=0x0031`). C then routes an encrypted ping to
  the coordinator: B forwards `0x0031->0x0000`, A decrypts it (`mic=0`) and
  replies, and the pong comes back `A->B->C` over the reverse route each node
  learned from the traffic — a full `C->B->A->B->C` encrypted round trip on
  hardware. Build the three roles with
  `-DNIUS_ZIGBEE_THIS_NODE=0x0001` (A), `0x0002` (B), `0x0003` (C).
- **APS end-to-end acknowledged delivery.** The end device's data plane sends
  an acked APS frame to the coordinator over the routed mesh; the coordinator
  replies with an APS ACK (same APS counter) over the reverse route, and the
  sender retransmits until the ACK arrives or the retry budget runs out. On
  the 3-board line this lifts confirmed end-to-end delivery from ~11% (a raw
  routed ping with no recovery) to ~55%, and the sender's
  `aps[q=.. ok=.. rtx=.. drop=..]` status reports exactly which frames made it.
- The joiner then discovers a route to the coordinator (Route Request
  broadcast -> unicast Route Reply, reverse route recorded on the way) and
  pings over it every 10 s: routed `route ping`/`route pong` round trips run
  at 100%. The full self-healing loop is verified: silencing the coordinator
  ages the parent out at 54 s, the joiner's rejoin attempts fail, it falls
  back to scanning (correctly skipping up to six foreign `permit=no` networks
  heard on channel 25), and when the coordinator returns it rejoins on the
  first attempt and rebuilds the route.
- The entire flow above also runs with **NWK security enabled**: every NWK
  frame (announce, link status, route discovery, routed data) carries the
  Zigbee auxiliary header and is AES-CCM* ENC-MIC-32 protected, computed on
  the nRF52840's hardware AES block. Status lines report
  `sec[tx=18 rx=5 mic=0 rpl=0]`-style statistics. A joiner built with
  `-DNIUS_ZIGBEE_WRONG_KEY=1` still associates at the MAC level but every
  NWK frame it sends is MIC-rejected (`mic=` climbs, `rx=` freezes, its
  link costs stay 0/0).
- `CC2530_BeaconJoin` scales to a **multi-hop A-B-C line**: build the three
  boards with `-DNIUS_ZIGBEE_THIS_NODE=0x0001/0x0002/0x0003` for
  coordinator / router / end device. Routers answer beacon requests and
  accept children from their own address pool, and unicast NWK data destined
  elsewhere is forwarded to the route's next hop (radius-1, re-encrypted per
  hop), so the end device reaches the coordinator through the router. An
  optional range-simulation ignore list lets three co-located radios form a
  real 2-hop topology. Verified on hardware so far: the router joins the
  coordinator and becomes a parent (`children=1`, encrypted Link Status,
  `mic=0`); the full A-B-C routed ping is the remaining bring-up step.
- **ZDO Mgmt_Lqi network mapping.** The end device queries the coordinator's
  neighbor table; the response is carried as an APS-acked frame with single-in-
  flight retransmit (an earlier naive version caused a retransmit storm). On a
  clean 1-hop link the request is answered on the first try every cycle and the
  acked APS data plane runs at 100% (`q=44 ok=44 drop=0`).
- **Protocol self-tests on hardware** (one board, J-Link): APS key-transport
  security 25/25 (`CC2530_ApsSecurity`), broadcast transaction table 22/22
  (`CC2530_BroadcastTable`), indirect transmission 27/27 (`CC2530_IndirectQueue`),
  End Device Timeout 18/18 (`CC2530_EndDeviceTimeout`), PAN-ID conflict 16/16
  (`CC2530_PanIdConflict`), plus fragmentation / key-transport / binding.
- Promiscuous examples can still show unrelated 802.15.4 traffic on the channel;
  filtered examples program PAN/short/IEEE addresses before exchanging frames.

## Current stack boundary

NiusZigbee now implements a large, hardware-verified subset of Zigbee PRO on top
of the SDCC CC2530 MAC/PHY backend: active scan + parent selection, MAC
association join/rejoin, neighbor aging, AODV route discovery + multi-hop
forwarding, NWK **and** APS AES-CCM* security, end-to-end acked delivery,
fragmentation, ZDO discovery + network management (Mgmt_Lqi/Rtg), binding,
persistence, Trust-Center key-transport tooling, broadcast transaction table,
sleepy-end-device queue + keep-alive, and PAN-ID conflict frames.

It is **not yet certified Zigbee PRO**. Remaining work includes: finishing the
on-air secure-join key install, many-to-one routing + source routing (Route
Record), setting the ack frame-pending bit (needs CC2530 firmware), full ZCL
cluster libraries + groups, install codes, and APS-layer encrypted application
data. A future ZNP / Z-Stack backend can still live beside the raw driver. See
the milestone-by-milestone status in
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
