/*
  ZigbeeMac.h - small IEEE 802.15.4 MAC helpers used by NiusZigbee.

  This is intentionally the layer below a full Zigbee PRO stack. It builds and
  parses short-address data frames (PAN ID + 16-bit source/destination address),
  which is enough for structured CC2530-to-CC2530 links and gives future NWK /
  APS / ZCL code a real MAC envelope to sit on.
*/
#ifndef NIUS_ZIGBEE_MAC_H
#define NIUS_ZIGBEE_MAC_H

#include <Arduino.h>

namespace nzb {

enum MacFrameType : uint8_t {
  MAC_FRAME_BEACON = 0,
  MAC_FRAME_DATA = 1,
  MAC_FRAME_ACK = 2,
  MAC_FRAME_COMMAND = 3
};

struct MacDataFrame {
  bool valid;
  uint8_t frameType;
  uint8_t sequence;
  bool ackRequest;
  bool panIdCompression;
  uint16_t dstPanId;
  uint16_t dstShort;
  uint16_t srcPanId;
  uint16_t srcShort;
  const uint8_t* payload;
  uint8_t payloadLen;
};

class ZigbeeMac {
 public:
  static const uint8_t kMaxPsdu = 125;
  static const uint16_t kBroadcastPan = 0xFFFF;
  static const uint16_t kBroadcastShort = 0xFFFF;

  static uint8_t buildShortDataFrame(uint8_t* out, uint8_t outMax,
                                     uint16_t panId, uint16_t dstShort,
                                     uint16_t srcShort, uint8_t sequence,
                                     const uint8_t* payload,
                                     uint8_t payloadLen,
                                     bool ackRequest = false);

  static bool parseShortDataFrame(const uint8_t* psdu, uint8_t len,
                                  MacDataFrame& frame);

  static bool isBroadcastShort(uint16_t shortAddress) {
    return shortAddress == kBroadcastShort;
  }

 private:
  static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  }
  static void writeLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_MAC_H
