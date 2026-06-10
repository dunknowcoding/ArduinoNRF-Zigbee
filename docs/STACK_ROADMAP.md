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
     child acceptance, and parent bookkeeping before the over-the-air join
     command exchange.
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
boolean report scheduler, and local network-state/permit-join/address-allocation
helpers, while preserving the existing raw send / receive / sniffer APIs. It is
still not a full Zigbee PRO stack: there is no over-the-air association / join
exchange, neighbor aging protocol, automatic route discovery, binding,
persistent reporting table, persistent attribute storage, full cluster library,
or Zigbee security yet.
