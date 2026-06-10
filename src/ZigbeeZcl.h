/*
  ZigbeeZcl.h - small Zigbee Cluster Library frame helpers used by NiusZigbee.

  This builds and parses basic ZCL command frames. It does not implement device
  discovery, attribute storage, default responses, reporting, or cluster logic.
*/
#ifndef NIUS_ZIGBEE_ZCL_H
#define NIUS_ZIGBEE_ZCL_H

#include <Arduino.h>

namespace nzb {

enum ZclFrameType : uint8_t {
  ZCL_FRAME_PROFILE_WIDE = 0,
  ZCL_FRAME_CLUSTER_SPECIFIC = 1
};

enum ZclDirection : uint8_t {
  ZCL_DIRECTION_CLIENT_TO_SERVER = 0,
  ZCL_DIRECTION_SERVER_TO_CLIENT = 1
};

enum ZclCommandId : uint8_t {
  ZCL_CMD_READ_ATTRIBUTES = 0x00,
  ZCL_CMD_READ_ATTRIBUTES_RESPONSE = 0x01,
  ZCL_CMD_WRITE_ATTRIBUTES = 0x02,
  ZCL_CMD_WRITE_ATTRIBUTES_RESPONSE = 0x04,
  ZCL_CMD_DEFAULT_RESPONSE = 0x0B
};

enum ZclOnOffCommandId : uint8_t {
  ZCL_ON_OFF_CMD_OFF = 0x00,
  ZCL_ON_OFF_CMD_ON = 0x01,
  ZCL_ON_OFF_CMD_TOGGLE = 0x02
};

struct ZclFrame {
  bool valid;
  uint8_t frameType;
  bool manufacturerSpecific;
  uint16_t manufacturerCode;
  uint8_t direction;
  bool disableDefaultResponse;
  uint8_t sequence;
  uint8_t commandId;
  const uint8_t* payload;
  uint8_t payloadLen;
};

class ZigbeeZcl {
 public:
  static const uint8_t kBaseHeaderLen = 3;
  static const uint8_t kManufacturerHeaderLen = 5;
  static const uint8_t kMaxFrame = 100;
  static const uint8_t kMaxPayload = kMaxFrame - kBaseHeaderLen;
  static const uint16_t kClusterBasic = 0x0000;
  static const uint16_t kClusterOnOff = 0x0006;
  static const uint16_t kClusterLevelControl = 0x0008;

  static uint8_t buildCommandFrame(uint8_t* out, uint8_t outMax,
                                   uint8_t frameType, uint8_t sequence,
                                   uint8_t commandId,
                                   const uint8_t* payload,
                                   uint8_t payloadLen,
                                   uint8_t direction = ZCL_DIRECTION_CLIENT_TO_SERVER,
                                   bool disableDefaultResponse = true);

  static bool parseFrame(const uint8_t* zcl, uint8_t len, ZclFrame& frame);

 private:
  static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_ZCL_H
