#include "ZigbeeNwk.h"

namespace nzb {

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
  frame.payload = &npdu[kBaseHeaderLen];
  frame.payloadLen = (uint8_t)(len - kBaseHeaderLen);
  return true;
}

}  // namespace nzb
