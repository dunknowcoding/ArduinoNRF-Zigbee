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

> **M1-M6 completion batch (release 0.4.0).** A planned sweep closed most of the
> remaining gaps, each with a hardware self-test:
> - **M1 ZDO network management** - `ZigbeeZdoMgmt`: Mgmt_Permit_Joining,
>   Mgmt_Leave, Node_Desc (9/9).
> - **M2 APS fragmentation send path** - `ZigbeeApsFragmenter` iterates a long
>   payload into fragments; reassembles end to end (10/10).
> - **M3 Key rotation** - `ZigbeeSecurity` dual-key by sequence + `switchKey()`;
>   APS app-data encryption via `ZigbeeApsSecurity` (10/10, single-key
>   regression preserved).
> - **M4 Source-routing completion** - `nwkHeaderLength` handles the
>   source-route subframe; concentrator originates from Route Records (38/38).
> - **M5 BDB Finding & Binding** - `ZigbeeFindingBinding` matches client/server
>   clusters and creates the bindings (9/9).
> - **M6 Device abstraction** - `ZigbeeLight` ties On/Off + Level + Color +
>   Identify + Groups + Scenes into one device; `ZigbeeIasZoneCluster` sensor
>   (14/14).
> Remaining beyond this batch: fully-secured multi-hop source routing (mutable
> relay index excluded from the CCM* AAD), OTA Upgrade cluster, and more HA
> clusters; plus multi-board OTA verification of source routing on a stable
> 2-hop bench.

1. **Key transport** - command frames DONE; APS-layer encryption envelope
   DONE + HW-verified; join-time handshake remains. `ZigbeeApsKey` builds/parses
   the APS key commands - Transport-Key (network key, 35 B), Request-Key
   (network or app link key with partner), Switch-Key - and provides the
   default global TC link key "ZigBeeAlliance09". `CC2530_KeyTransport`
   self-tests them (10/10 PASS). `ZigbeeApsSecurity` now encrypts the
   Transport-Key on air at the APS layer: AES-MMO hash + HMAC-over-MMO (Zigbee
   B.6/B.1.4), the specialized key derivation (key-transport key =
   HMAC(linkKey, 0x00), key-load key = HMAC(linkKey, 0x02)), and a CCM*
   envelope (APS nonce = src IEEE | frame counter | security control; AAD =
   APS header + aux header; on-air level zeroed) built on the SAME
   hardware-verified CCM* core as the NWK layer (extracted into the shared
   `ZigbeeCcmStar.h`, which `ZigbeeSecurity` now also uses). `CC2530_ApsSecurity`
   self-tests it 25/25 on hardware: MMO cross-checked against the raw AES block,
   specialized keys distinct + key-dependent, a full Transport-Key wrap under
   the default TC link key round-trips and parses, and tampered
   ciphertext/AAD/wrong-key are all MIC-rejected. The join-time handshake is
   wired into CC2530_BeaconJoin behind `-DNIUS_ZIGBEE_SECURE_JOIN=1` and is now
   **HW-verified on air**: the joiner starts with ONLY the link key, the TC
   sends the encrypted Transport-Key (via `radio.sendNwkDataUnsecured`, since
   the joiner has no network key yet) after association, the joiner decrypts it
   with its link key, installs the network key, and confirms with its first
   secured Device_annce (`secure join COMPLETE`). The joiner then runs the
   secured data plane (`mic=0`). This required a CC2530 firmware fix (v0.4): the
   on-air MIC failures were a clone-radio bug, not the crypto - `radio_rx()`
   read the RXFIFO in a tight loop the moment FIFOP asserted, but FIFOP fires at
   the default ~64-byte threshold (mid-reception) for a large frame, so the read
   underran the FIFO and copied repeated stale bytes for the frame tail (the
   cipher/MIC), which only large frames (the ~80 B key transport) ever hit. The
   fix paces each read to reception (`while(RXFIFOCNT==0)` per byte, bounded).
   Remaining: key rotation via keySeq + Switch-Key, and the fresh-key-install
   replay-counter resync (the joiner currently logs `rpl` churn right after
   keying). Frame-counter persistence is DONE (item 5).
