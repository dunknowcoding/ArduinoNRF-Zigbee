# Verified behavior

Every claim here was confirmed on hardware with up to **five** nRF52840 boards
(ProMicro clones and nice!nano v2), each wired to a CC2530 module running the
bundled **v0.4** SDCC transceiver firmware. Board names are used consistently:

| Board | Typical role in the demos | Notes |
|-------|---------------------------|-------|
| **board1** | Coordinator / Trust Center | ProMicro clone; has a J-Link, so it also runs the single-board protocol self-tests |
| **board2** | Router | ProMicro clone |
| **board3** | Router / end device | ProMicro clone |
| **board4** | End device (sleepy) | **nice!nano v2** — app at `0x26000`, S140 SoftDevice dormant so the hardware AES stays free |
| **board5** | Green Power (battery-less) | nice!nano v2; same dormant-SoftDevice story |

The fourth/fifth boards being nice!nano (S140) prove the full stack runs
identically on a SoftDevice-layout board: the SoftDevice sits dormant, the
hardware AES (ECB) is free, and crypto/security behave exactly as on the
no-SoftDevice ProMicro boards. See the ArduinoNRF core's
[SOFTDEVICE.md](https://github.com/dunknowcoding/ArduinoNRF/blob/main/docs/platform/SOFTDEVICE.md).

## Single board

- `CC2530_FlashFirmware` detects `0xA5xx`, flashes the SDCC transceiver, and
  verifies read-back.
- `CC2530_Info` reports firmware `v0.4` and repeated `ping -> PONG`.
- `CC2530_MacControl` writes PAN/short/IEEE address, enables filtering +
  Auto ACK + CCA TX with three retries, and reads the settings back.

### Protocol self-tests (board1, over J-Link)

| Example | Result |
|---------|--------|
| `CC2530_ApsSecurity` — APS key-transport crypto (AES-MMO, HMAC, specialized keys, CCM* Transport-Key envelope) | 25/25 |
| `CC2530_BroadcastTable` — broadcast dedup + passive-ack table | 22/22 |
| `CC2530_IndirectQueue` — sleepy-child Data Request + parent queue | 27/27 |
| `CC2530_EndDeviceTimeout` — SED keep-alive negotiation | 18/18 |
| `CC2530_PanIdConflict` — PAN-ID conflict detect + Network Report/Update | 16/16 |
| `CC2530_DeviceClusters` — Door Lock / Occupancy / Temperature / Humidity / Electrical Measurement clusters (Read Attributes records, reports, lock commands, signed/unsigned 16-bit values) | 26/26 |
| `CC2530_SourceRouteSecurity` — secured source-route OTA: relay-index AAD exclusion (bump the index on a secured frame and it still opens), per-hop re-secure, MIC integrity + replay rejection | 14/14 |
| `CC2530_Fragmentation` / `CC2530_KeyTransport` / `CC2530_Binding` | APS fragment reassembly, TC key-transport frames, source binding table |

### v0.8.0 feature-completion self-tests (board1, over J-Link)

Hardware-verified on board1 (nRF52840 hardware AES via NrfEcb), each printing
`RESULT: N passed, 0 failed`:

| Example | Result | What it proves on silicon |
|---------|--------|---------------------------|
| `CC2530_InstallCode` | 21/21 | install-code little-endian CRC + the real reference vector 83FED3...C3B5 -> 66B6900981E1EE3CA4206B6B861C02BB (AES-MMO on the hardware AES), and per-joiner Transport-Key wrap/unwrap (`ZigbeeTcLinkKeyStore`) |
| `CC2530_GreenPowerCluster` | 17/17 | GP proxy + GP cluster (0x0021): commissioning-through-proxy, secured operational decrypt at the sink (hardware CCM*), replay/dup + tampered-MIC rejection |
| `CC2530_InterPan` | 18/18 | full MAC inter-PAN frame build/parse (broadcast + unicast) carrying a touchlink Scan Request end to end |
| `CC2530_KeyRotation` | 16/16 | dual-key Switch-Key + the post-rekey replay resync (low new-key counter accepted, real replay still rejected) |
| `CC2530_PanIdConflict` | 29/29 | `ZigbeeNetworkManager` detect-from-beacon / new-PAN selection / EPID-scoped freshness-checked PAN-ID + channel apply |

## Two boards (board1 + board2)

- `CC2530_Link` shows `TX "hello N" ok` and reciprocal `RX (... dBm): hello N`
  on channel 11.
- `CC2530_MacLink` builds standards-shaped short-address MAC data frames and
  filters received frames by PAN ID / destination short address.
- `CC2530_NwkLink` wraps those in simple Zigbee NWK data frames
  (`NWK RX ... payload="nwk hello N"`).
- `CC2530_ZclLink` wraps ZCL On/Off Toggle command frames inside APS/NWK/MAC
  (`ZCL RX ... cmd=0x02`).
- `CC2530_OnOffCluster` applies Toggle to a local OnOff state, replies with ZCL
  Default Response, and answers Read Attributes.
- `CC2530_ClusterNode` dispatches incoming ZCL frames through reusable
  `ZigbeeOnOffCluster` / `ZigbeeBasicCluster` helpers.
- `CC2530_ReportingNode` accepts Configure Reporting for OnOff.OnOff and emits
  Report Attributes on state change / max interval.
- `CC2530_NwkCommandLink` exchanges Route Request/Reply and Network Status under
  hardware filtering + Auto ACK + CCA TX.
- `CC2530_ZdoDiscovery` exchanges IEEE/NWK address, Active Endpoint, Simple
  Descriptor, and Match Descriptor requests/responses.
- `CC2530_AssociationJoin` runs board1 as a coordinator with permit join open,
  accepts board2's MAC Association Request, assigns a short address, and
  receives board2's ZDO Device_annce.

## Join, scan, and self-healing

- `CC2530_BeaconJoin` runs a **parameterless** joiner (no preconfigured PAN,
  channel, or coordinator address): board2 scans channels 11/15/20/25 with
  Beacon Requests, hears board1's Zigbee beacon on channel 15
  (`pan=0x1A62 depth=0 permit=yes`), selects it as parent, associates on the
  first attempt (addr `0x0001`), and announces. The scan also met two foreign
  production Zigbee networks on channel 25 (`permit=no`) and correctly skipped
  them as parents.
- After joining, both nodes broadcast NWK Link Status every 15 s and settle at
  `in=1 out=1` link costs; silencing one makes the other print
  `aged out 1 stale router neighbor(s)` after three missed periods. A stale
  parent triggers parent-loss and re-entry into joining via `rejoinParent()`.
- The joiner discovers a route to board1 (Route Request broadcast → unicast
  Route Reply, reverse route recorded) and pings over it every 10 s at 100%.
  The full self-healing loop is verified: silencing board1 ages the parent out
  at 54 s, rejoin attempts fail, it falls back to scanning (skipping up to six
  foreign `permit=no` networks on channel 25), and when board1 returns it
  rejoins on the first attempt and rebuilds the route.

## Multi-hop routing (board1 + board2 + board3)

- **Encrypted round trip.** With board1 and board3 set to ignore each other's
  frames (a bench stand-in for being out of radio range), board3 scans, finds
  only router board2's beacon, and joins **through** board2 from board2's
  address pool (`addr=0x0031`). board3 then routes an encrypted ping to the
  coordinator: board2 forwards `0x0031->0x0000`, board1 decrypts it (`mic=0`)
  and replies, and the pong returns `board1->board2->board3` over the reverse
  route — a full `board3->board2->board1->board2->board3` encrypted round trip.
  Build the roles with `-DNIUS_ZIGBEE_THIS_NODE=0x0001` (board1), `0x0002`
  (board2), `0x0003` (board3).
- **APS end-to-end acked delivery.** board3's data plane sends an acked APS
  frame to board1 over the routed mesh; board1 replies with an APS ACK (same
  APS counter) over the reverse route, and the sender retransmits until the ACK
  arrives or the retry budget runs out. On the 3-board line this lifts confirmed
  end-to-end delivery from ~11% (raw routed ping, no recovery) to ~55%, and the
  sender's `aps[q=.. ok=.. rtx=.. drop=..]` status reports exactly which frames
  made it. (The 3-hop line below reaches 100% with raised retry budgets.)

## NWK security

- The entire join/route/data flow also runs with **NWK security enabled**:
  every NWK frame (announce, link status, route discovery, routed data) carries
  the Zigbee auxiliary header and is AES-CCM* ENC-MIC-32 protected, computed on
  the nRF52840 hardware AES block. Status lines report
  `sec[tx=18 rx=5 mic=0 rpl=0]`.
- A joiner built with `-DNIUS_ZIGBEE_WRONG_KEY=1` still associates at the MAC
  level but every NWK frame it sends is MIC-rejected (`mic=` climbs, `rx=`
  freezes, its link costs stay 0/0) — the security check actually bites.
- **Secure join** (`-DNIUS_ZIGBEE_SECURE_JOIN=1`): a joiner that holds only the
  default link key `ZigBeeAlliance09` associates, then board1 transports the
  network key at the APS layer (AES-MMO hash, HMAC-MMO, specialized
  key-transport key, APS CCM* envelope); the joiner decrypts it, installs it,
  and runs the secured data plane with `mic=0`. This needs CC2530 firmware v0.4,
  which fixes a clone RXFIFO underrun that corrupted the tail of large (~80 B)
  received frames.

## ZDO network mapping

- **`Mgmt_Lqi`.** board3 queries board1's neighbor table; the response is
  carried as an APS-acked frame with single-in-flight retransmit (an earlier
  naive version caused a retransmit storm). On a clean 1-hop link the request is
  answered on the first try every cycle and the acked APS data plane runs at
  100% (`q=44 ok=44 drop=0`).

## Multi-board topologies (4–5 boards, on air)

`CC2530_BeaconJoin` builds named topologies; each forces the desired shape on a
bench of co-located radios with a depth-filtered join + IEEE association-ignore
lists (deterministic regardless of the pool-assigned addresses).

> See the diagram in [the README](../README.md#mesh-topology).

- **3-hop line board1–board2–board3–board4** (`-DNIUS_ZIGBEE_LINE_TOPO=1`):
  board4's APS-acked data crosses `board4->board3->board2->board1` and back.
  With raised retry budgets (8 APS retransmits, 5 MAC retries/hop) and the
  mapping query suppressed on multi-hop, delivery is **100%**
  (`aps[q=15 ok=15 drop=0]`).
- **2×2 mesh + route repair** (`-DNIUS_ZIGBEE_MESH_TOPO=1`): board2 and board3
  are redundant routers under board1; board4 joins one, and when that parent is
  silenced it detects the stall, re-scans, and **self-heals onto the other
  router** (its address moves from one router's pool to the other's, delivery
  resumes).
- **Mesh + Green Power** (`-DNIUS_ZIGBEE_GP_SINK=1`): board1 runs the 4-node
  mesh **and** sinks board5's Green Power frames — a battery-less GPD toggles
  board1's LED while the mesh keeps running on the same channel.
- **Source-route OTA** (`-DNIUS_ZIGBEE_SOURCEROUTE=1`): board4's Route Record
  walks up the line (each relay appends itself), the concentrator (board1)
  learns the full path and source-routes a frame back down, and the destination
  delivers it (`SRCROUTE delivered ... "SR-ping"`).

## Notes

- Promiscuous examples can still show unrelated 802.15.4 traffic on the channel;
  filtered examples program PAN/short/IEEE addresses before exchanging frames.
- Channel choice on the bench: examples run on channel 25 (or 15) to dodge the
  lab's foreign Zigbee traffic on channel 11; any 11–26 channel works.
