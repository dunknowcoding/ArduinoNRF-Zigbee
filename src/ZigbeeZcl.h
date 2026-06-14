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
  ZCL_CMD_CONFIGURE_REPORTING = 0x06,
  ZCL_CMD_CONFIGURE_REPORTING_RESPONSE = 0x07,
  ZCL_CMD_REPORT_ATTRIBUTES = 0x0A,
  ZCL_CMD_DEFAULT_RESPONSE = 0x0B
};

enum ZclOnOffCommandId : uint8_t {
  ZCL_ON_OFF_CMD_OFF = 0x00,
  ZCL_ON_OFF_CMD_ON = 0x01,
  ZCL_ON_OFF_CMD_TOGGLE = 0x02
};

enum ZclStatus : uint8_t {
  ZCL_STATUS_SUCCESS = 0x00,
  ZCL_STATUS_UNSUPPORTED_CLUSTER_COMMAND = 0x81,
  ZCL_STATUS_UNSUPPORTED_GENERAL_COMMAND = 0x82,
  ZCL_STATUS_INVALID_FIELD = 0x85,
  ZCL_STATUS_UNSUPPORTED_ATTRIBUTE = 0x86,
  ZCL_STATUS_UNREPORTABLE_ATTRIBUTE = 0x8C
};

enum ZclReportDirection : uint8_t {
  ZCL_REPORT_DIRECTION_REPORTED = 0x00,
  ZCL_REPORT_DIRECTION_RECEIVED = 0x01
};

enum ZclDataType : uint8_t {
  ZCL_TYPE_BOOLEAN = 0x10,
  ZCL_TYPE_UINT8 = 0x20,
  ZCL_TYPE_UINT16 = 0x21,
  ZCL_TYPE_CHAR_STRING = 0x42
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
  static const uint16_t kClusterIdentify = 0x0003;
  static const uint16_t kClusterGroups = 0x0004;
  static const uint16_t kClusterOnOff = 0x0006;
  static const uint16_t kClusterLevelControl = 0x0008;
  static const uint16_t kAttrOnOff = 0x0000;
  static const uint16_t kAttrBasicZclVersion = 0x0000;
  static const uint16_t kAttrBasicApplicationVersion = 0x0001;
  static const uint16_t kAttrBasicStackVersion = 0x0002;
  static const uint16_t kAttrBasicHardwareVersion = 0x0003;
  static const uint16_t kAttrBasicManufacturerName = 0x0004;
  static const uint16_t kAttrBasicModelIdentifier = 0x0005;
  static const uint16_t kAttrBasicDateCode = 0x0006;
  static const uint16_t kAttrBasicPowerSource = 0x0007;

  static uint8_t buildCommandFrame(uint8_t* out, uint8_t outMax,
                                   uint8_t frameType, uint8_t sequence,
                                   uint8_t commandId,
                                   const uint8_t* payload,
                                   uint8_t payloadLen,
                                   uint8_t direction = ZCL_DIRECTION_CLIENT_TO_SERVER,
                                   bool disableDefaultResponse = true);

  static bool parseFrame(const uint8_t* zcl, uint8_t len, ZclFrame& frame);

  static uint8_t buildReadAttributesPayload(uint8_t* out, uint8_t outMax,
                                            const uint16_t* attrIds,
                                            uint8_t attrCount);

  static bool getReadAttributeId(const uint8_t* payload, uint8_t payloadLen,
                                 uint8_t index, uint16_t& attrId);

  static uint8_t buildDefaultResponsePayload(uint8_t* out, uint8_t outMax,
                                             uint8_t commandId,
                                             uint8_t status);

  static bool parseDefaultResponsePayload(const uint8_t* payload,
                                          uint8_t payloadLen,
                                          uint8_t& commandId,
                                          uint8_t& status);

  static uint8_t buildAttributeStatusRecord(uint8_t* out, uint8_t outMax,
                                            uint16_t attrId, uint8_t status);

  static uint8_t buildBoolAttributeRecord(uint8_t* out, uint8_t outMax,
                                          uint16_t attrId, bool value);

  static uint8_t buildUint8AttributeRecord(uint8_t* out, uint8_t outMax,
                                           uint16_t attrId, uint8_t value);

  static uint8_t buildCharStringAttributeRecord(uint8_t* out, uint8_t outMax,
                                                uint16_t attrId,
                                                const char* value);

  static uint8_t buildReportBoolAttributePayload(uint8_t* out, uint8_t outMax,
                                                 uint16_t attrId, bool value);

  static bool parseReportBoolAttributePayload(const uint8_t* payload,
                                              uint8_t payloadLen,
                                              uint16_t& attrId, bool& value);

  static uint8_t buildConfigureReportingBoolPayload(uint8_t* out,
                                                    uint8_t outMax,
                                                    uint16_t attrId,
                                                    uint16_t minIntervalSec,
                                                    uint16_t maxIntervalSec);

  static bool parseConfigureReportingBoolPayload(const uint8_t* payload,
                                                 uint8_t payloadLen,
                                                 uint16_t& attrId,
                                                 uint16_t& minIntervalSec,
                                                 uint16_t& maxIntervalSec);

  static uint8_t buildConfigureReportingResponsePayload(uint8_t* out,
                                                        uint8_t outMax,
                                                        uint8_t status,
                                                        uint8_t direction,
                                                        uint16_t attrId);

  static bool parseConfigureReportingStatusRecord(const uint8_t* payload,
                                                  uint8_t payloadLen,
                                                  uint8_t& status,
                                                  uint8_t& direction,
                                                  uint16_t& attrId);

  static bool applyOnOffCommand(uint8_t commandId, bool& on);

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

#endif  // NIUS_ZIGBEE_ZCL_H
