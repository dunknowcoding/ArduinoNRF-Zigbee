/*
  ZigbeeHumidityMeasurementCluster.h - ZCL Relative Humidity Measurement (0x0405).

  A humidity sensor reports MeasuredValue as an unsigned 16-bit value in 0.01 %
  (so 48.30 % is 4830). MinMeasuredValue / MaxMeasuredValue give the range. Same
  shape as the temperature cluster but with unsigned values.
*/
#ifndef NIUS_ZIGBEE_HUMIDITY_MEASUREMENT_CLUSTER_H
#define NIUS_ZIGBEE_HUMIDITY_MEASUREMENT_CLUSTER_H

#include <Arduino.h>
#include "ZigbeeZcl.h"

namespace nzb {

class ZigbeeHumidityMeasurementCluster {
 public:
  static const uint16_t kClusterId = 0x0405;
  static const uint16_t kAttrMeasuredValue = 0x0000;     // uint16, 0.01 %
  static const uint16_t kAttrMinMeasuredValue = 0x0001;  // uint16
  static const uint16_t kAttrMaxMeasuredValue = 0x0002;  // uint16

  ZigbeeHumidityMeasurementCluster()
      : measured_(5000), min_(0), max_(10000) {}

  void setMeasuredRaw(uint16_t v) { measured_ = v; }
  void setMeasuredPercent(float pct) {
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    measured_ = (uint16_t)(pct * 100.0f + 0.5f);
  }
  uint16_t measuredRaw() const { return measured_; }
  void setRange(uint16_t lo, uint16_t hi) { min_ = lo; max_ = hi; }

  uint8_t appendReadAttributeRecord(uint16_t attrId, uint8_t* out,
                                    uint8_t outMax) const {
    switch (attrId) {
      case kAttrMeasuredValue:
        return ZigbeeZcl::buildUint16AttributeRecord(out, outMax, attrId, measured_);
      case kAttrMinMeasuredValue:
        return ZigbeeZcl::buildUint16AttributeRecord(out, outMax, attrId, min_);
      case kAttrMaxMeasuredValue:
        return ZigbeeZcl::buildUint16AttributeRecord(out, outMax, attrId, max_);
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

  uint8_t buildReport(uint8_t* out, uint8_t outMax) const {
    return ZigbeeZcl::buildReportUint16AttributePayload(out, outMax,
                                                        kAttrMeasuredValue, measured_);
  }

 private:
  uint16_t measured_;
  uint16_t min_;
  uint16_t max_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_HUMIDITY_MEASUREMENT_CLUSTER_H
