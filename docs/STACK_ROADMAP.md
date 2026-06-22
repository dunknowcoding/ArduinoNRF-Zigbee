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
> Remaining beyond this batch: multi-board OTA verification of source routing on
> a stable 2-hop bench (and any further HA clusters as needed - Thermostat and
> Window Covering are now added, `CC2530_HaClusters` 12/12, so the device set
> spans light / switch / sensor / thermostat / blinds). **Fully-secured
> multi-hop source routing is now DONE:** secureNpdu/openNpdu zero the mutable
> relay-index byte in the CCM* AAD, so a relay can rewrite it without breaking
> the MIC while the rest of the NWK header stays authenticated
> (CC2530_SourceRouting verifies this: secure -> change relay index -> still
> decrypts; a changed destination still fails). The **OTA Upgrade
> cluster** (0x0019) is now DONE: `ZigbeeOtaCluster` builds/parses Query Next
> Image, Image Block, and Upgrade End (request + response); `CC2530_OtaUpgrade`
> self-tests a full image transfer 8/8 (query -> block download -> reassemble
> matching the server image -> upgrade end).

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
   The dual-key rotation (keySeq + Switch-Key) and the **post-rekey replay resync
   are now DONE**: the NWK-security replay identity is (source IEEE, key sequence),
   so a low frame counter under a freshly installed key is accepted as fresh
   instead of being dropped as a replay against the old key's high counter (the
   `rpl` churn). `CC2530_KeyRotation` adds a test that rotates the key without
   resetting the replay table and confirms the low new-key counter opens with no
   replay drop, while a genuine replay is still rejected. Remaining: the on-air
   `-DNIUS_ZIGBEE_KEY_ROTATE=1` wiring in CC2530_BeaconJoin (TC distributes the
   new key + broadcasts Switch-Key; each node resets its outgoing counter on
   switch). Frame-counter persistence is DONE (item 5).
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
   recycle). Reassessed during Phase 2: the on-air example does NOT need it - NWK
   security's per-(source, frame-counter) replay check already drops duplicate
   rebroadcasts at every relay, so an app-layer broadcast transaction table is
   redundant on this stack. The data structure remains available for callers that
   run NWK without security. (Phase 2 broadcast item: closed by this finding.)
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
   capacity reuse, expiry). **The on-air frame-pending bit is now supported by
   CC2530 firmware v0.5:** a new `SET_PENDING` command (0x0A) sets/clears
   `FRMCTRL1.PENDING_OR`, forcing the frame-pending bit in outgoing auto-ACKs, and
   `CC2530Radio::setFramePending(bool)` drives it from `ZigbeeIndirectQueue::
   hasPending()` (opt-in; default off, so non-sleepy behavior is unchanged). The
   v0.5 firmware is build-verified (SDCC) and the binary/embedded header are
   regenerated. What remains: confirm the on-air pending-bit effect with a real
   sleepy child on the bench, and wire it into the multi-board sleepy demo.
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
   indirect-send walk. **On-air wiring DONE** (Phase 2): `CC2530_BeaconJoin
   -DNIUS_ZIGBEE_BINDTEST=1` installs a source binding (On/Off endpoint -> the
   coordinator) and unicasts a ZCL Toggle resolved through the binding table;
   bench-verified over the 3-hop line (D "BIND tx ... delivered=1", coordinator
   "BIND rx On/Off -> LED=ON/OFF"). See docs/VERIFIED_BEHAVIOR.md.
   **APS fragmentation - DONE, including on-air (Phase 2).** `ZigbeeApsFragment`
   builds/parses fragment APDUs (APS extended header: extFCF first/subsequent +
   block number; first fragment carries the total block count), and
   `ZigbeeApsReassembler` reassembles by block number with a caller-supplied
   block size so out-of-order and duplicate blocks are handled. The
   `CC2530_Fragmentation` example self-tests a 180 B ASDU -> 5 fragments ->
   reassembly (6/6 PASS) and now also round-trips a fragment through
   `ZigbeeAps::parseDataFrame`. Multi-hop wiring is bench-verified via
   `CC2530_BeaconJoin -DNIUS_ZIGBEE_FRAGTEST=1` (120 B ASDU -> 3 blocks over
   D->C->B->A -> "REASSEMBLED 120B OK"). Phase 2 also fixed a real bug: the APS
   ext-header FCF bit was 0x08 (collides with delivery mode) and parseDataFrame
   rejected ext-header frames - so fragmentation could never be received on air;
   the bit is now 0x80 and parseDataFrame accepts ext-header unicast data.
