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
  if (deliveryMode != APS_DELIVERY_UNICAST || security || extendedHeader) {
    return false;
  }

  frame.valid = true;
  frame.frameType = frameType;
  frame.deliveryMode = deliveryMode;
  frame.ackRequest = (fcf & (1u << 6)) != 0;
  frame.security = security;
  frame.extendedHeader = extendedHeader;
  frame.dstEndpoint = apdu[1];
  frame.clusterId = readLe16(&apdu[2]);
  frame.profileId = readLe16(&apdu[4]);
  frame.srcEndpoint = apdu[6];
  frame.counter = apdu[7];
  frame.payload = &apdu[kBaseHeaderLen];
  frame.payloadLen = (uint8_t)(len - kBaseHeaderLen);
  return true;
}

}  // namespace nzb
