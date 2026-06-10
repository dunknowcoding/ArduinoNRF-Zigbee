/*
  ZigbeeAps.h - small Zigbee APS frame helpers used by NiusZigbee.

  This is frame tooling, not APSDE/AIB/binding/group-table behavior. It builds
  and parses simple unicast APS data frames that fit inside Zigbee NWK payloads.
*/
#ifndef NIUS_ZIGBEE_APS_H
#define NIUS_ZIGBEE_APS_H

#include <Arduino.h>

namespace nzb {

enum ApsFrameType : uint8_t {
  APS_FRAME_DATA = 0,
  APS_FRAME_COMMAND = 1,
  APS_FRAME_ACK = 2
};

enum ApsDeliveryMode : uint8_t {
  APS_DELIVERY_UNICAST = 0,
  APS_DELIVERY_INDIRECT = 1,
  APS_DELIVERY_BROADCAST = 2,
  APS_DELIVERY_GROUP = 3
};

struct ApsDataFrame {
  bool valid;
  uint8_t frameType;
  uint8_t deliveryMode;
  bool ackRequest;
  bool security;
  bool extendedHeader;
  uint8_t dstEndpoint;
  uint16_t clusterId;
  uint16_t profileId;
  uint8_t srcEndpoint;
  uint8_t counter;
  const uint8_t* payload;
  uint8_t payloadLen;
};

class ZigbeeAps {
 public:
  static const uint8_t kBaseHeaderLen = 8;
  static const uint8_t kMaxFrame = 108;
  static const uint8_t kMaxPayload = kMaxFrame - kBaseHeaderLen;
  static const uint16_t kProfileHomeAutomation = 0x0104;
  static const uint16_t kProfileZigbeeDevice = 0x0000;

  static uint8_t buildDataFrame(uint8_t* out, uint8_t outMax,
                                uint8_t dstEndpoint, uint16_t clusterId,
                                uint16_t profileId, uint8_t srcEndpoint,
                                uint8_t counter, const uint8_t* payload,
                                uint8_t payloadLen,
                                bool ackRequest = false);

  static bool parseDataFrame(const uint8_t* apdu, uint8_t len,
                             ApsDataFrame& frame);

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

#endif  // NIUS_ZIGBEE_APS_H
