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
  uint8_t dstEndpoint;    // valid for unicast delivery
  uint16_t groupAddress;  // valid for group delivery (deliveryMode == GROUP)
  uint16_t clusterId;
  uint16_t profileId;
  uint8_t srcEndpoint;
  uint8_t counter;
  const uint8_t* payload;
  uint8_t payloadLen;
};

/** A parsed APS acknowledgement frame (frame type = 0b10). It echoes the
    addressing of the data frame it confirms and carries the same APS
    counter, which is what the sender matches against its pending table. */
struct ApsAckFrame {
  bool valid;
  uint8_t dstEndpoint;
  uint16_t clusterId;
  uint16_t profileId;
  uint8_t srcEndpoint;
  uint8_t counter;
};

class ZigbeeAps {
 public:
  static const uint8_t kBaseHeaderLen = 8;
  static const uint8_t kGroupHeaderLen = 9;  // group(2) replaces dstEndpoint(1)
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

  /** Build a GROUP-addressed APS data frame: delivery mode = group, with a
      16-bit group address in place of the destination endpoint (no dst endpoint
      - every member endpoint of the group receives it). Layout: FCF(1) +
      group(2) + cluster(2) + profile(2) + src endpoint(1) + counter(1) = 9. */
  static uint8_t buildGroupDataFrame(uint8_t* out, uint8_t outMax,
                                     uint16_t groupAddress, uint16_t clusterId,
                                     uint16_t profileId, uint8_t srcEndpoint,
                                     uint8_t counter, const uint8_t* payload,
                                     uint8_t payloadLen, bool ackRequest = false);

  /** APS frame type of an APDU (APS_FRAME_DATA / _COMMAND / _ACK), or 0xFF
      if too short to tell. Lets a receiver branch before full parsing. */
  static uint8_t frameType(const uint8_t* apdu, uint8_t len);

  /** Build an APS acknowledgement frame confirming a received data frame.
      The endpoints are passed already swapped for the return direction
      (ackDstEndpoint = received srcEndpoint, ackSrcEndpoint = received
      dstEndpoint); counter is the received data frame's APS counter. */
  static uint8_t buildAckFrame(uint8_t* out, uint8_t outMax,
                               uint8_t ackDstEndpoint, uint16_t clusterId,
                               uint16_t profileId, uint8_t ackSrcEndpoint,
                               uint8_t counter);

  static bool parseAckFrame(const uint8_t* apdu, uint8_t len,
                            ApsAckFrame& frame);

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