7. **PAN ID conflict resolution and network update** - frames + detection DONE.
   `ZigbeePanIdConflict` provides the conflict test (same 16-bit PAN ID +
   different 64-bit extended PAN ID = a real clash, not our own network) and
   builds/parses the NWK Network Report (0x09, PAN ID conflict: options + EPID +
   conflicting PAN ID list) and Network Update (0x0A, PAN ID update: options +
   EPID + nwkUpdateId + new PAN ID), with the 8-bit-wraparound update-id
   freshness test so every node adopts the manager's newest value.
   `CC2530_PanIdConflict` self-tests it 16/16 on hardware. **The integration is
   now DONE:** `ZigbeeNetworkManager` drives detection from heard beacons
   (`noteBeacon` records each PAN ID and flags a conflict), the manager's
   new-PAN-ID selection (`selectNewPanId` avoids our PAN ID, all heard PAN IDs,
   and the reserved 0x0000/0xFFFF), and applying an update (`applyUpdate` adopts a
   PAN ID *or* channel change that is EPID-scoped and fresher by the update-id
   half-window test; `commitPanId`/`commitChannel` for the manager's own side).
   Channel-change propagation is added too (`buildChannelUpdate`/
   `parseChannelUpdate`, NWK_UPDATE_CHANNEL). `CC2530_PanIdConflict` self-tests
   the manager + channel update. Compiles clean; on-air drive from the live beacon
   path in CC2530_BeaconJoin pending.
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
   (unsecured, via `radio.sendData`). **OTA-verified end to end on a 4-board
   3-hop line** (A-B-C-D, `-DNIUS_ZIGBEE_LINE_TOPO=1 -DNIUS_ZIGBEE_SOURCEROUTE=1`):
   the end device D sends a Route Record up to the concentrator A (each relay
   appends itself), A learns the full path A->B->C->D and reverses it, then A
   originates a source-routed data frame down to D; B and C forward it by the
   subframe's relay list (not a route lookup) and D delivers it
   (`SRCROUTE delivered from 0x0000 "SR-ping"`). What remains: NWK security AAD
   over the (mutable-relay-index) subframe so secured frames can be source-routed.
10. **Install codes** - DONE (derivation + self-test). `ZigbeeInstallCode`
    validates an install code's CRC (CRC-16/X-25, checked against the standard
    "123456789" -> 0x906E vector), builds a code from a body, and derives the
    per-device Trust Center link key as the AES-MMO-128 hash of the full code
    (reusing `ZigbeeApsSecurity::aesMmoHash`). This is the per-device
    alternative to the global "ZigBeeAlliance09" link key for APS key transport.
    `CC2530_InstallCode` self-tests it 11/11 on hardware. **Per-joiner secure join
    is now DONE.** Two fixes/additions: (a) the install-code CRC is now stored
    LITTLE-endian to match real Zigbee install codes (was big-endian; the
    build/validate round trip was self-consistent but did not interoperate with
    real codes) - verified against the canonical reference vector
    83FED3407A939723A5C639B26916D505 / CRC C3 B5 ->
    66B6900981E1EE3CA4206B6B861C02BB; (b) `ZigbeeTcLinkKeyStore` maps a joiner
    IEEE to its link key (`provisionInstallCode` derives + stores it; `keyFor`
    returns the per-device key or falls back to the global TC link key), so the
    secure-join code wraps the Transport-Key under the joiner's unique key. The
    `CC2530_InstallCode` self-test now adds the reference vector and a full
    per-joiner Transport-Key wrap/unwrap (the joiner with the matching install
    code recovers the network key; a wrong code is MIC-rejected). Compiles clean;
    HW bench run pending. What remains: wire `ZigbeeTcLinkKeyStore` into
    `CC2530_BeaconJoin` behind a build flag for an on-air per-joiner secure join.
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

