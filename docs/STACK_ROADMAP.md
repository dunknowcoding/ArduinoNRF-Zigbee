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

1. **NWK security** - AES-CCM* frame protection. Frame counters exist in
   `ZigbeeNetwork`; the nRF52840 has hardware AES (ECB/CCM) drivers in the
   ArduinoNRF core that can encrypt host-side before handing PSDUs to the
   CC2530.
2. **Broadcast Transaction Table** - broadcast dedup/passive ack. Required
   for correct multi-hop RREQ/broadcast behavior (the current discovery
   table only dedups route requests).
3. **Indirect transmission / sleepy end devices** - a parent-side pending
   queue plus MAC Data Request handling so rx-off children can poll.
   Needs CC2530 firmware support for the pending bit in acks (or host
   emulation with relaxed timing).
4. **ZDO Mgmt_Lqi_rsp / Mgmt_Rtg_rsp** - expose our neighbor/route tables
   to standard Zigbee network-mapping tools.
5. **Persistence** - network identity, addresses, frame counters, bindings,
   and reporting config in nRF flash (NrfNvmc) so nodes survive reboots.
6. **Binding table + group addressing**, APS acknowledgements/retries, and
   APS fragmentation.
7. **PAN ID conflict resolution and network update** (updateId / channel
   change propagation).
8. **Multi-hop relay verification** - the intermediate-router code paths
   (RREQ rebroadcast, RREP relay) compile and follow the spec but need a
   third board+module to verify over the air.
