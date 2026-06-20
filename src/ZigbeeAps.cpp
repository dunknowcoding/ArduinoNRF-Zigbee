#include "ZigbeeAps.h"

namespace nzb {

uint8_t ZigbeeAps::buildDataFrame(uint8_t* out, uint8_t outMax,
                                  uint8_t dstEndpoint, uint16_t clusterId,
                                  uint16_t profileId, uint8_t srcEndpoint,
                                  uint8_t counter, const uint8_t* payload,
                                  uint8_t payloadLen, bool ackRequest) {
  if (!out) return 0;
  if (payloadLen > kMaxPayload) return 0;
  if (outMax < kBaseHeaderLen + payloadLen) return 0;
  if (payloadLen > 0 && !payload) return 0;

  uint8_t fcf = APS_FRAME_DATA;
  fcf |= (uint8_t)(APS_DELIVERY_UNICAST << 2);
  if (ackRequest) fcf |= 1u << 6;

  out[0] = fcf;
  out[1] = dstEndpoint;
  writeLe16(&out[2], clusterId);
  writeLe16(&out[4], profileId);
  out[6] = srcEndpoint;
  out[7] = counter;
  for (uint8_t i = 0; i < payloadLen; ++i) {
    out[kBaseHeaderLen + i] = payload[i];
  }
  return (uint8_t)(kBaseHeaderLen + payloadLen);
}

uint8_t ZigbeeAps::buildGroupDataFrame(uint8_t* out, uint8_t outMax,
                                       uint16_t groupAddress, uint16_t clusterId,
                                       uint16_t profileId, uint8_t srcEndpoint,
                                       uint8_t counter, const uint8_t* payload,
                                       uint8_t payloadLen, bool ackRequest) {
  if (!out) return 0;
  if (payloadLen > (uint8_t)(kMaxFrame - kGroupHeaderLen)) return 0;
  if (outMax < (uint8_t)(kGroupHeaderLen + payloadLen)) return 0;
  if (payloadLen > 0 && !payload) return 0;

  uint8_t fcf = APS_FRAME_DATA;
  fcf |= (uint8_t)(APS_DELIVERY_GROUP << 2);
  if (ackRequest) fcf |= 1u << 6;

  out[0] = fcf;
  writeLe16(&out[1], groupAddress);
  writeLe16(&out[3], clusterId);
  writeLe16(&out[5], profileId);
  out[7] = srcEndpoint;
  out[8] = counter;
  for (uint8_t i = 0; i < payloadLen; ++i) {
    out[kGroupHeaderLen + i] = payload[i];
  }
  return (uint8_t)(kGroupHeaderLen + payloadLen);
}

uint8_t ZigbeeAps::frameType(const uint8_t* apdu, uint8_t len) {
  if (!apdu || len < 1) return 0xFF;
  return (uint8_t)(apdu[0] & 0x03);
}

uint8_t ZigbeeAps::buildAckFrame(uint8_t* out, uint8_t outMax,
                                 uint8_t ackDstEndpoint, uint16_t clusterId,
                                 uint16_t profileId, uint8_t ackSrcEndpoint,
                                 uint8_t counter) {
  if (!out || outMax < kBaseHeaderLen) return 0;

  uint8_t fcf = APS_FRAME_ACK;
  fcf |= (uint8_t)(APS_DELIVERY_UNICAST << 2);

  out[0] = fcf;
  out[1] = ackDstEndpoint;
  writeLe16(&out[2], clusterId);
  writeLe16(&out[4], profileId);
  out[6] = ackSrcEndpoint;
  out[7] = counter;
  return kBaseHeaderLen;
}

bool ZigbeeAps::parseAckFrame(const uint8_t* apdu, uint8_t len,
                              ApsAckFrame& frame) {
  frame = ApsAckFrame();
  if (!apdu || len < kBaseHeaderLen) return false;

  uint8_t fcf = apdu[0];
  if ((uint8_t)(fcf & 0x03) != APS_FRAME_ACK) return false;

  frame.valid = true;
  frame.dstEndpoint = apdu[1];
  frame.clusterId = readLe16(&apdu[2]);
  frame.profileId = readLe16(&apdu[4]);
  frame.srcEndpoint = apdu[6];
  frame.counter = apdu[7];
  return true;
}

bool ZigbeeAps::parseDataFrame(const uint8_t* apdu, uint8_t len,
                               ApsDataFrame& frame) {
  frame = ApsDataFrame();
  frame.payload = nullptr;

  if (!apdu || len < kBaseHeaderLen) return false;

  uint8_t fcf = apdu[0];
  uint8_t frameType = (uint8_t)(fcf & 0x03);
  uint8_t deliveryMode = (uint8_t)((fcf >> 2) & 0x03);
  bool security = (fcf & (1u << 5)) != 0;
  bool extendedHeader = (fcf & (1u << 7)) != 0;

  if (frameType != APS_FRAME_DATA) return false;
  if (security) return false;  // APS-layer security envelope not handled here
  if (deliveryMode != APS_DELIVERY_UNICAST && deliveryMode != APS_DELIVERY_GROUP) {
    return false;
  }

  frame.valid = true;
  frame.frameType = frameType;
  frame.deliveryMode = deliveryMode;
  frame.ackRequest = (fcf & (1u << 6)) != 0;
  frame.security = security;
  frame.extendedHeader = extendedHeader;

  uint8_t hdr;
  if (deliveryMode == APS_DELIVERY_GROUP) {
    // FCF(1) group(2) cluster(2) profile(2) srcEp(1) counter(1) = 9.
    if (len < kGroupHeaderLen) return false;
    frame.groupAddress = readLe16(&apdu[1]);
    frame.clusterId = readLe16(&apdu[3]);
    frame.profileId = readLe16(&apdu[5]);
    frame.srcEndpoint = apdu[7];
    frame.counter = apdu[8];
    hdr = kGroupHeaderLen;
  } else {
    frame.dstEndpoint = apdu[1];
    frame.clusterId = readLe16(&apdu[2]);
    frame.profileId = readLe16(&apdu[4]);
    frame.srcEndpoint = apdu[6];
    frame.counter = apdu[7];
    hdr = kBaseHeaderLen;
  }
  // APS extended header (fragmentation): extFCF(1) + blockNumber(1) follow the
  // base header, then the block payload. extFCF fragmentation bits: 01=first
  // block (blockNumber = total count), 10=subsequent (blockNumber = index).
  if (extendedHeader) {
    if ((uint16_t)hdr + 2u > len) return false;
    frame.firstBlock = (uint8_t)(apdu[hdr] & 0x03) == 0x01;
    frame.blockNumber = apdu[hdr + 1];
    hdr = (uint8_t)(hdr + 2);
  }
  frame.payload = &apdu[hdr];
  frame.payloadLen = (uint8_t)(len - hdr);
  return true;
}

}  // namespace nzb
