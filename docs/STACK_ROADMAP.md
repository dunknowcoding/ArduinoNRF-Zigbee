# Zigbee Stack Roadmap

Date: 2026-06-10

NiusZigbee currently uses a CC2530 running the bundled SDCC transceiver
firmware. That firmware exposes raw IEEE 802.15.4 PSDUs over UART; it does not
run TI Z-Stack or Zigbee PRO internally.

## Priority order

1. **IEEE 802.15.4 MAC helpers**
   - Build and parse short-address data frames.
   - Carry PAN ID, source/destination short address, sequence number, and ACK
     request bit.
   - Use the current SDCC firmware's PAN/address registers, hardware filtering,
     Auto ACK, CCA transmit, and retry controls.

2. **Zigbee NWK frame helpers**
   - Add Zigbee network header construction/parsing on top of MAC payloads.
   - Keep this initially as frame tooling, not automatic joining/routing.
   - Add NWK command payload helpers for route request/reply, network status,
     route record, leave, and rejoin messages.

3. **APS and ZCL frame helpers**
   - Add endpoint/profile/cluster addressing and common ZCL command encoders.
   - Start with Basic, On/Off, Level Control, and reporting frame builders.

4. **ZDO discovery helpers**
   - Add endpoint 0 Zigbee Device Profile payload builders/parsers.
   - Cover address, active endpoint, simple descriptor, and match descriptor
     discovery before joining/routing is automatic.
   - Add a static local Device Object descriptor store that can answer those
     requests from sketch-provided endpoint descriptors.

5. **Association, join, and local tables**
   - Add coordinator/router/end-device state machines on the nRF host while
     keeping CC2530 as a MAC/PHY co-processor.
   - Add neighbor/routing tables, route discovery/repair, and NWK/APS security.
   - Add local network identity, permit-join timing, short-address allocation,
     child acceptance, parent bookkeeping, MAC association request/response, and
     ZDO Device_annce.
   - Persist network state, frame counters, bindings, reporting tables, and
     attributes in nRF flash.

6. **ZNP / Z-Stack backend**
   - Add a separate backend for CC2530 modules flashed with TI Z-Stack ZNP.
   - This is the path for real coordinator/router/end-device join flows,
     binding, Trust Center behavior, install codes, and Zigbee security.

## Implemented in this revision

- `ZigbeeMac::buildShortDataFrame()`
- `ZigbeeMac::parseShortDataFrame()`
- `CC2530Radio::sendData()`
- `CC2530Radio::onDataReceive()`
- `examples/CC2530_MacLink`
- `ZigbeeNwk::buildDataFrame()`
- `ZigbeeNwk::parseDataFrame()`
- `ZigbeeNwk::buildCommandFrame()`
- `ZigbeeNwk::parseCommandFrame()`
- `ZigbeeNwk` route request/reply, network status, route record, leave, and rejoin helpers
- `CC2530Radio::sendNwkData()`
- `CC2530Radio::onNwkReceive()`
- `CC2530Radio::sendNwkCommand()`
- `CC2530Radio::onNwkCommandReceive()`
- `examples/CC2530_NwkLink`
- `examples/CC2530_NwkCommandLink`
- `ZigbeeAps::buildDataFrame()`
- `ZigbeeAps::parseDataFrame()`
- `ZigbeeZcl::buildCommandFrame()`
- `ZigbeeZcl::parseFrame()`
- `ZigbeeZcl::buildReadAttributesPayload()`
- `ZigbeeZcl::buildDefaultResponsePayload()`
- `ZigbeeZcl::buildBoolAttributeRecord()`
- `ZigbeeZcl::buildCharStringAttributeRecord()`
- `ZigbeeZcl::buildReportBoolAttributePayload()`
- `ZigbeeZcl::applyOnOffCommand()`
- `ZigbeeOnOffCluster`
- `ZigbeeBasicCluster`
- `ZigbeeBoolReportScheduler`
- `CC2530Radio::setAddress()`
- `CC2530Radio::configureMac()`
- `CC2530Radio::getMacInfo()`
- `CC2530Radio::setTxPowerRaw()`
- `CC2530Radio::sendWithRetries()`
- `CC2530Radio::sendApsData()`
- `ZigbeeZdo`
- `ZigbeeDeviceObject`
- `ZigbeeNeighborTable`
- `ZigbeeRouteTable`
- `ZigbeeNetwork`
- `ZigbeePermitJoin`
- `ZigbeeAddressAllocator`
- `ZigbeeMac` Association Request/Response helpers
- `CC2530Radio::sendAssociationRequest()`
- `CC2530Radio::sendAssociationResponse()`
- `CC2530Radio::onMacCommandReceive()`
- `ZigbeeZdo::buildDeviceAnnounce()`
- `ZigbeeZdo::parseDeviceAnnounce()`
- `examples/CC2530_AssociationJoin`
- `ZigbeeMac::buildBeaconRequest()` / `buildBeacon()` / `parseBeacon()`
- `ZigbeeNwk::buildBeaconPayload()` / `parseBeaconPayload()`
- `CC2530Radio::sendBeaconRequest()` / `sendBeacon()` / `onBeaconReceive()`
- `ZigbeeNetwork` active scan: `beginScan()` / `noteBeacon()` /
  `selectParent()` / `beginJoiningCandidate()` / `rejoinParent()` and
  join-attempt bookkeeping
