/*
  ZigbeeElectricalMeasurementCluster.h - ZCL Electrical Measurement (0x0B04).

  A smart plug / energy monitor reports the AC line measurements: RMS voltage
  (V), RMS current (mA), active power (W) and line frequency (Hz). This header
  holds those attributes, builds Read Attributes Response records, and builds a
  Report Attributes payload for the active power - the metering counterpart to
  the temperature / humidity sensors.

  Values are raw register units (the real reading = raw * multiplier / divisor,
  which a product advertises via the *Multiplier / *Divisor attributes; this
  helper carries the raw values and the common defaults).
*/
#ifndef NIUS_ZIGBEE_ELECTRICAL_MEASUREMENT_CLUSTER_H
#define NIUS_ZIGBEE_ELECTRICAL_MEASUREMENT_CLUSTER_H

#include <Arduino.h>
#include "ZigbeeZcl.h"

namespace nzb {

class ZigbeeElectricalMeasurementCluster {
 public:
  static const uint16_t kClusterId = 0x0B04;
  static const uint16_t kAttrMeasurementType = 0x0000;  // map32 (we expose low 16b)
  static const uint16_t kAttrACFrequency = 0x0300;      // uint16, Hz
  static const uint16_t kAttrRmsVoltage = 0x0505;       // uint16, V
  static const uint16_t kAttrRmsCurrent = 0x0508;       // uint16, mA
  static const uint16_t kAttrActivePower = 0x050B;      // int16, W

  ZigbeeElectricalMeasurementCluster()
      : rmsVoltage_(230), rmsCurrent_(0), activePower_(0), acFrequency_(50) {}

  void setRmsVoltage(uint16_t v) { rmsVoltage_ = v; }
  void setRmsCurrent(uint16_t mA) { rmsCurrent_ = mA; }
  void setActivePower(int16_t w) { activePower_ = w; }
  void setAcFrequency(uint16_t hz) { acFrequency_ = hz; }
  int16_t activePower() const { return activePower_; }

  uint8_t appendReadAttributeRecord(uint16_t attrId, uint8_t* out,
                                    uint8_t outMax) const {
    switch (attrId) {
      case kAttrACFrequency:
        return ZigbeeZcl::buildUint16AttributeRecord(out, outMax, attrId, acFrequency_);
      case kAttrRmsVoltage:
        return ZigbeeZcl::buildUint16AttributeRecord(out, outMax, attrId, rmsVoltage_);
      case kAttrRmsCurrent:
        return ZigbeeZcl::buildUint16AttributeRecord(out, outMax, attrId, rmsCurrent_);
      case kAttrActivePower:
        return ZigbeeZcl::buildInt16AttributeRecord(out, outMax, attrId, activePower_);
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

  /** Report Attributes payload for ActivePower (int16). */
  uint8_t buildReport(uint8_t* out, uint8_t outMax) const {
    return ZigbeeZcl::buildReportInt16AttributePayload(out, outMax,
                                                       kAttrActivePower, activePower_);
  }

 private:
  uint16_t rmsVoltage_;
  uint16_t rmsCurrent_;
  int16_t activePower_;
  uint16_t acFrequency_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_ELECTRICAL_MEASUREMENT_CLUSTER_H
