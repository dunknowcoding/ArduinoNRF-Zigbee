/*
  ZigbeeNwk.h - small Zigbee NWK frame helpers used by NiusZigbee.

  This is frame tooling, not a full Zigbee PRO network stack. It builds and
  parses simple NWK data frames that fit inside the MAC data-frame payload
  produced by ZigbeeMac.
*/
#ifndef NIUS_ZIGBEE_NWK_H
#define NIUS_ZIGBEE_NWK_H

#include <Arduino.h>

namespace nzb {

enum NwkFrameType : uint8_t {
  NWK_FRAME_DATA = 0,
  NWK_FRAME_COMMAND = 1,
  NWK_FRAME_INTERPAN = 3
};

enum NwkDiscoverRoute : uint8_t {
  NWK_DISCOVER_SUPPRESS = 0,
  NWK_DISCOVER_ENABLE = 1
};

struct NwkDataFrame {
  bool valid;
  uint8_t frameType;
  uint8_t protocolVersion;
  uint8_t discoverRoute;
  bool multicast;
  bool security;
  bool sourceRoute;
  bool dstIeeePresent;
  bool srcIeeePresent;
  bool endDeviceInitiator;
  uint16_t dstShort;
  uint16_t srcShort;
  uint8_t radius;
  uint8_t sequence;
  const uint8_t* payload;
  uint8_t payloadLen;
};

class ZigbeeNwk {
 public:
  static const uint8_t kProtocolVersion = 2;
  static const uint8_t kBaseHeaderLen = 8;
  static const uint8_t kMaxFrame = 116;
  static const uint8_t kMaxPayload = kMaxFrame - kBaseHeaderLen;
  static const uint8_t kDefaultRadius = 30;
  static const uint16_t kBroadcastRxOnWhenIdle = 0xFFFD;
  static const uint16_t kBroadcastAllRouters = 0xFFFC;

  static uint8_t buildDataFrame(uint8_t* out, uint8_t outMax,
                                uint16_t dstShort, uint16_t srcShort,
                                uint8_t radius, uint8_t sequence,
                                const uint8_t* payload,
                                uint8_t payloadLen,
                                uint8_t discoverRoute = NWK_DISCOVER_SUPPRESS);

  static bool parseDataFrame(const uint8_t* npdu, uint8_t len,
                             NwkDataFrame& frame);

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

#endif  // NIUS_ZIGBEE_NWK_H