- `examples/CC2530_BeaconJoin` (HW-verified: parameterless joiner scans
  4 channels, finds the coordinator's beacon, picks it as parent,
  associates on the first attempt, and announces)
- `ZigbeeNwk` Link Status command build/parse (`buildLinkStatusPayload` /
  `parseLinkStatusPayload` / `getLinkStatusEntry`)
- `ZigbeeNeighbor` incoming/outgoing link costs;
  `ZigbeeNeighborTable::removeStaleRouters()` and raw `slot()` iteration
- `ZigbeeNetwork` neighbor aging: `costFromLqi()`, `linkStatusDue()` /
  `markLinkStatusSent()`, `collectLinkStatusEntries()`,
  `handleLinkStatus()` (parent-aware), `ageNeighbors()` with parent-loss
  reporting wired to `rejoinParent()`
- `examples/CC2530_BeaconJoin` link-status phase (HW-verified: bidirectional
  15 s Link Status with in/out cost 1 on both ends; a silenced neighbor is
  aged out after 3 missed periods)
- `ZigbeeRouting` AODV-style discovery decisions: `originateDiscovery()`,
  `handleRouteRequest()` (duplicate suppression via a discovery table,
  reverse-route recording, reply-as-destination, rebroadcast decision),
  `handleRouteReply()` (route installation + relay toward originator),
  `nextHopFor()` / `routeIsActive()` / `expire()`
- `examples/CC2530_BeaconJoin` route phase (HW-verified: RREQ -> RREP ->
  ACTIVE routes both ends -> periodic routed ping/pong at 100% round trip;
  full self-healing loop: silenced parent aged out at 54 s -> rejoin
  attempts -> re-scan -> auto-rejoin on coordinator return -> route rebuilt)
- `ZigbeeSecurity` (new): Zigbee NWK security level 5 (ENC-MIC-32) with
  AES-CCM* assembled host-side on the nRF52840 hardware AES-ECB block;
  14-byte auxiliary header with on-air level-bit zeroing/substitution,
  source-IEEE replay table, and tx/rx/MIC/replay statistics
- `CC2530Radio::attachSecurity()`: transparent encrypt on
  `sendNwkData()`/`sendNwkCommand()` (and everything layered on them) and
  verify-decrypt-reparse on receive; MIC/replay failures drop the frame for
  NWK-and-above callbacks
- `examples/CC2530_BeaconJoin` security phase (HW-verified: the whole
  join/link-status/route/ping flow runs encrypted with mic=0 rpl=0;
  a wrong-key joiner associates at MAC level but every NWK frame it sends
  is MIC-rejected)
- Multi-hop (HW-verified on a 3-board A-B-C line): routers answer beacon
  requests + accept children from a disjoint address pool; NWK unicast data
  is forwarded to the route next hop with radius decrement and per-hop
  re-encryption; relays and endpoints learn reverse routes from received
  frames (source NWK address via the previous-hop MAC neighbor). With A and
  C simulated out of range of each other, C joins **through** router B, and
  an encrypted routed ping/pong completes the full C->B->A->B->C round trip
  (mic=0 on the verified hops). Per-hop FCS/MIC loss without a NWK-layer ack
  still drops some round trips - end-to-end reliability is the next item.
- `CC2530Radio` promiscuous-state tracking: decryption is skipped while the
  frame filter is off (active scan), and `configureMac(kMacFilter)` clears
  the promiscuous flag so a coordinator that never calls `setPromiscuous`
  still decrypts. (Fixes a one-way-link bug where the coordinator silently
  skipped all decryption.)
- APS end-to-end acknowledged delivery: `ZigbeeAps::buildAckFrame()` /
  `parseAckFrame()` / `frameType()` (APS frame type 0b10), a
  `ZigbeeApsRetransmit` pending table (copy the acked APDU, match incoming
  ACKs by APS counter + endpoint, surface due retransmits, count
  delivered/retransmits/dropped), and `CC2530Radio::onApsAckReceive()` to
  dispatch ACK frames. The `CC2530_BeaconJoin` data plane now sends an acked
  APS frame to the coordinator over the routed mesh; the coordinator answers
  with an APS ACK over the reverse route. HW-verified on the 3-board A-B-C
  line: end-to-end delivery rose from ~11% (raw routed ping, no recovery) to
  ~55% confirmed delivery with retransmit, and the sender knows exactly which
  frames were delivered vs dropped.
- APS receive-side duplicate rejection (`ZigbeeApsDuplicateTable`): records
  the last APS counter per (source short addr, source endpoint) so a
  retransmit that reaches the destination is **re-acked but not reprocessed**
  by the application (the sender is still waiting for the ACK, but the frame
  must only be handled once). HW-verified: the coordinator logs
  `APS dup ... re-acked, not reprocessed` instead of handling the same APS
  counter twice.
