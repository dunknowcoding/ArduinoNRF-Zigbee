#include "ZigbeeZcl.h"

#include <string.h>

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

uint8_t ZigbeeZcl::buildReadAttributesPayload(uint8_t* out, uint8_t outMax,
                                              const uint16_t* attrIds,
                                              uint8_t attrCount) {
  if (!out || !attrIds || attrCount == 0) return 0;
  if (outMax < attrCount * 2) return 0;
  for (uint8_t i = 0; i < attrCount; ++i) {
    writeLe16(&out[i * 2], attrIds[i]);
  }
  return (uint8_t)(attrCount * 2);
}

bool ZigbeeZcl::getReadAttributeId(const uint8_t* payload, uint8_t payloadLen,
                                   uint8_t index, uint16_t& attrId) {
  uint8_t offset = (uint8_t)(index * 2);
  if (!payload || offset > payloadLen || payloadLen - offset < 2) return false;
  attrId = readLe16(&payload[offset]);
  return true;
}

uint8_t ZigbeeZcl::buildDefaultResponsePayload(uint8_t* out, uint8_t outMax,
                                               uint8_t commandId,
                                               uint8_t status) {
  if (!out || outMax < 2) return 0;
  out[0] = commandId;
  out[1] = status;
  return 2;
}

bool ZigbeeZcl::parseDefaultResponsePayload(const uint8_t* payload,
                                            uint8_t payloadLen,
                                            uint8_t& commandId,
                                            uint8_t& status) {
  if (!payload || payloadLen < 2) return false;
  commandId = payload[0];
  status = payload[1];
  return true;
}

uint8_t ZigbeeZcl::buildAttributeStatusRecord(uint8_t* out, uint8_t outMax,
                                              uint16_t attrId,
                                              uint8_t status) {
  if (!out || outMax < 3) return 0;
  writeLe16(&out[0], attrId);
  out[2] = status;
  return 3;
}

uint8_t ZigbeeZcl::buildBoolAttributeRecord(uint8_t* out, uint8_t outMax,
                                            uint16_t attrId, bool value) {
  if (!out || outMax < 5) return 0;
  writeLe16(&out[0], attrId);
  out[2] = ZCL_STATUS_SUCCESS;
  out[3] = ZCL_TYPE_BOOLEAN;
  out[4] = value ? 1 : 0;
  return 5;
}

uint8_t ZigbeeZcl::buildUint8AttributeRecord(uint8_t* out, uint8_t outMax,
                                             uint16_t attrId, uint8_t value) {
  if (!out || outMax < 5) return 0;
  writeLe16(&out[0], attrId);
  out[2] = ZCL_STATUS_SUCCESS;
  out[3] = ZCL_TYPE_UINT8;
  out[4] = value;
  return 5;
}

uint8_t ZigbeeZcl::buildUint16AttributeRecord(uint8_t* out, uint8_t outMax,
                                              uint16_t attrId, uint16_t value) {
  if (!out || outMax < 6) return 0;
  writeLe16(&out[0], attrId);
  out[2] = ZCL_STATUS_SUCCESS;
  out[3] = ZCL_TYPE_UINT16;
  writeLe16(&out[4], value);
  return 6;
}

uint8_t ZigbeeZcl::buildInt16AttributeRecord(uint8_t* out, uint8_t outMax,
                                             uint16_t attrId, int16_t value) {
  if (!out || outMax < 6) return 0;
  writeLe16(&out[0], attrId);
  out[2] = ZCL_STATUS_SUCCESS;
  out[3] = ZCL_TYPE_INT16;
  writeLe16(&out[4], (uint16_t)value);
  return 6;
}

uint8_t ZigbeeZcl::buildTyped8AttributeRecord(uint8_t* out, uint8_t outMax,
                                              uint16_t attrId, uint8_t type,
                                              uint8_t value) {
  if (!out || outMax < 5) return 0;
  writeLe16(&out[0], attrId);
  out[2] = ZCL_STATUS_SUCCESS;
  out[3] = type;
  out[4] = value;
  return 5;
}

uint8_t ZigbeeZcl::buildReportUint16AttributePayload(uint8_t* out,
                                                     uint8_t outMax,
                                                     uint16_t attrId,
                                                     uint16_t value) {
  if (!out || outMax < 5) return 0;
  writeLe16(&out[0], attrId);
  out[2] = ZCL_TYPE_UINT16;
  writeLe16(&out[3], value);
  return 5;
}

