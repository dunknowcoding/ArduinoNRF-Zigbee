#include "ZigbeeMac.h"

namespace nzb {

uint64_t ZigbeeMac::readLe64(const uint8_t* p) {
  uint64_t v = 0;
  for (uint8_t i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (8 * i);
  return v;
}

void ZigbeeMac::writeLe64(uint8_t* p, uint64_t v) {
  for (uint8_t i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

uint8_t ZigbeeMac::buildShortDataFrame(uint8_t* out, uint8_t outMax,
                                       uint16_t panId, uint16_t dstShort,
                                       uint16_t srcShort, uint8_t sequence,
                                       const uint8_t* payload,
                                       uint8_t payloadLen,
                                       bool ackRequest) {
  if (!out) return 0;

  // 802.15.4 data frame with short dst/src addresses and PAN compression:
  // FCF(2), seq(1), dst PAN(2), dst short(2), src short(2), payload...
  const uint8_t headerLen = 9;
  if (payloadLen > kMaxPsdu - headerLen) return 0;
  if (outMax < headerLen + payloadLen) return 0;
  if (payloadLen > 0 && !payload) return 0;

  uint16_t fcf = 0;
  fcf |= MAC_FRAME_DATA;       // frame type
  if (ackRequest) fcf |= 1u << 5;
  fcf |= 1u << 6;              // PAN ID compression
  fcf |= 2u << 10;             // destination address mode: 16-bit short
  fcf |= 1u << 12;             // frame version: IEEE 802.15.4-2006
  fcf |= 2u << 14;             // source address mode: 16-bit short

  writeLe16(&out[0], fcf);
  out[2] = sequence;
  writeLe16(&out[3], panId);
  writeLe16(&out[5], dstShort);
  writeLe16(&out[7], srcShort);
  for (uint8_t i = 0; i < payloadLen; ++i) {
    out[headerLen + i] = payload[i];
  }
  return (uint8_t)(headerLen + payloadLen);
}

bool ZigbeeMac::parseShortDataFrame(const uint8_t* psdu, uint8_t len,
                                    MacDataFrame& frame) {
  frame = MacDataFrame();
  frame.payload = nullptr;

  if (!psdu || len < 9) return false;

  uint16_t fcf = readLe16(&psdu[0]);
  uint8_t frameType = (uint8_t)(fcf & 0x7);
  uint8_t dstMode = (uint8_t)((fcf >> 10) & 0x3);
  uint8_t srcMode = (uint8_t)((fcf >> 14) & 0x3);
  bool panCompression = (fcf & (1u << 6)) != 0;

  if (frameType != MAC_FRAME_DATA || dstMode != 2 || srcMode != 2) {
    return false;
  }

  uint8_t offset = 2;
  frame.sequence = psdu[offset++];
  frame.dstPanId = readLe16(&psdu[offset]);
  offset += 2;
  frame.dstShort = readLe16(&psdu[offset]);
  offset += 2;
  if (panCompression) {
    frame.srcPanId = frame.dstPanId;
  } else {
    if (len < offset + 2) return false;
    frame.srcPanId = readLe16(&psdu[offset]);
    offset += 2;
  }
  if (len < offset + 2) return false;
  frame.srcShort = readLe16(&psdu[offset]);
  offset += 2;

  frame.valid = true;
  frame.frameType = frameType;
  frame.ackRequest = (fcf & (1u << 5)) != 0;
  frame.panIdCompression = panCompression;
  frame.payload = &psdu[offset];
  frame.payloadLen = (uint8_t)(len - offset);
  return true;
}

uint8_t ZigbeeMac::buildAssociationRequest(uint8_t* out, uint8_t outMax,
                                           uint16_t panId, uint16_t coordShort,
                                           uint64_t srcIeee, uint8_t sequence,
                                           uint8_t capability,
                                           bool ackRequest) {
  if (!out || outMax < 17) return 0;

  uint16_t fcf = 0;
  fcf |= MAC_FRAME_COMMAND;
  if (ackRequest) fcf |= 1u << 5;
  fcf |= 1u << 6;
  fcf |= (uint16_t)MAC_ADDR_SHORT << 10;
  fcf |= 1u << 12;
  fcf |= (uint16_t)MAC_ADDR_EXTENDED << 14;

  writeLe16(&out[0], fcf);
  out[2] = sequence;
  writeLe16(&out[3], panId);
  writeLe16(&out[5], coordShort);
  writeLe64(&out[7], srcIeee);
  out[15] = MAC_CMD_ASSOCIATION_REQUEST;
  out[16] = capability;
  return 17;
}

uint8_t ZigbeeMac::buildAssociationResponse(uint8_t* out, uint8_t outMax,
                                            uint16_t panId, uint64_t dstIeee,
                                            uint16_t srcShort,
                                            uint8_t sequence,
                                            uint16_t assignedShort,
                                            uint8_t status,
                                            bool ackRequest) {
  if (!out || outMax < 19) return 0;

  uint16_t fcf = 0;
  fcf |= MAC_FRAME_COMMAND;
  if (ackRequest) fcf |= 1u << 5;
  fcf |= 1u << 6;
  fcf |= (uint16_t)MAC_ADDR_EXTENDED << 10;
  fcf |= 1u << 12;
  fcf |= (uint16_t)MAC_ADDR_SHORT << 14;

  writeLe16(&out[0], fcf);
  out[2] = sequence;
  writeLe16(&out[3], panId);
  writeLe64(&out[5], dstIeee);
  writeLe16(&out[13], srcShort);
  out[15] = MAC_CMD_ASSOCIATION_RESPONSE;
  writeLe16(&out[16], assignedShort);
  out[18] = status;
  return 19;
}

bool ZigbeeMac::parseCommandFrame(const uint8_t* psdu, uint8_t len,
                                  MacCommandFrame& frame) {
  frame = MacCommandFrame();
  frame.payload = nullptr;

  if (!psdu || len < 4) return false;

  uint16_t fcf = readLe16(&psdu[0]);
  uint8_t frameType = (uint8_t)(fcf & 0x7);
  uint8_t dstMode = (uint8_t)((fcf >> 10) & 0x3);
  uint8_t srcMode = (uint8_t)((fcf >> 14) & 0x3);
  bool panCompression = (fcf & (1u << 6)) != 0;

  if (frameType != MAC_FRAME_COMMAND) return false;

  uint8_t offset = 2;
  frame.sequence = psdu[offset++];
  frame.dstAddrMode = dstMode;
  frame.srcAddrMode = srcMode;

  if (dstMode != MAC_ADDR_NONE) {
    if (len < offset + 2) return false;
    frame.dstPanId = readLe16(&psdu[offset]);
    offset += 2;
    if (dstMode == MAC_ADDR_SHORT) {
      if (len < offset + 2) return false;
      frame.dstShort = readLe16(&psdu[offset]);
      offset += 2;
    } else if (dstMode == MAC_ADDR_EXTENDED) {
      if (len < offset + 8) return false;
      frame.dstIeee = readLe64(&psdu[offset]);
      offset += 8;
    } else {
      return false;
    }
  }

  if (srcMode != MAC_ADDR_NONE) {
    if (panCompression) {
      frame.srcPanId = frame.dstPanId;
    } else {
      if (len < offset + 2) return false;
      frame.srcPanId = readLe16(&psdu[offset]);
      offset += 2;
    }
    if (srcMode == MAC_ADDR_SHORT) {
      if (len < offset + 2) return false;
      frame.srcShort = readLe16(&psdu[offset]);
      offset += 2;
    } else if (srcMode == MAC_ADDR_EXTENDED) {
      if (len < offset + 8) return false;
      frame.srcIeee = readLe64(&psdu[offset]);
      offset += 8;
    } else {
      return false;
    }
  }

  if (len < offset + 1) return false;
  frame.commandId = psdu[offset++];
  frame.valid = true;
  frame.frameType = frameType;
  frame.ackRequest = (fcf & (1u << 5)) != 0;
  frame.panIdCompression = panCompression;
  frame.payload = &psdu[offset];
  frame.payloadLen = (uint8_t)(len - offset);
  return true;
}

uint8_t ZigbeeMac::buildBeaconRequest(uint8_t* out, uint8_t outMax,
                                      uint8_t sequence) {
  if (!out || outMax < 8) return 0;

  // Broadcast MAC command with no source: FCF(2), seq(1), dst PAN(2),
  // dst short(2), command id(1). Never ack-requested (broadcast).
  uint16_t fcf = 0;
  fcf |= MAC_FRAME_COMMAND;
  fcf |= (uint16_t)MAC_ADDR_SHORT << 10;
  // frame version 2003 (0): scan probes must reach every conformant MAC.
  fcf |= (uint16_t)MAC_ADDR_NONE << 14;

  writeLe16(&out[0], fcf);
  out[2] = sequence;
  writeLe16(&out[3], kBroadcastPan);
  writeLe16(&out[5], kBroadcastShort);
  out[7] = MAC_CMD_BEACON_REQUEST;
  return 8;
}

uint8_t ZigbeeMac::buildDataRequest(uint8_t* out, uint8_t outMax, uint16_t panId,
                                    uint16_t parentShort, uint16_t childShort,
                                    uint8_t sequence) {
  if (!out || outMax < 10) return 0;

  // FCF(2), seq(1), dst PAN(2), dst short(2), src short(2), command id(1).
  // Ack requested so the parent's ack can carry the frame-pending bit; PAN-ID
  // compression on (child and parent share the PAN, so the src PAN is omitted).
  uint16_t fcf = 0;
  fcf |= MAC_FRAME_COMMAND;
  fcf |= 1u << 5;                          // ack request
  fcf |= 1u << 6;                          // PAN ID compression
  fcf |= (uint16_t)MAC_ADDR_SHORT << 10;   // dst short
  fcf |= (uint16_t)MAC_ADDR_SHORT << 14;   // src short

  writeLe16(&out[0], fcf);
  out[2] = sequence;
  writeLe16(&out[3], panId);
  writeLe16(&out[5], parentShort);  // dst = parent
  writeLe16(&out[7], childShort);   // src = child (no src PAN, compressed)
  out[9] = MAC_CMD_DATA_REQUEST;
  return 10;
}

uint8_t ZigbeeMac::buildBeacon(uint8_t* out, uint8_t outMax, uint16_t srcPanId,
                               uint16_t srcShort, uint8_t sequence,
                               bool panCoordinator, bool associationPermit,
                               const uint8_t* payload, uint8_t payloadLen) {
  if (!out) return 0;

  // Beaconless-PAN beacon: FCF(2), seq(1), src PAN(2), src short(2),
  // superframe spec(2), GTS(1, none), pending addresses(1, none), payload.
  const uint8_t headerLen = 11;
  if (payloadLen > kMaxPsdu - headerLen) return 0;
  if (outMax < headerLen + payloadLen) return 0;
  if (payloadLen > 0 && !payload) return 0;

  uint16_t fcf = 0;
  fcf |= MAC_FRAME_BEACON;
  // no destination, source PAN + short address, frame version 2003.
  fcf |= (uint16_t)MAC_ADDR_SHORT << 14;

  // Superframe spec for a beaconless PAN: beacon order 15, superframe
  // order 15, final CAP slot 15, battery extension 0.
  uint16_t superframe = 0x0FFF;
  if (panCoordinator) superframe |= 1u << 14;
  if (associationPermit) superframe |= 1u << 15;

  writeLe16(&out[0], fcf);
  out[2] = sequence;
  writeLe16(&out[3], srcPanId);
  writeLe16(&out[5], srcShort);
  writeLe16(&out[7], superframe);
  out[9] = 0;   // GTS spec: no GTS
  out[10] = 0;  // pending address spec: none
  for (uint8_t i = 0; i < payloadLen; ++i) {
    out[headerLen + i] = payload[i];
  }
  return (uint8_t)(headerLen + payloadLen);
}

bool ZigbeeMac::parseBeacon(const uint8_t* psdu, uint8_t len,
                            MacBeaconFrame& frame) {
  frame = MacBeaconFrame();
  frame.payload = nullptr;

  if (!psdu || len < 11) return false;

  uint16_t fcf = readLe16(&psdu[0]);
  uint8_t frameType = (uint8_t)(fcf & 0x7);
  uint8_t dstMode = (uint8_t)((fcf >> 10) & 0x3);
  uint8_t srcMode = (uint8_t)((fcf >> 14) & 0x3);

  if (frameType != MAC_FRAME_BEACON || dstMode != MAC_ADDR_NONE ||
      srcMode != MAC_ADDR_SHORT) {
    return false;
  }

  uint8_t offset = 2;
  frame.sequence = psdu[offset++];
  frame.srcPanId = readLe16(&psdu[offset]);
  offset += 2;
  frame.srcShort = readLe16(&psdu[offset]);
  offset += 2;

  uint16_t superframe = readLe16(&psdu[offset]);
  offset += 2;
  frame.panCoordinator = (superframe & (1u << 14)) != 0;
  frame.associationPermit = (superframe & (1u << 15)) != 0;

  // GTS spec: low 3 bits = descriptor count, each descriptor is 3 bytes
  // (plus the 1-byte direction mask when count > 0).
  uint8_t gtsCount = (uint8_t)(psdu[offset] & 0x07);
  offset += 1;
  if (gtsCount > 0) {
    uint8_t gtsBytes = (uint8_t)(1 + 3 * gtsCount);
    if (len < offset + gtsBytes) return false;
    offset += gtsBytes;
  }

  // Pending address spec: 3 bits short count, 3 bits extended count.
  if (len < offset + 1) return false;
  uint8_t pending = psdu[offset++];
  uint8_t pendingBytes =
      (uint8_t)(2 * (pending & 0x07) + 8 * ((pending >> 4) & 0x07));
  if (len < offset + pendingBytes) return false;
  offset += pendingBytes;

  frame.valid = true;
  frame.payload = &psdu[offset];
  frame.payloadLen = (uint8_t)(len - offset);
  return true;
}

bool ZigbeeMac::parseAssociationRequest(
    const MacCommandFrame& frame, MacAssociationRequest& request) {
  request = MacAssociationRequest();
  if (!frame.valid ||
      frame.commandId != MAC_CMD_ASSOCIATION_REQUEST ||
      frame.payloadLen < 1) {
    return false;
  }
  request.valid = true;
  request.capability = frame.payload[0];
  request.fullFunctionDevice = (request.capability & 0x02) != 0;
  request.receiverOnWhenIdle = (request.capability & 0x08) != 0;
  request.allocateAddress = (request.capability & 0x80) != 0;
  return true;
}

bool ZigbeeMac::parseAssociationResponse(
    const MacCommandFrame& frame, MacAssociationResponse& response) {
  response = MacAssociationResponse();
  if (!frame.valid ||
      frame.commandId != MAC_CMD_ASSOCIATION_RESPONSE ||
      frame.payloadLen < 3) {
    return false;
  }
  response.valid = true;
  response.shortAddress = readLe16(&frame.payload[0]);
  response.status = frame.payload[2];
  return true;
}

}  // namespace nzb