- `CC2530Radio::sendZdoCommand()`
- `CC2530Radio::onZdoReceive()`
- `CC2530Radio::sendZclCommand()`
- `CC2530Radio::onApsReceive()`
- `CC2530Radio::onZclReceive()`
- `examples/CC2530_ZclLink`
- `examples/CC2530_OnOffCluster`
- `examples/CC2530_ClusterNode`
- `examples/CC2530_ReportingNode`
- `examples/CC2530_MacControl`
- `examples/CC2530_ZdoDiscovery`

This gives the library a real MAC envelope, a CC2530 MAC/PHY co-processor
configuration API, a minimal Zigbee NWK data-frame layer, unicast APS, endpoint
0 ZDO discovery payload tooling, NWK command-frame tooling, a static local
Device Object descriptor store, fixed neighbor/route table storage, ZCL
command-frame tooling, and tiny reusable Basic / OnOff behavior helpers with a
boolean report scheduler, local network-state/permit-join/address-allocation
helpers, over-the-air MAC association + Device_annce, and a hardware-verified
active scan / parent selection / join-retry flow (beacon request -> Zigbee
beacons -> candidate table -> association, with re-scan fallback and a
rejoin-toward-remembered-parent primitive), the Link Status neighbor-aging
protocol (periodic broadcast with bidirectional link costs, stale-router
removal, parent-loss detection feeding rejoin), and AODV-style route
discovery (RREQ/RREP with duplicate suppression and reverse routes,
hardware-verified end to end including the parent-loss self-healing loop),
while preserving the existing raw send / receive / sniffer APIs.

## Remaining gaps toward Zigbee PRO (priority order)

1. **Key transport** - the network key is currently pre-shared in the
   sketch. Standard Trust Center behavior (transport the network key at
   join time under a link key, key rotation via keySeq) is the next
   security step. Frame-counter persistence also belongs here (today a
   reboot restarts counters and peers compensate by resetting their replay
   tables at join events).
2. **Broadcast Transaction Table** - broadcast dedup/passive ack. Required
   for correct multi-hop RREQ/broadcast behavior (the current discovery
   table only dedups route requests).
3. **Indirect transmission / sleepy end devices** - a parent-side pending
   queue plus MAC Data Request handling so rx-off children can poll.
   Needs CC2530 firmware support for the pending bit in acks (or host
   emulation with relaxed timing).
4. **ZDO Mgmt_Lqi_rsp / Mgmt_Rtg_rsp** - DONE (frame tooling +
   Mgmt_Lqi HW-verified). `ZigbeeZdo` builds/parses Mgmt_Lqi_req /
   Mgmt_Lqi_rsp / Mgmt_Rtg_req / Mgmt_Rtg_rsp with neighbor-list (22 B) and
   routing-list (5 B) entry encode/decode. `CC2530_BeaconJoin`: the end
   device queries the coordinator's neighbor table over the routed mesh and
   re-sends the request until the response arrives (the rsp is the ack - ZDO
   frames carry no APS ack here). HW-verified: the end device reads a
   neighbor table back and prints device-type / relationship / depth / LQI
   per entry; the addressed-node check makes only the request's target answer
   (the coordinator answered 40 requests in a run, where before the fix an
   intermediate router also replied to a request meant for the coordinator),
   and the routed request/response carry a per-hop MAC ack like the data
   plane. The end device is also marked RFD in its capability so a parent no
   longer treats it as a router and ages it out - it now holds a stable short
   address (0x0031) across a run with ~100% APS delivery, instead of taking a
   fresh address each re-association. Two follow-ups remain: Mgmt_Rtg example
   wiring, and the end device actually *receiving* the routed Mgmt_Lqi_rsp -
   the coordinator answers every request (verified 40x) and the same A->B->C
   path carries APS acks at ~100%, but the ZDO response's last hop is not yet
   delivered/parsed at the end device. Needs a frame capture to isolate why a
   ZDO APS frame forwards differently from an APS-data frame on that hop.
5. **Persistence** - network identity, addresses, frame counters, bindings,
   and reporting config in nRF flash (NrfNvmc) so nodes survive reboots.
6. **Binding table + group addressing** and APS fragmentation. (APS
   end-to-end acknowledgements/retries are DONE - see below; binding,
   groups, and fragmentation remain.)
7. **PAN ID conflict resolution and network update** (updateId / channel
   change propagation).
8. **Multi-hop relay** - DONE (code): routers answer beacon requests and
   accept children from a disjoint address pool, NWK unicast data frames
   whose destination is not the receiver are forwarded to the route's next
   hop (radius-1, re-encrypted per hop), and RREQ rebroadcast / RREP relay
   build the multi-hop routes. The `CC2530_BeaconJoin` example scales to an
   A-B-C line (coordinator / router / end) with an optional range-simulation
   ignore list so three co-located radios still form a 2-hop topology.
   Coordinator+router (A-B) is hardware-verified; the full A-B-C run with a
   third board+module is the remaining verification.
