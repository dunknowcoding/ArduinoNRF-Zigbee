#include "ZigbeeZcl.h"

namespace nzb {

uint8_t ZigbeeZcl::buildCommandFrame(uint8_t* out, uint8_t outMax,
                                     uint8_t frameType, uint8_t sequence,
                                     uint8_t commandId,
                                     const uint8_t* payload,
                                     uint8_t payloadLen,
                                     uint8_t direction,
                                     bool disableDefaultResponse) {
  if (!out) return 0;
  if (frameType > ZCL_FRAME_CLUSTER_SPECIFIC) return 0;
  if (direction > ZCL_DIRECTION_SERVER_TO_CLIENT) return 0;
  if (payloadLen > kMaxPayload) return 0;
  if (outMax < kBaseHeaderLen + payloadLen) return 0;
  if (payloadLen > 0 && !payload) return 0;

  uint8_t fcf = (uint8_t)(frameType & 0x03);
  if (direction == ZCL_DIRECTION_SERVER_TO_CLIENT) fcf |= 1u << 3;
  if (disableDefaultResponse) fcf |= 1u << 4;

  out[0] = fcf;
  out[1] = sequence;
  out[2] = commandId;
  for (uint8_t i = 0; i < payloadLen; ++i) {
    out[kBaseHeaderLen + i] = payload[i];
  }
  return (uint8_t)(kBaseHeaderLen + payloadLen);
}

bool ZigbeeZcl::parseFrame(const uint8_t* zcl, uint8_t len, ZclFrame& frame) {
  frame = ZclFrame();
  frame.payload = nullptr;

  if (!zcl || len < kBaseHeaderLen) return false;

  uint8_t fcf = zcl[0];
  uint8_t frameType = (uint8_t)(fcf & 0x03);
  bool manufacturerSpecific = (fcf & (1u << 2)) != 0;
  uint8_t offset = 1;

  if (frameType > ZCL_FRAME_CLUSTER_SPECIFIC) return false;
  frame.manufacturerCode = 0;
  if (manufacturerSpecific) {
    if (len < kManufacturerHeaderLen) return false;
    frame.manufacturerCode = readLe16(&zcl[offset]);
    offset += 2;
  }

  if (len < offset + 2) return false;

  frame.valid = true;
  frame.frameType = frameType;
  frame.manufacturerSpecific = manufacturerSpecific;
  frame.direction = (fcf & (1u << 3)) ? ZCL_DIRECTION_SERVER_TO_CLIENT
                                      : ZCL_DIRECTION_CLIENT_TO_SERVER;
  frame.disableDefaultResponse = (fcf & (1u << 4)) != 0;
  frame.sequence = zcl[offset++];
  frame.commandId = zcl[offset++];
  frame.payload = &zcl[offset];
  frame.payloadLen = (uint8_t)(len - offset);
  return true;
}

}  // namespace nzb