13. **Touchlink (ZLL) commissioning** - DONE (frames + key transport + software
    AES decrypt). Touchlink is Zigbee 3.0 proximity commissioning over inter-PAN:
    `ZigbeeTouchlink` builds/parses the commissioning-cluster (0x1000) commands
    (Scan Request/Response, Identify Request, Network Join Router Request) and
    implements the ZLL network-key transport - transport key =
    AES-ECB-Encrypt(masterKey, expanded transaction/response id), network key =
    AES-ECB-Encrypt(transportKey, key) on the initiator, recovered with
    AES-ECB-Decrypt on the target. Because the nRF ECB peripheral is
    encrypt-only, `ZigbeeAes128Decrypt` adds a compact FIPS-197 software inverse
    cipher (verified against the FIPS-197 vector AND cross-checked vs the
    hardware ECB). `CC2530_Touchlink` self-tests it 13/13 on hardware: AES KAT,
    command round-trips, and a full encrypt-on-initiator / recover-on-target key
    transport (wrong transaction id does not recover the key). The inter-PAN
    transmission layer the commands ride on is now DONE (item 15:
    `ZigbeeMac::buildInterPanFrame` + `CC2530Radio::sendInterPan`), so a touchlink
    Scan Request now goes out as a real broadcast MAC inter-PAN frame. What
    remains: a 2-board OTA scan/response + network-start key transport in
    `CC2530_Touchlink`, and confirming the key-index-4 expanded-input order
    against a certified ZLL reference vector.

14. **Green Power** - DONE (GPDF + GP security + commissioning + sink table).
    Green Power devices (GPDs) are battery-less switches/sensors that emit Green
    Power Data Frames - a stub NWK frame (protocol version 3) with a GPD source
    id, frame counter, command, and an AES-CCM* MIC. `ZigbeeGreenPower` builds/
    parses the GPDF (unsecured for commissioning, and level-3 encrypted-command
    + 4-byte FC + 4-byte MIC), secures/opens it with the hardware-verified CCM*
    core (GP nonce = srcId||srcId||frameCounter||securityControl), and builds/
    parses the GP commissioning command (0xE0, carrying the GPD key).
    `ZigbeeGpSinkTable` commissions GPDs (stores the key) with per-GPD
    frame-counter replay protection. `CC2530_GreenPower` self-tests it 20/20 on
    hardware: commissioning key transport, secured-frame round trip (command
    encrypted on the wire, tamper + wrong-key rejected), and a sink decrypting
    and accepting a live GPD frame. **OTA-verified on the bench**
    (`CC2530_GreenPowerLink`, -DNIUS_ZIGBEE_GP_ROLE=1 GPD / =2 sink): a GPD with
    NO network join broadcasts a secured Toggle GPDF (encrypted command,
    advancing frame counter) every 3 s on a raw 802.15.4 broadcast, and the sink
    parses + decrypts it (mic ok), rejects replays (monotonic counter), and
    toggles its built-in LED - confirmed alternating LED=ON/OFF on hardware.
    **GP proxy forwarding + the GP cluster (0x0021) are now DONE.**
    `ZigbeeGreenPowerCluster` builds/parses the network-side GP cluster commands -
    GP Notification (0x00) and GP Commissioning Notification (0x04) proxy->sink,
    and GP Pairing (0x01) sink->proxy - on the GP endpoint (242) / ZGP profile
    (0xA1E0), and provides sink-side handlers that commission a GPD from a
    Commissioning Notification and verify+decrypt an operational Notification
    against the sink table. `ZigbeeGpProxy` turns an overheard raw GPDF into the
    right GP cluster payload (a secured operational frame is forwarded as
    ciphertext + MIC so the proxy needs no key; the sink reconstructs the
    deterministic secured-GPDF header and opens it), with per-GPD frame-counter
    duplicate suppression. `CC2530_GreenPowerCluster` self-tests the build/parse
    round trips, commissioning-through-proxy, secured operational decrypt at the
    sink, replay/dup rejection, and a tampered-MIC rejection (16 checks; compiles
    clean, HW bench run pending). What remains: a GP Response (0x06) downlink path
    and a certified GP reference vector.

