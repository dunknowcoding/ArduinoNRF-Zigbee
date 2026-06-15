/*
  ZigbeeOccupancyCluster.h - ZCL Occupancy Sensing cluster (0x0406).

  An occupancy sensor (PIR / ultrasonic) reports the Occupancy attribute, an
  8-bit bitmap whose bit 0 is "occupied". OccupancySensorType names the sensing
  technology. This header holds the state, builds Read Attributes Response
  records, and builds a Report Attributes payload for the occupancy bitmap.
*/
#ifndef NIUS_ZIGBEE_OCCUPANCY_CLUSTER_H
#define NIUS_ZIGBEE_OCCUPANCY_CLUSTER_H

#include <Arduino.h>
#include "ZigbeeZcl.h"

namespace nzb {

enum ZclOccupancySensorType : uint8_t {
  OCCUPANCY_SENSOR_PIR = 0x00,
  OCCUPANCY_SENSOR_ULTRASONIC = 0x01,
  OCCUPANCY_SENSOR_PIR_AND_ULTRASONIC = 0x02,
};

class ZigbeeOccupancyCluster {
 public:
  static const uint16_t kClusterId = 0x0406;
  static const uint16_t kAttrOccupancy = 0x0000;            // map8, bit0 = occupied
  static const uint16_t kAttrOccupancySensorType = 0x0001;  // enum8
  static const uint16_t kAttrSensorTypeBitmap = 0x0002;     // map8
  static const uint8_t kOccupiedBit = 0x01;

  explicit ZigbeeOccupancyCluster(uint8_t sensorType = OCCUPANCY_SENSOR_PIR)
      : occupied_(false), sensorType_(sensorType) {}

  void setOccupied(bool occupied) { occupied_ = occupied; }
  bool isOccupied() const { return occupied_; }
  uint8_t occupancyBitmap() const { return occupied_ ? kOccupiedBit : 0; }

  uint8_t appendReadAttributeRecord(uint16_t attrId, uint8_t* out,
                                    uint8_t outMax) const {
    switch (attrId) {
      case kAttrOccupancy:
        return ZigbeeZcl::buildTyped8AttributeRecord(out, outMax, attrId,
                                                     ZCL_TYPE_MAP8, occupancyBitmap());
      case kAttrOccupancySensorType:
        return ZigbeeZcl::buildTyped8AttributeRecord(out, outMax, attrId,
                                                     ZCL_TYPE_ENUM8, sensorType_);
      case kAttrSensorTypeBitmap:
        return ZigbeeZcl::buildTyped8AttributeRecord(
            out, outMax, attrId, ZCL_TYPE_MAP8, (uint8_t)(1u << sensorType_));
      default:
        return ZigbeeZcl::buildAttributeStatusRecord(
            out, outMax, attrId, ZCL_STATUS_UNSUPPORTED_ATTRIBUTE);
    }
  }

  uint8_t buildReadAttributesResponsePayload(const uint8_t* reqPayload,
                                             uint8_t reqLen, uint8_t* out,
                                             uint8_t outMax) const {
    if (!out || !reqPayload) return 0;
    uint8_t len = 0;
    for (uint8_t i = 0;; ++i) {
      uint16_t attrId = 0;
      if (!ZigbeeZcl::getReadAttributeId(reqPayload, reqLen, i, attrId)) break;
      uint8_t added = appendReadAttributeRecord(attrId, &out[len],
                                                (uint8_t)(outMax - len));
      if (added == 0) return 0;
      len += added;
    }
    return len;
  }

  /** Report Attributes payload for the Occupancy bitmap (map8). */
  uint8_t buildReport(uint8_t* out, uint8_t outMax) const {
    if (!out || outMax < 4) return 0;
    out[0] = (uint8_t)(kAttrOccupancy & 0xFF);
    out[1] = (uint8_t)(kAttrOccupancy >> 8);
    out[2] = ZCL_TYPE_MAP8;
    out[3] = occupancyBitmap();
    return 4;
  }

 private:
  bool occupied_;
  uint8_t sensorType_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_OCCUPANCY_CLUSTER_H