uint8_t ZigbeeZcl::buildReportInt16AttributePayload(uint8_t* out, uint8_t outMax,
                                                    uint16_t attrId,
                                                    int16_t value) {
  if (!out || outMax < 5) return 0;
  writeLe16(&out[0], attrId);
  out[2] = ZCL_TYPE_INT16;
  writeLe16(&out[3], (uint16_t)value);
  return 5;
}

uint8_t ZigbeeZcl::buildCharStringAttributeRecord(uint8_t* out, uint8_t outMax,
                                                  uint16_t attrId,
                                                  const char* value) {
  if (!out || !value) return 0;
  size_t len = strlen(value);
  if (len > 255) len = 255;
  if (outMax < 5 + len) return 0;
  writeLe16(&out[0], attrId);
  out[2] = ZCL_STATUS_SUCCESS;
  out[3] = ZCL_TYPE_CHAR_STRING;
  out[4] = (uint8_t)len;
  for (uint8_t i = 0; i < (uint8_t)len; ++i) {
    out[5 + i] = (uint8_t)value[i];
  }
  return (uint8_t)(5 + len);
}

uint8_t ZigbeeZcl::buildReportBoolAttributePayload(uint8_t* out,
                                                   uint8_t outMax,
                                                   uint16_t attrId,
                                                   bool value) {
  if (!out || outMax < 4) return 0;
  writeLe16(&out[0], attrId);
  out[2] = ZCL_TYPE_BOOLEAN;
  out[3] = value ? 1 : 0;
  return 4;
}

bool ZigbeeZcl::parseReportBoolAttributePayload(const uint8_t* payload,
                                                uint8_t payloadLen,
                                                uint16_t& attrId,
                                                bool& value) {
  if (!payload || payloadLen < 4) return false;
  attrId = readLe16(&payload[0]);
  if (payload[2] != ZCL_TYPE_BOOLEAN) return false;
  value = payload[3] != 0;
  return true;
}

uint8_t ZigbeeZcl::buildConfigureReportingBoolPayload(uint8_t* out,
                                                      uint8_t outMax,
                                                      uint16_t attrId,
                                                      uint16_t minIntervalSec,
                                                      uint16_t maxIntervalSec) {
  if (!out || outMax < 8) return 0;
  out[0] = ZCL_REPORT_DIRECTION_REPORTED;
  writeLe16(&out[1], attrId);
  out[3] = ZCL_TYPE_BOOLEAN;
  writeLe16(&out[4], minIntervalSec);
  writeLe16(&out[6], maxIntervalSec);
  return 8;
}

bool ZigbeeZcl::parseConfigureReportingBoolPayload(const uint8_t* payload,
                                                   uint8_t payloadLen,
                                                   uint16_t& attrId,
                                                   uint16_t& minIntervalSec,
                                                   uint16_t& maxIntervalSec) {
  if (!payload || payloadLen < 8) return false;
  if (payload[0] != ZCL_REPORT_DIRECTION_REPORTED) return false;
  if (payload[3] != ZCL_TYPE_BOOLEAN) return false;
  attrId = readLe16(&payload[1]);
  minIntervalSec = readLe16(&payload[4]);
  maxIntervalSec = readLe16(&payload[6]);
  return true;
}

uint8_t ZigbeeZcl::buildConfigureReportingResponsePayload(uint8_t* out,
                                                          uint8_t outMax,
                                                          uint8_t status,
                                                          uint8_t direction,
                                                          uint16_t attrId) {
  if (status == ZCL_STATUS_SUCCESS) return 0;
  if (!out || outMax < 4) return 0;
  out[0] = status;
  out[1] = direction;
  writeLe16(&out[2], attrId);
  return 4;
}

bool ZigbeeZcl::parseConfigureReportingStatusRecord(const uint8_t* payload,
                                                    uint8_t payloadLen,
                                                    uint8_t& status,
                                                    uint8_t& direction,
                                                    uint16_t& attrId) {
  if (!payload || payloadLen < 4) return false;
  status = payload[0];
  direction = payload[1];
  attrId = readLe16(&payload[2]);
  return true;
}

bool ZigbeeZcl::applyOnOffCommand(uint8_t commandId, bool& on) {
  if (commandId == ZCL_ON_OFF_CMD_OFF) {
    on = false;
    return true;
  }
  if (commandId == ZCL_ON_OFF_CMD_ON) {
    on = true;
    return true;
  }
  if (commandId == ZCL_ON_OFF_CMD_TOGGLE) {
    on = !on;
    return true;
  }
  return false;
}

}  // namespace nzb