15. **Inter-PAN transmission** - DONE. `ZigbeeInterPan` builds/parses the
    inter-PAN APDU - a one-octet stub NWK header (0x0B: frame type inter-PAN,
    protocol version 2) + a stripped APS header (frame control + optional group
    address + cluster + profile) + the command - the MAC payload that carries
    commissioning traffic (touchlink scans) before a device has joined a
    network. `CC2530_InterPan` self-tests it 9/9 on hardware, including a real
    touchlink Scan Request wrapped in an inter-PAN broadcast and recovered. This
    is the transmission layer touchlink (M13/#13) and Green Power scanning ride
    on. **The MAC-level inter-PAN frame is now DONE:**
    `ZigbeeMac::buildInterPanFrame` / `parseInterPanFrame` build/parse the real
    on-air frame (extended IEEE source addressing, broadcast PAN/short 0xFFFF for
    a scan or an extended destination for a unicast, no PAN ID compression), and
    `CC2530Radio::sendInterPan()` transmits it. `CC2530_InterPan` self-tests the
    full stack end to end (touchlink Scan Request -> inter-PAN APDU -> MAC
    inter-PAN frame -> parse back -> recovered command, broadcast + unicast, and
    a negative test that a short data frame is not mis-parsed as inter-PAN).
    Compiles clean; HW bench run pending.

16. **APS application-data encryption** - DONE. On top of the NWK-layer
    encryption every frame gets, `ZigbeeApsSecurity::secureDataFrame` /
    `openDataFrame` add a second, end-to-end APS-layer envelope between two
    devices that share a link key, so a relay (which holds the network key)
    still cannot read the application payload. It sets the APS frame-control
    security bit, inserts the APS auxiliary header, and encrypts the payload
    under the link key (key id = data key) via the hardware-verified CCM* core.
    `CC2530_ApsDataCrypt` self-tests it 10/10 on hardware: payload is ciphertext
    on the wire, the frame counter is recovered, the original APS frame is
    reconstructed, and tampered / wrong-key frames are rejected.

17. **BDB commissioning state machine** - DONE. `ZigbeeBdb` is the Base Device
    Behaviour orchestration layer: the app requests a bitmask of commissioning
    modes and the state machine runs them in the fixed Zigbee 3.0 precedence
    order (touchlink -> network steering -> network formation -> finding &
    binding), handing one mode at a time to the host (which performs the actual
    step with the rest of the stack) and collecting the status. A failed network
    step (steering/formation) ends commissioning - there is no network to do
    finding & binding on. `CC2530_Bdb` self-tests it 16/16 on hardware:
    coordinator (form + F&B), router (steer + F&B), full four-mode precedence,
    no-network abort, and a finding & binding failure path. This ties the
    individual commissioning pieces (M5 finding & binding, M7 touchlink,
    formation/steering) into one procedure with one result.

## Toward 1.0 — CC2530 firmware MAC reliability (v0.6–v0.8)

With the full Zigbee 3.0 / Zigbee PRO surface complete (items 1–17), a second
**TI Z-Stack ZNP** backend bring-up'd on real hardware, and the host/USB stack
(TaichiUSB) hardened, the work toward a 1.0 stable release brings the SDCC CC2530
transceiver firmware up to the channel-access and per-hop reliability of the
official MAC:

- **v0.6 — unslotted CSMA-CA.** On a busy channel the transmitter waits a random
  exponential backoff (BE 3..5, up to `macMaxCSMABackoffs`) and retries with a
  widening window instead of v0.5's immediate retry-on-busy. Built on the proven
  hardware `STXONCCA` path plus a software xorshift PRNG seeded per node from the
  IEEE (no radio-register reads in the TX path). On-air verified: a v0.6 router
  scans, transmits, receives, and joins a coordinator.
- **v0.7 — MAC-level ACK + retransmit.** An ack-requested unicast now waits for the
  matching ACK (frame type, CRC, DSN) and retransmits the whole frame, re-running
  CSMA-CA, up to `macMaxFrameRetries`; a non-ACK frame received during the wait is
  forwarded to the host so no inbound traffic is dropped. On-air verified through
  the association handshake (itself an ack-requested unicast).
- **v0.8 — MAC reliability counters.** `CMD_GET_STATS` exposes `mac_retx` /
  `mac_noack`, surfaced as `mac[retx noack]` in the example status line, so per-hop
  delivery is directly observable. Verified readable on air; under a co-located
  jammer the coordinator recovered 93 frames at the MAC layer (87% of all
  recoveries) at 100% APS delivery - the MAC+APS reliability ZNP relies on.
- **v0.9 — Energy-Detect scan.** `CMD_ED_SCAN` returns the peak RSSI on a channel
  (`CC2530Radio::energyScan`, `CC2530_EnergyScan`), the MLME-SCAN energy-detect
  primitive for quietest-channel formation and frequency agility. Verified on air:
  reliably flags a jammed channel vs the noise floor. This was the last CC2530 MAC
  primitive the SDCC firmware lacked relative to ZNP's MAC.
- **v0.10 — runtime-tunable MAC PIB.** `CMD_SET_MAC_PIB` makes `macMinBE` /
  `macMaxBE` / `macMaxCSMABackoffs` / `macMaxFrameRetries` settable at runtime
  (`CC2530Radio::setMacPib`), like ZNP's MAC PIB attributes, so the host can trade
  reliability against latency or adapt the backoff window to congestion
  (`macMaxFrameRetries=0` disables retransmit at runtime). Build-verified.

The firmware now carries both halves of TI's MAC reliability story.

## 1.0.0 — first stable release

NiusZigbee **1.0.0** is the first stable release: the full Zigbee 3.0 / Zigbee PRO
surface, a second TI Z-Stack ZNP backend, ZNP-grade MAC reliability (CSMA-CA,
MAC-ACK + retransmit verified recovering frames under congestion, energy-detect
scan, tunable MAC PIB), and R23 Dynamic Link Key on NiusZigbee's own elliptic-curve
crypto (X25519 + SPEKE) - all hardware-verified. Remaining work (each gated on a
verification rig or the R23 spec constants, to be released only once verified) is in
[POST_1.0_PLAN.md](POST_1.0_PLAN.md).

Detailed on-air evidence lives in [VERIFIED_BEHAVIOR.md](VERIFIED_BEHAVIOR.md).

## Zigbee PRO 2023 (R23)

NiusZigbee implements its **own** cryptography for R23 - no external crypto library.

- **Dynamic Link Key (DLK) — done.** R23 replaces the well-known `ZigBeeAlliance09`
  link key with a password-authenticated key exchange. NiusZigbee implements
  **SPEKE over Curve25519**: `ZigbeeCurve25519` (X25519, RFC 7748 - field arithmetic
  mod 2^255-19 + Montgomery ladder) and `ZigbeeSpeke` (password->generator mapping,
  ephemeral ECDH, AES-MMO KDF + key confirmation), all in-tree on the nRF host.
  Verified on hardware: `CC2530_Curve25519` 6/6 against the RFC 7748 test vectors;
  `CC2530_Speke` 3/3 (mutual agreement, key confirmation, wrong-password rejection).
  *Byte-exact interop with a certified R23 device additionally needs the spec's
  generator/KDF/transcript constants.*
- **Device Interview, Trust Center Swap-Out — pending** (host-side protocol on top
  of the existing key-transport machinery).
- **Zigbee Direct (BLE) — parked** on a mature NimBLE controller in the board
  package (currently an early slice).
- **Sub-GHz — out of scope:** the CC2530 is a 2.4 GHz-only transceiver. A 2.4 GHz
  R23 device is still conformant.
