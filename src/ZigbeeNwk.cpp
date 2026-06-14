#include "ZigbeeNwk.h"

namespace nzb {

uint64_t ZigbeeNwk::readLe64(const uint8_t* p) {
  uint64_t v = 0;
  for (uint8_t i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (8 * i);
  return v;
}

void ZigbeeNwk::writeLe64(uint8_t* p, uint64_t v) {
  for (uint8_t i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

uint8_t ZigbeeNwk::buildDataFrame(uint8_t* out, uint8_t outMax,
                                  uint16_t dstShort, uint16_t srcShort,
                                  uint8_t radius, uint8_t sequence,
                                  const uint8_t* payload,
                                  uint8_t payloadLen,
                                  uint8_t discoverRoute) {
  if (!out) return 0;
  if (payloadLen > kMaxPayload) return 0;
  if (outMax < kBaseHeaderLen + payloadLen) return 0;
  if (payloadLen > 0 && !payload) return 0;
  if (discoverRoute > 3) return 0;

  uint16_t fcf = 0;
  fcf |= NWK_FRAME_DATA;                       // frame type
  fcf |= (uint16_t)kProtocolVersion << 2;      // protocol version
  fcf |= (uint16_t)(discoverRoute & 0x03) << 6;

  writeLe16(&out[0], fcf);
  writeLe16(&out[2], dstShort);
  writeLe16(&out[4], srcShort);
  out[6] = radius;
  out[7] = sequence;
  for (uint8_t i = 0; i < payloadLen; ++i) {
    out[kBaseHeaderLen + i] = payload[i];
  }
  return (uint8_t)(kBaseHeaderLen + payloadLen);
}

uint8_t ZigbeeNwk::buildDataFrameSourceRouted(
    uint8_t* out, uint8_t outMax, uint16_t dstShort, uint16_t srcShort,
    uint8_t radius, uint8_t sequence, const uint16_t* relays,
    uint8_t relayCount, uint8_t relayIndex, const uint8_t* payload,
    uint8_t payloadLen, uint8_t discoverRoute) {
  if (!out || discoverRoute > 3) return 0;
  if (relayCount > 0 && !relays) return 0;
  if (payloadLen > 0 && !payload) return 0;
  uint8_t subframeLen = (uint8_t)(2 + relayCount * 2);  // count + index + relays
  uint8_t headerLen = (uint8_t)(kBaseHeaderLen + subframeLen);
  if ((uint16_t)headerLen + payloadLen > outMax) return 0;
  if (payloadLen > kMaxPayload) return 0;

  uint16_t fcf = 0;
  fcf |= NWK_FRAME_DATA;
  fcf |= (uint16_t)kProtocolVersion << 2;
  fcf |= (uint16_t)(discoverRoute & 0x03) << 6;
  fcf |= (uint16_t)1 << 10;  // source route present

  writeLe16(&out[0], fcf);
  writeLe16(&out[2], dstShort);
  writeLe16(&out[4], srcShort);
  out[6] = radius;
  out[7] = sequence;
  out[8] = relayCount;
  out[9] = relayIndex;
  for (uint8_t i = 0; i < relayCount; ++i) writeLe16(&out[10 + i * 2], relays[i]);
  for (uint8_t i = 0; i < payloadLen; ++i) out[headerLen + i] = payload[i];
  return (uint8_t)(headerLen + payloadLen);
}

bool ZigbeeNwk::parseDataFrame(const uint8_t* npdu, uint8_t len,
                               NwkDataFrame& frame) {
  frame = NwkDataFrame();
  frame.payload = nullptr;

  if (!npdu || len < kBaseHeaderLen) return false;

  uint16_t fcf = readLe16(&npdu[0]);
  uint8_t frameType = (uint8_t)(fcf & 0x03);
  bool multicast = (fcf & (1u << 8)) != 0;
  bool security = (fcf & (1u << 9)) != 0;
  bool sourceRoute = (fcf & (1u << 10)) != 0;
  bool dstIeeePresent = (fcf & (1u << 11)) != 0;
  bool srcIeeePresent = (fcf & (1u << 12)) != 0;

  if (frameType != NWK_FRAME_DATA) return false;
  // The other NWK header extensions (multicast control, IEEE addresses) and
  // pre-decryption security are still out of scope, but a source-route subframe
  // is parsed below.
  if (multicast || security || dstIeeePresent || srcIeeePresent) {
    return false;
  }

  uint8_t offset = kBaseHeaderLen;
  uint8_t srRelayCount = 0, srRelayIndex = 0;
  const uint8_t* srRelayList = nullptr;
  if (sourceRoute) {
    if (len < (uint8_t)(offset + 2)) return false;
    srRelayCount = npdu[offset];
    srRelayIndex = npdu[offset + 1];
    uint8_t subframeLen = (uint8_t)(2 + srRelayCount * 2);
    if (len < (uint8_t)(offset + subframeLen)) return false;
    srRelayList = &npdu[offset + 2];
    offset = (uint8_t)(offset + subframeLen);
  }

  frame.valid = true;
  frame.frameType = frameType;
  frame.protocolVersion = (uint8_t)((fcf >> 2) & 0x0F);
  frame.discoverRoute = (uint8_t)((fcf >> 6) & 0x03);
  frame.multicast = multicast;
  frame.security = security;
  frame.sourceRoute = sourceRoute;
  frame.dstIeeePresent = dstIeeePresent;
  frame.srcIeeePresent = srcIeeePresent;
  frame.endDeviceInitiator = (fcf & (1u << 13)) != 0;
  frame.dstShort = readLe16(&npdu[2]);
  frame.srcShort = readLe16(&npdu[4]);
  frame.radius = npdu[6];
  frame.sequence = npdu[7];
  frame.srRelayCount = srRelayCount;
  frame.srRelayIndex = srRelayIndex;
  frame.srRelayList = srRelayList;
  frame.payload = &npdu[offset];
  frame.payloadLen = (uint8_t)(len - offset);
  return true;
}

bool ZigbeeNwk::getDataFrameRelay(const NwkDataFrame& frame, uint8_t index,
                                  uint16_t& relay) {
  if (!frame.valid || !frame.sourceRoute || !frame.srRelayList ||
      index >= frame.srRelayCount) {
    return false;
  }
  relay = readLe16(&frame.srRelayList[index * 2]);
  return true;
}

uint8_t ZigbeeNwk::buildCommandFrame(uint8_t* out, uint8_t outMax,
                                     uint16_t dstShort, uint16_t srcShort,
                                     uint8_t radius, uint8_t sequence,
                                     uint8_t commandId,
                                     const uint8_t* payload,
                                     uint8_t payloadLen,
                                     uint8_t discoverRoute) {
  if (!out) return 0;
  if (payloadLen > (uint8_t)(kMaxPayload - 1)) return 0;
  if (outMax < kBaseHeaderLen + 1 + payloadLen) return 0;
  if (payloadLen > 0 && !payload) return 0;
  if (discoverRoute > 3) return 0;

  uint16_t fcf = 0;
  fcf |= NWK_FRAME_COMMAND;
  fcf |= (uint16_t)kProtocolVersion << 2;
  fcf |= (uint16_t)(discoverRoute & 0x03) << 6;

  writeLe16(&out[0], fcf);
  writeLe16(&out[2], dstShort);
  writeLe16(&out[4], srcShort);
  out[6] = radius;
  out[7] = sequence;
  out[8] = commandId;
  for (uint8_t i = 0; i < payloadLen; ++i) {
    out[kBaseHeaderLen + 1 + i] = payload[i];
  }
  return (uint8_t)(kBaseHeaderLen + 1 + payloadLen);
}

bool ZigbeeNwk::parseCommandFrame(const uint8_t* npdu, uint8_t len,
                                  NwkCommandFrame& frame) {
  frame = NwkCommandFrame();
  frame.payload = nullptr;

  if (!npdu || len < kBaseHeaderLen + 1) return false;

  uint16_t fcf = readLe16(&npdu[0]);
  uint8_t frameType = (uint8_t)(fcf & 0x03);
  bool multicast = (fcf & (1u << 8)) != 0;
  bool security = (fcf & (1u << 9)) != 0;
  bool sourceRoute = (fcf & (1u << 10)) != 0;
  bool dstIeeePresent = (fcf & (1u << 11)) != 0;
  bool srcIeeePresent = (fcf & (1u << 12)) != 0;

  if (frameType != NWK_FRAME_COMMAND) return false;
  if (multicast || security || sourceRoute || dstIeeePresent || srcIeeePresent) {
    return false;
  }

  frame.valid = true;
  frame.frameType = frameType;
  frame.protocolVersion = (uint8_t)((fcf >> 2) & 0x0F);
  frame.discoverRoute = (uint8_t)((fcf >> 6) & 0x03);
  frame.multicast = multicast;
  frame.security = security;
  frame.sourceRoute = sourceRoute;
  frame.dstIeeePresent = dstIeeePresent;
  frame.srcIeeePresent = srcIeeePresent;
  frame.endDeviceInitiator = (fcf & (1u << 13)) != 0;
  frame.dstShort = readLe16(&npdu[2]);
  frame.srcShort = readLe16(&npdu[4]);
  frame.radius = npdu[6];
  frame.sequence = npdu[7];
  frame.commandId = npdu[8];
  frame.payload = &npdu[kBaseHeaderLen + 1];
  frame.payloadLen = (uint8_t)(len - kBaseHeaderLen - 1);
  return true;
}

uint8_t ZigbeeNwk::buildRouteRequestPayload(
    uint8_t* out, uint8_t outMax, uint8_t routeRequestId,
    uint16_t destination, uint8_t pathCost, bool manyToOne,
    const uint64_t* destinationIeee, bool multicast) {
  uint8_t n = destinationIeee ? 13 : 5;
  if (!out || outMax < n) return 0;
  uint8_t options = 0;
  if (manyToOne) options |= 0x08;
  if (destinationIeee) options |= 0x10;
  if (multicast) options |= 0x20;
  out[0] = options;
  out[1] = routeRequestId;
  writeLe16(&out[2], destination);
  out[4] = pathCost;
  if (destinationIeee) writeLe64(&out[5], *destinationIeee);
  return n;
}

bool ZigbeeNwk::parseRouteRequestPayload(
    const uint8_t* payload, uint8_t payloadLen,
    NwkRouteRequestCommand& command) {
  command = NwkRouteRequestCommand();
  if (!payload || payloadLen < 5) return false;
  uint8_t options = payload[0];
  command.valid = true;
  command.manyToOne = (options & 0x08) != 0;
  command.destinationIeeePresent = (options & 0x10) != 0;
  command.multicast = (options & 0x20) != 0;
  command.routeRequestId = payload[1];
  command.destination = readLe16(&payload[2]);
  command.pathCost = payload[4];
  if (command.destinationIeeePresent) {
    if (payloadLen < 13) return false;
    command.destinationIeee = readLe64(&payload[5]);
  }
  return true;
}

uint8_t ZigbeeNwk::buildRouteReplyPayload(
    uint8_t* out, uint8_t outMax, uint8_t routeRequestId,
    uint16_t originator, uint16_t responder, uint8_t pathCost,
    const uint64_t* originatorIeee, const uint64_t* responderIeee,
    bool multicast) {
  uint8_t n = 7;
  if (originatorIeee) n += 8;
  if (responderIeee) n += 8;
  if (!out || outMax < n) return 0;
  uint8_t options = 0;
  if (originatorIeee) options |= 0x10;
  if (responderIeee) options |= 0x20;
  if (multicast) options |= 0x40;
  out[0] = options;
  out[1] = routeRequestId;
  writeLe16(&out[2], originator);
  writeLe16(&out[4], responder);
  out[6] = pathCost;
  uint8_t idx = 7;
  if (originatorIeee) {
    writeLe64(&out[idx], *originatorIeee);
    idx += 8;
  }
  if (responderIeee) {
    writeLe64(&out[idx], *responderIeee);
    idx += 8;
  }
  return idx;
}

bool ZigbeeNwk::parseRouteReplyPayload(const uint8_t* payload,
                                       uint8_t payloadLen,
                                       NwkRouteReplyCommand& command) {
  command = NwkRouteReplyCommand();
  if (!payload || payloadLen < 7) return false;
  uint8_t options = payload[0];
  command.valid = true;
  command.originatorIeeePresent = (options & 0x10) != 0;
  command.responderIeeePresent = (options & 0x20) != 0;
  command.multicast = (options & 0x40) != 0;
  command.routeRequestId = payload[1];
  command.originator = readLe16(&payload[2]);
  command.responder = readLe16(&payload[4]);
  command.pathCost = payload[6];
  uint8_t idx = 7;
  if (command.originatorIeeePresent) {
    if (payloadLen < idx + 8) return false;
    command.originatorIeee = readLe64(&payload[idx]);
    idx += 8;
  }
  if (command.responderIeeePresent) {
    if (payloadLen < idx + 8) return false;
    command.responderIeee = readLe64(&payload[idx]);
  }
  return true;
}

uint8_t ZigbeeNwk::buildNetworkStatusPayload(uint8_t* out, uint8_t outMax,
                                             uint8_t status,
                                             uint16_t destination) {
  if (!out || outMax < 3) return 0;
  out[0] = status;
  writeLe16(&out[1], destination);
  return 3;
}

bool ZigbeeNwk::parseNetworkStatusPayload(
    const uint8_t* payload, uint8_t payloadLen,
    NwkNetworkStatusCommand& command) {
  command = NwkNetworkStatusCommand();
  if (!payload || payloadLen < 3) return false;
  command.valid = true;
  command.status = payload[0];
  command.destination = readLe16(&payload[1]);
  return true;
}

uint8_t ZigbeeNwk::buildRouteRecordPayload(uint8_t* out, uint8_t outMax,
                                           const uint16_t* relays,
                                           uint8_t relayCount) {
  if (!out || outMax < (uint8_t)(1 + relayCount * 2)) return 0;
  if (relayCount > 0 && !relays) return 0;
  out[0] = relayCount;
  for (uint8_t i = 0; i < relayCount; ++i) {
    writeLe16(&out[1 + i * 2], relays[i]);
  }
  return (uint8_t)(1 + relayCount * 2);
}

bool ZigbeeNwk::parseRouteRecordPayload(const uint8_t* payload,
                                        uint8_t payloadLen,
                                        NwkRouteRecordCommand& command) {
  command = NwkRouteRecordCommand();
  command.relayList = nullptr;
  if (!payload || payloadLen < 1) return false;
  command.relayCount = payload[0];
  if (payloadLen < (uint8_t)(1 + command.relayCount * 2)) return false;
  command.valid = true;
  command.relayList = &payload[1];
  return true;
}

bool ZigbeeNwk::getRouteRecordRelay(const NwkRouteRecordCommand& command,
                                    uint8_t index, uint16_t& relay) {
  if (!command.valid || !command.relayList || index >= command.relayCount) {
    return false;
  }
  relay = readLe16(&command.relayList[index * 2]);
  return true;
}

uint8_t ZigbeeNwk::buildLeavePayload(uint8_t* out, uint8_t outMax,
                                     bool rejoin, bool request,
                                     bool removeChildren) {
  if (!out || outMax < 1) return 0;
  out[0] = 0;
  if (rejoin) out[0] |= 0x20;
  if (request) out[0] |= 0x40;
  if (removeChildren) out[0] |= 0x80;
  return 1;
}

bool ZigbeeNwk::parseLeavePayload(const uint8_t* payload,
                                  uint8_t payloadLen,
                                  NwkLeaveCommand& command) {
  command = NwkLeaveCommand();
  if (!payload || payloadLen < 1) return false;
  command.valid = true;
  command.rejoin = (payload[0] & 0x20) != 0;
  command.request = (payload[0] & 0x40) != 0;
  command.removeChildren = (payload[0] & 0x80) != 0;
  return true;
}

uint8_t ZigbeeNwk::buildRejoinRequestPayload(uint8_t* out, uint8_t outMax,
                                             uint8_t capability) {
  if (!out || outMax < 1) return 0;
  out[0] = capability;
  return 1;
}

bool ZigbeeNwk::parseRejoinRequestPayload(
    const uint8_t* payload, uint8_t payloadLen,
    NwkRejoinRequestCommand& command) {
  command = NwkRejoinRequestCommand();
  if (!payload || payloadLen < 1) return false;
  command.valid = true;
  command.capability = payload[0];
  return true;
}

uint8_t ZigbeeNwk::buildRejoinResponsePayload(uint8_t* out, uint8_t outMax,
                                              uint16_t nwkAddress,
                                              uint8_t status) {
  if (!out || outMax < 3) return 0;
  writeLe16(&out[0], nwkAddress);
  out[2] = status;
  return 3;
}

bool ZigbeeNwk::parseRejoinResponsePayload(
    const uint8_t* payload, uint8_t payloadLen,
    NwkRejoinResponseCommand& command) {
  command = NwkRejoinResponseCommand();
  if (!payload || payloadLen < 3) return false;
  command.valid = true;
  command.nwkAddress = readLe16(&payload[0]);
  command.status = payload[2];
  return true;
}

uint8_t ZigbeeNwk::buildLinkStatusPayload(uint8_t* out, uint8_t outMax,
                                          const NwkLinkStatusEntry* entries,
                                          uint8_t entryCount,
                                          bool firstFrame, bool lastFrame) {
  if (!out || (entryCount > 0 && !entries)) return 0;
  if (entryCount > 31) return 0;  // 5-bit count field
  uint8_t needed = (uint8_t)(1 + 3 * entryCount);
  if (outMax < needed) return 0;

  out[0] = (uint8_t)(entryCount & 0x1F);
  if (firstFrame) out[0] |= 0x20;
  if (lastFrame) out[0] |= 0x40;
  for (uint8_t i = 0; i < entryCount; ++i) {
    writeLe16(&out[1 + 3 * i], entries[i].address);
    out[3 + 3 * i] = (uint8_t)((entries[i].incomingCost & 0x07) |
                               ((entries[i].outgoingCost & 0x07) << 4));
  }
  return needed;
}

bool ZigbeeNwk::parseLinkStatusPayload(const uint8_t* payload,
                                       uint8_t payloadLen,
                                       NwkLinkStatusCommand& command) {
  command = NwkLinkStatusCommand();
  if (!payload || payloadLen < 1) return false;
  uint8_t count = (uint8_t)(payload[0] & 0x1F);
  if (payloadLen < 1 + 3 * count) return false;
  command.valid = true;
  command.firstFrame = (payload[0] & 0x20) != 0;
  command.lastFrame = (payload[0] & 0x40) != 0;
  command.entryCount = count;
  command.entries = &payload[1];
  return true;
}

bool ZigbeeNwk::getLinkStatusEntry(const NwkLinkStatusCommand& command,
                                   uint8_t index, NwkLinkStatusEntry& entry) {
  entry = NwkLinkStatusEntry();
  if (!command.valid || index >= command.entryCount || !command.entries) {
    return false;
  }
  const uint8_t* p = &command.entries[3 * index];
  entry.address = readLe16(&p[0]);
  entry.incomingCost = (uint8_t)(p[2] & 0x07);
  entry.outgoingCost = (uint8_t)((p[2] >> 4) & 0x07);
  return true;
}

uint8_t ZigbeeNwk::buildBeaconPayload(uint8_t* out, uint8_t outMax,
                                      uint64_t extendedPanId,
                                      uint8_t deviceDepth,
                                      bool routerCapacity,
                                      bool endDeviceCapacity,
                                      uint8_t updateId,
                                      uint8_t stackProfile) {
  if (!out || outMax < kBeaconPayloadLen) return 0;

  out[0] = kProtocolIdZigbee;
  out[1] = (uint8_t)((stackProfile & 0x0F) |
                     ((kProtocolVersion & 0x0F) << 4));
  out[2] = (uint8_t)(((routerCapacity ? 1 : 0) << 2) |
                     ((deviceDepth & 0x0F) << 3) |
                     ((endDeviceCapacity ? 1 : 0) << 7));
  writeLe64(&out[3], extendedPanId);
  out[11] = (uint8_t)(kBeaconlessTxOffset & 0xFF);
  out[12] = (uint8_t)((kBeaconlessTxOffset >> 8) & 0xFF);
  out[13] = (uint8_t)((kBeaconlessTxOffset >> 16) & 0xFF);
  out[14] = updateId;
  return kBeaconPayloadLen;
}

bool ZigbeeNwk::parseBeaconPayload(const uint8_t* payload, uint8_t payloadLen,
                                   NwkBeaconPayload& beacon) {
  beacon = NwkBeaconPayload();
  if (!payload || payloadLen < kBeaconPayloadLen) return false;
  if (payload[0] != kProtocolIdZigbee) return false;

  beacon.valid = true;
  beacon.protocolId = payload[0];
  beacon.stackProfile = (uint8_t)(payload[1] & 0x0F);
  beacon.protocolVersion = (uint8_t)((payload[1] >> 4) & 0x0F);
  beacon.routerCapacity = (payload[2] & (1u << 2)) != 0;
  beacon.deviceDepth = (uint8_t)((payload[2] >> 3) & 0x0F);
  beacon.endDeviceCapacity = (payload[2] & (1u << 7)) != 0;
  beacon.extendedPanId = readLe64(&payload[3]);
  beacon.txOffset = (uint32_t)payload[11] | ((uint32_t)payload[12] << 8) |
                    ((uint32_t)payload[13] << 16);
  beacon.updateId = payload[14];
  return true;
}

}  // namespace nzb