2. **Broadcast Transaction Table** - DONE (data structure + self-test).
   `ZigbeeBroadcastTable` tracks each broadcast transaction = (NWK source,
   sequence number): `recordIncoming` returns NEW (process + rebroadcast once)
   vs DUPLICATE (count as a passive ack, do not reprocess); `recordOutgoing`
   tracks our own floods; `markPassiveAck` records a neighbor's rebroadcast;
   `due(neededAcks, retryMs, maxRetries)` returns the next transaction needing
   a retry (window elapsed, retries left, too few passive acks); `expire`
   reclaims entries after nwkNetworkBroadcastDeliveryTime (~9 s); the table
   recycles the oldest slot when full. `CC2530_BroadcastTable` self-tests it
   22/22 on hardware (dedup, passive-ack suppression, retry cap, expiry +
   recycle). What remains: wire it into the example's broadcast RX/TX path
   (RREQ, Link Status, Device_annce) so relays dedup + passively-ack on air.
3. **Indirect transmission / sleepy end devices** - host-side DONE (frame +
   queue + self-test); on-air pending bit needs CC2530 firmware. A sleepy child
   polls its parent with a MAC Data Request (`ZigbeeMac::buildDataRequest`:
   short-addressed, ack-requested, PAN-ID compressed); the parent buffers frames
   for the child in `ZigbeeIndirectQueue` (enqueue keeps the latest frame per
   child + refreshes the timeout, `pending`/`hasPending` answer a poll without
   removing so a lost frame can be re-polled, `dequeue` on confirmed delivery,
   `expire` ages out after macTransactionPersistenceTime ~7.68 s, oldest slot
   reused when full). `CC2530_IndirectQueue` self-tests it 27/27 on hardware
   (Data Request build+parse, enqueue/poll/peek/dequeue, replace-latest +
   capacity reuse, expiry). What remains: set the frame-pending bit in the ack
   to the Data Request on air, which needs CC2530 firmware support (or
   relaxed-timing host emulation); `hasPending()` is the host-side input.
   The keep-alive side is DONE too: `ZigbeeEndDeviceTimeout` builds/parses the
   NWK End Device Timeout Request (0x0B) / Response (0x0C) and maps the timeout
   enumeration to seconds (index 0 = 10 s, then 2^n minutes), so a sleepy child
   negotiates how long it may stay silent and the parent advertises which
   keep-alive methods it supports (MAC data poll / ED timeout).
   `CC2530_EndDeviceTimeout` self-tests it 18/18 on hardware.
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
   wiring, and the end device actually *receiving* the routed Mgmt_Lqi_rsp.
   **FIXED + HW-verified (1-hop).** The rsp is now carried as an APS *data*
   frame with the APS ack-request bit set (ZDO endpoint/profile, so it still
   dispatches to `onZdoFrame` at the far end) and registered in the
   `ZigbeeApsRetransmit` table - exactly like the data plane. The requester
   ACKs it; the responder retransmits until the ACK arrives. A first naive
   "queue an acked rsp per request" version caused a **retransmit storm** (the
   requester re-sends its request every couple seconds until answered, so the
   responder queued a fresh rsp + 5 retransmits per request retry, ~322
   re-sends in a run, congesting the half-duplex channel so the *longer* rsp
   never won air - delivered 0). Two changes fixed it: (a)
   `ZigbeeApsRetransmit::hasPendingFor(dst, srcEndpoint)` lets the responder
   keep **one rsp in flight per requester** (no per-retry re-queue), and (b)
   gentler budgets (rsp 3x/2 s, request 6x/2.5 s). HW result on a clean 1-hop
   A<->C star: every Mgmt_Lqi_req is answered on **try 1**, `Mgmt_Lqi_rsp from
   0x0000` printed every cycle, and the APS data plane runs `q=44 ok=44 drop=0`
   (100%, vs ok=0 during the storm). Lesson learned on the bench: the rsp
   "multi-hop failure" was compounded by stale cross-board state (boards
   reflashed separately disagree on addresses/routes) and the range-sim forcing
   a restored direct-parent route that the simulated-deaf link can't carry; a
   consistent fresh bring-up (`-DNIUS_ZIGBEE_IGNORE_SAVED=1`) is required to
   test the mesh cleanly. Remaining: a genuine 2-hop verification through a
   live relay (the mechanism is identical - `sendApsRouted` already routes via
   the next hop and retransmits).
5. **Persistence** - DONE for network identity + frame counter.
   `ZigbeePersistence` serializes the network state (PAN, ext PAN, channel,
   short/parent address, depth, device type, IEEE, outgoing security frame
   counter, key sequence) to a fixed little-endian blob with a CRC-16, and
   restores it; the sketch stores the blob in the core's wear-levelled
   EEPROM. `CC2530Radio::securityFrameCounter()` / `setSecurityFrameCounter()`
   expose the counter. `CC2530_BeaconJoin` saves every 20 s and, on boot,
   restores the joined identity (no re-scan / re-association) and resumes the
   frame counter with a +1024 margin so it never rewinds. HW-verified: after
   a reboot the end device prints `RESTORED from flash: addr=0x0031
   parent=0x0001 counter=1072 - skipping scan` and immediately resumes
   encrypted traffic. Remaining: persisting bindings + reporting config (once
   those tables exist).
