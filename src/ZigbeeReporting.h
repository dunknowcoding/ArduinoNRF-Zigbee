/*
  ZigbeeReporting.h - tiny reporting helpers for NiusZigbee.

  This is not a full reporting engine. It tracks one boolean attribute and tells
  a sketch when to emit a ZCL Report Attributes command.
*/
#ifndef NIUS_ZIGBEE_REPORTING_H
#define NIUS_ZIGBEE_REPORTING_H

#include <Arduino.h>
#include "ZigbeeZcl.h"

namespace nzb {

class ZigbeeBoolReportScheduler {
 public:
  ZigbeeBoolReportScheduler();

  void configure(uint16_t attrId, uint16_t minIntervalSec,
                 uint16_t maxIntervalSec, bool currentValue,
                 uint32_t nowMs);
  void disable();

  bool isConfigured() const { return configured_; }
  uint16_t attributeId() const { return attrId_; }
  uint16_t minIntervalSec() const { return minIntervalSec_; }
  uint16_t maxIntervalSec() const { return maxIntervalSec_; }

  bool shouldReport(bool currentValue, uint32_t nowMs) const;
  void markReported(bool currentValue, uint32_t nowMs);

  uint8_t buildReportCommand(uint8_t zclSequence, bool currentValue,
                             uint8_t* out, uint8_t outMax) const;

 private:
  bool configured_;
  uint16_t attrId_;
  uint16_t minIntervalSec_;
  uint16_t maxIntervalSec_;
  bool lastValue_;
  uint32_t lastReportMs_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_REPORTING_H
