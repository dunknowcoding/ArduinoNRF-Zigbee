/*
  ZigbeeTemperatureMeasurementCluster.h - ZCL Temperature Measurement (0x0402).

  A temperature sensor reports MeasuredValue as a signed 16-bit value in 0.01 C
  (so 21.50 C is 2150, -5.00 C is -500). MinMeasuredValue / MaxMeasuredValue give
  the sensor's range. This header holds those attributes, builds Read Attributes
  Response records for them, and builds a Report Attributes payload for the
  measured value (for periodic / on-change reporting). The measurement-cluster
  counterpart to the On/Off and IAS Zone behaviors.
*/
#ifndef NIUS_ZIGBEE_TEMPERATURE_MEASUREMENT_CLUSTER_H
#define NIUS_ZIGBEE_TEMPERATURE_MEASUREMENT_CLUSTER_H

#include <Arduino.h>
#include "ZigbeeZcl.h"

namespace nzb {

class ZigbeeTemperatureMeasurementCluster {
 public:
  static const uint16_t kClusterId = 0x0402;
  static const uint16_t kAttrMeasuredValue = 0x0000;     // int16, 0.01 C
  static const uint16_t kAttrMinMeasuredValue = 0x0001;  // int16
  static const uint16_t kAttrMaxMeasuredValue = 0x0002;  // int16
  static const uint16_t kAttrTolerance = 0x0003;         // uint16, 0.01 C

  ZigbeeTemperatureMeasurementCluster()
      : measured_(2000), min_(-4000), max_(12500), tolerance_(50) {}

  void setMeasuredRaw(int16_t v) { measured_ = v; }
  void setMeasuredCelsius(float c) {
    float r = c * 100.0f;
    measured_ = (int16_t)(r + (r >= 0.0f ? 0.5f : -0.5f));
  }
  int16_t measuredRaw() const { return measured_; }
  void setRange(int16_t lo, int16_t hi) { min_ = lo; max_ = hi; }

  /** Append one Read Attributes Response record for @p attrId. */
  uint8_t appendReadAttributeRecord(uint16_t attrId, uint8_t* out,
                                    uint8_t outMax) const {
    switch (attrId) {
      case kAttrMeasuredValue:
        return ZigbeeZcl::buildInt16AttributeRecord(out, outMax, attrId, measured_);
      case kAttrMinMeasuredValue:
        return ZigbeeZcl::buildInt16AttributeRecord(out, outMax, attrId, min_);
      case kAttrMaxMeasuredValue:
        return ZigbeeZcl::buildInt16AttributeRecord(out, outMax, attrId, max_);
      case kAttrTolerance:
        return ZigbeeZcl::buildUint16AttributeRecord(out, outMax, attrId, tolerance_);
      default:
        return ZigbeeZcl::buildAttributeStatusRecord(
            out, outMax, attrId, ZCL_STATUS_UNSUPPORTED_ATTRIBUTE);
    }
  }

  /** Concatenate Read Attributes Response records for every attribute in the
      request payload. Returns the response-payload length (0 on overflow). */
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

  /** Report Attributes payload for MeasuredValue. */
  uint8_t buildReport(uint8_t* out, uint8_t outMax) const {
    return ZigbeeZcl::buildReportInt16AttributePayload(out, outMax,
                                                       kAttrMeasuredValue, measured_);
  }

 private:
  int16_t measured_;
  int16_t min_;
  int16_t max_;
  uint16_t tolerance_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_TEMPERATURE_MEASUREMENT_CLUSTER_H