6. **Binding table + group addressing** - DONE for the table and frames.
   `ZigbeeBindingTable` is a local source-binding store (source endpoint +
   cluster -> destination IEEE+endpoint or group) with idempotent add,
   remove, and a `next()` iterator a sender uses to deliver to every bound
   destination. `ZigbeeZdo` builds/parses Bind_req / Unbind_req (IEEE mode 22
   B, group mode 15 B) and Bind_rsp. The `CC2530_Binding` example self-tests
   the table and frame round-trips (13/13 PASS on hardware) and shows the
   indirect-send walk. Remaining here: wiring the binding into an over-the-air
   APSDE indirect transmit in the mesh example.
   **APS fragmentation - DONE.** `ZigbeeApsFragment` builds/parses fragment
   APDUs (APS extended header: extFCF first/subsequent + block number; first
   fragment carries the total block count), and `ZigbeeApsReassembler`
   reassembles by block number with a caller-supplied block size so
   out-of-order and duplicate blocks are handled. The `CC2530_Fragmentation`
   example self-tests a 180 B ASDU -> 5 fragments -> reassembly in order,
   reversed, and with a duplicate block (6/6 PASS on hardware). This is the
   general fix for long frames over the air, including the long Mgmt_Lqi
   response - wiring fragmentation into the multi-hop send/receive path is the
   remaining integration step.
7. **PAN ID conflict resolution and network update** - frames + detection DONE.
   `ZigbeePanIdConflict` provides the conflict test (same 16-bit PAN ID +
   different 64-bit extended PAN ID = a real clash, not our own network) and
   builds/parses the NWK Network Report (0x09, PAN ID conflict: options + EPID +
   conflicting PAN ID list) and Network Update (0x0A, PAN ID update: options +
   EPID + nwkUpdateId + new PAN ID), with the 8-bit-wraparound update-id
   freshness test so every node adopts the manager's newest value.
   `CC2530_PanIdConflict` self-tests it 16/16 on hardware. What remains: drive
   detection from heard beacons, the manager's new-PAN-ID selection, and
   applying the update (and the analogous channel-change propagation).
8. **Multi-hop relay** - DONE (code): routers answer beacon requests and
   accept children from a disjoint address pool, NWK unicast data frames
   whose destination is not the receiver are forwarded to the route's next
   hop (radius-1, re-encrypted per hop), and RREQ rebroadcast / RREP relay
   build the multi-hop routes. The `CC2530_BeaconJoin` example scales to an
   A-B-C line (coordinator / router / end) with an optional range-simulation
   ignore list so three co-located radios still form a 2-hop topology.
   Coordinator+router (A-B) is hardware-verified; the full A-B-C run with a
   third board+module is the remaining verification.
9. **Many-to-one / source routing** - frames + concentrator table DONE.
   `ZigbeeNwk::buildRouteRequestPayload(..., manyToOne=true)` builds the
   many-to-one route request a concentrator floods, and the Route Record
   command (NWK 0x05, `buildRouteRecordPayload` / `parseRouteRecordPayload` /
   `getRouteRecordRelay`) carries the relay path a device accumulates back to
   the concentrator. `ZigbeeSourceRouteTable` stores those paths concentrator-
   side: `install` from a received Route Record (travel order device ->
   concentrator), `downstreamRoute` returns the reversed path (concentrator ->
   device) for the NWK source-route subframe, plus refresh/expire/recycle.
   The NWK source-route subframe is now emitted/parsed too:
   `ZigbeeNwk::buildDataFrameSourceRouted` sets the source-route FCF bit and
   writes the subframe (relay count, relay index, ordered relay list) before the
   payload, and `parseDataFrame` parses it (exposing `srRelayCount` /
   `srRelayIndex` / `getDataFrameRelay`) instead of rejecting it - plain data
   frames are unchanged. The per-hop forwarding decision is implemented too:
   `ZigbeeNwk::sourceRouteAction(frame, selfShort, nextHop, outRelayIndex)`
   returns DELIVER (we are the destination) / RELAY (forward to the next relay,
   or the destination if we were the last relay, with the updated relay index) /
   DROP (off-path or malformed). `CC2530_SourceRouting` self-tests the frames +
   the forwarding decision 34/34 on hardware. The relay is wired into the
   example: `CC2530_BeaconJoin -DNIUS_ZIGBEE_SOURCEROUTE=1` makes a router that
   receives a source-routed frame not addressed to it rebuild it with the
   advanced relay index and forward it to the next hop named in the subframe
   (unsecured, via `radio.sendData`). What remains: a concentrator that
   originates source-routed frames from collected Route Records, and NWK
   security AAD over the (mutable-relay-index) subframe so secured frames can be
   source-routed. Multi-board OTA verification needs a stable 2-hop bench (the
   co-located radios only form one via the flaky range-sim).
