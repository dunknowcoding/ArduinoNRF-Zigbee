#include "ZigbeeClusters.h"

namespace nzb {

namespace {

uint8_t buildReadResponseFrame(const ZclFrame& request, const uint8_t* payload,
                               uint8_t payloadLen, uint8_t* out,
                               uint8_t outMax) {
  return ZigbeeZcl::buildCommandFrame(
      out, outMax, ZCL_FRAME_PROFILE_WIDE, request.sequence,
      ZCL_CMD_READ_ATTRIBUTES_RESPONSE, payload, payloadLen,
      ZCL_DIRECTION_SERVER_TO_CLIENT);
}

uint8_t buildDefaultResponseFrame(const ZclFrame& request, uint8_t status,
                                  uint8_t* out, uint8_t outMax) {
  uint8_t payload[2];
  uint8_t payloadLen = ZigbeeZcl::buildDefaultResponsePayload(
      payload, sizeof(payload), request.commandId, status);
  return ZigbeeZcl::buildCommandFrame(
      out, outMax, ZCL_FRAME_PROFILE_WIDE, request.sequence,
      ZCL_CMD_DEFAULT_RESPONSE, payload, payloadLen,
      ZCL_DIRECTION_SERVER_TO_CLIENT);
}

}  // namespace

ZigbeeOnOffCluster::ZigbeeOnOffCluster(bool initialState)
    : on_(initialState) {}

uint8_t ZigbeeOnOffCluster::appendReadAttributeRecord(uint16_t attrId,
                                                      uint8_t* out,
                                                      uint8_t outMax) const {
  if (attrId == ZigbeeZcl::kAttrOnOff) {
    return ZigbeeZcl::buildBoolAttributeRecord(out, outMax, attrId, on_);
  }
  return ZigbeeZcl::buildAttributeStatusRecord(
      out, outMax, attrId, ZCL_STATUS_UNSUPPORTED_ATTRIBUTE);
}

uint8_t ZigbeeOnOffCluster::buildReadAttributesResponse(
    const ZclFrame& request, uint8_t* out, uint8_t outMax) const {
  if (!out || !request.payload || request.payloadLen == 0) return 0;

  uint8_t payload[ZigbeeZcl::kMaxPayload];
  uint8_t payloadLen = 0;
  for (uint8_t i = 0;; ++i) {
    uint16_t attrId = 0;
    if (!ZigbeeZcl::getReadAttributeId(request.payload, request.payloadLen, i,
                                       attrId)) {
      break;
    }
    uint8_t added = appendReadAttributeRecord(
        attrId, &payload[payloadLen], (uint8_t)(sizeof(payload) - payloadLen));
    if (added == 0) return 0;
    payloadLen += added;
  }
  if (payloadLen == 0) return 0;
  return buildReadResponseFrame(request, payload, payloadLen, out, outMax);
}

uint8_t ZigbeeOnOffCluster::handleFrame(const ZclFrame& request, uint8_t* out,
                                        uint8_t outMax) {
  if (!out) return 0;

  if (request.frameType == ZCL_FRAME_CLUSTER_SPECIFIC &&
      request.direction == ZCL_DIRECTION_CLIENT_TO_SERVER) {
    uint8_t status = applyCommand(request.commandId)
                         ? ZCL_STATUS_SUCCESS
                         : ZCL_STATUS_UNSUPPORTED_CLUSTER_COMMAND;
    return buildDefaultResponseFrame(request, status, out, outMax);
  }

  if (request.frameType == ZCL_FRAME_PROFILE_WIDE &&
      request.commandId == ZCL_CMD_READ_ATTRIBUTES) {
    return buildReadAttributesResponse(request, out, outMax);
  }

  return 0;
}

ZigbeeBasicCluster::ZigbeeBasicCluster()
    : zclVersion_(3),
      applicationVersion_(1),
      stackVersion_(1),
      hardwareVersion_(1),
      powerSource_(0x03),
      manufacturerName_("NiusRobotLab"),
      modelIdentifier_("ArduinoNRF-Zigbee"),
      dateCode_("") {}

void ZigbeeBasicCluster::setVersions(uint8_t zclVersion,
                                     uint8_t applicationVersion,
                                     uint8_t stackVersion,
                                     uint8_t hardwareVersion) {
  zclVersion_ = zclVersion;
  applicationVersion_ = applicationVersion;
  stackVersion_ = stackVersion;
  hardwareVersion_ = hardwareVersion;
}

void ZigbeeBasicCluster::setIdentity(const char* manufacturerName,
                                     const char* modelIdentifier,
                                     const char* dateCode) {
  manufacturerName_ = manufacturerName ? manufacturerName : "";
  modelIdentifier_ = modelIdentifier ? modelIdentifier : "";
  dateCode_ = dateCode ? dateCode : "";
}

uint8_t ZigbeeBasicCluster::appendReadAttributeRecord(uint16_t attrId,
                                                      uint8_t* out,
                                                      uint8_t outMax) const {
  switch (attrId) {
    case ZigbeeZcl::kAttrBasicZclVersion:
      return ZigbeeZcl::buildUint8AttributeRecord(out, outMax, attrId,
                                                  zclVersion_);
    case ZigbeeZcl::kAttrBasicApplicationVersion:
      return ZigbeeZcl::buildUint8AttributeRecord(out, outMax, attrId,
                                                  applicationVersion_);
    case ZigbeeZcl::kAttrBasicStackVersion:
      return ZigbeeZcl::buildUint8AttributeRecord(out, outMax, attrId,
                                                  stackVersion_);
    case ZigbeeZcl::kAttrBasicHardwareVersion:
      return ZigbeeZcl::buildUint8AttributeRecord(out, outMax, attrId,
                                                  hardwareVersion_);
    case ZigbeeZcl::kAttrBasicManufacturerName:
      return ZigbeeZcl::buildCharStringAttributeRecord(
          out, outMax, attrId, manufacturerName_);
    case ZigbeeZcl::kAttrBasicModelIdentifier:
      return ZigbeeZcl::buildCharStringAttributeRecord(
          out, outMax, attrId, modelIdentifier_);
    case ZigbeeZcl::kAttrBasicDateCode:
      return ZigbeeZcl::buildCharStringAttributeRecord(out, outMax, attrId,
                                                       dateCode_);
    case ZigbeeZcl::kAttrBasicPowerSource:
      return ZigbeeZcl::buildUint8AttributeRecord(out, outMax, attrId,
                                                  powerSource_);
    default:
      return ZigbeeZcl::buildAttributeStatusRecord(
          out, outMax, attrId, ZCL_STATUS_UNSUPPORTED_ATTRIBUTE);
  }
}

uint8_t ZigbeeBasicCluster::buildReadAttributesResponse(
    const ZclFrame& request, uint8_t* out, uint8_t outMax) const {
  if (!out || !request.payload || request.payloadLen == 0) return 0;

  uint8_t payload[ZigbeeZcl::kMaxPayload];
  uint8_t payloadLen = 0;
  for (uint8_t i = 0;; ++i) {
    uint16_t attrId = 0;
    if (!ZigbeeZcl::getReadAttributeId(request.payload, request.payloadLen, i,
                                       attrId)) {
      break;
    }
    uint8_t added = appendReadAttributeRecord(
        attrId, &payload[payloadLen], (uint8_t)(sizeof(payload) - payloadLen));
    if (added == 0) return 0;
    payloadLen += added;
  }
  if (payloadLen == 0) return 0;
  return buildReadResponseFrame(request, payload, payloadLen, out, outMax);
}

uint8_t ZigbeeBasicCluster::handleFrame(const ZclFrame& request, uint8_t* out,
                                        uint8_t outMax) const {
  if (!out) return 0;
  if (request.frameType == ZCL_FRAME_PROFILE_WIDE &&
      request.commandId == ZCL_CMD_READ_ATTRIBUTES) {
    return buildReadAttributesResponse(request, out, outMax);
  }
  return 0;
}

}  // namespace nzb
