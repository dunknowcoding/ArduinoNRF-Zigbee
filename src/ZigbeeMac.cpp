#include "ZigbeeMac.h"

namespace nzb {

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

}  // namespace nzb