10. **Install codes** - DONE (derivation + self-test). `ZigbeeInstallCode`
    validates an install code's CRC (CRC-16/X-25, checked against the standard
    "123456789" -> 0x906E vector), builds a code from a body, and derives the
    per-device Trust Center link key as the AES-MMO-128 hash of the full code
    (reusing `ZigbeeApsSecurity::aesMmoHash`). This is the per-device
    alternative to the global "ZigBeeAlliance09" link key for APS key transport.
    `CC2530_InstallCode` self-tests it 11/11 on hardware. What remains: feed the
    derived key into the secure-join handshake (per-joiner link key instead of
    the default) and a real spec install-code -> key reference vector.
11. **Group addressing / multicast** - frames + membership DONE.
    `ZigbeeAps::buildGroupDataFrame` builds a group-addressed APS data frame
    (delivery mode = group, a 16-bit group address in place of the destination
    endpoint, 9-byte header) and `parseDataFrame` now parses it (unicast frames
    unchanged). `ZigbeeGroupTable` is the device's group membership store
    (join/leave/isMember/enumerate); the receive path delivers a group frame to
    the app only if `isMember(groupAddress)`. `CC2530_GroupCast` self-tests it
    31/31 on hardware (group frame round-trip, unicast regression, membership,
    accept/ignore by membership). The ZCL Groups cluster (0x0004) is included:
    `ZigbeeGroupsCluster` builds the Add/Remove/Get-Membership command +
    response payloads and `handle()` applies an incoming command to the group
    table and produces the response (SUCCESS / DUPLICATE_EXISTS / NOT_FOUND /
    INSUFFICIENT_SPACE), so a coordinator can drive membership remotely. It is
    also **HW-verified on air**: `CC2530_BeaconJoin -DNIUS_ZIGBEE_GROUPCAST=1`
    has the coordinator broadcast a group-addressed ZCL On/Off Toggle (a group
    APS frame inside a NWK-secured broadcast to 0xFFFD) every 8 s, and a member
    end device parses it (`mic=0`) and toggles its built-in LED - verified as
    alternating `LED=ON/OFF` on the bench. Group addressing is complete.
12. **ZCL cluster library** - growing. Reusable cluster helpers: Basic + On/Off
    (`ZigbeeBasicCluster` / `ZigbeeOnOffCluster`), boolean reporting
    (`ZigbeeBoolReportScheduler`), Groups (`ZigbeeGroupsCluster`, item 11), and
    Level Control (`ZigbeeLevelControlCluster`, the dimming half of a dimmable
    light: Move to Level / Move / Step / Stop command payloads + apply-to-level
    behavior with 0..254 clamping). `CC2530_LevelControl` self-tests it 10/10 on
    hardware. Identify (`ZigbeeIdentifyCluster`, 0x0003) is included: Identify /
    Identify Query Response / Trigger Effect payloads + the IdentifyTime
    countdown (apply / tick-per-second / isIdentifying) used during commissioning
    to make a device blink; `CC2530_Identify` self-tests it 13/13. Scenes
    (`ZigbeeScenesCluster` + `ZigbeeSceneTable`, 0x0005) stores a device snapshot
    (On/Off + Level) per (group, scene) and handles Store / Recall / Remove /
    Remove-All (Store captures the current state, Recall writes it back);
    `CC2530_Scenes` self-tests it 17/17. The reusable cluster set is now Basic,
    Identify, Groups, On/Off, Level Control, Scenes + boolean reporting. Color
    Control (`ZigbeeColorControlCluster`, 0x0300) completes the color-bulb triad:
    Move to Hue and Saturation / Move to Color (CIE xy) / Move to Color
    Temperature command payloads + apply-to-`ColorState` (hue/sat/x/y/mireds +
    the active color mode); `CC2530_ColorControl` self-tests it 9/9. What
    remains for a full ZCL library: the other HA clusters (Window Covering,
    Thermostat, ...) and the OTA Upgrade cluster.
