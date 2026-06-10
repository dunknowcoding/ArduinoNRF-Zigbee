#include "ZigbeeReporting.h"

namespace nzb {

namespace {

uint32_t secondsToMs(uint16_t seconds) {
  return (uint32_t)seconds * 1000UL;
}

}  // namespace

ZigbeeBoolReportScheduler::ZigbeeBoolReportScheduler()
    : configured_(false),
      attrId_(0),
      minIntervalSec_(0),
      maxIntervalSec_(0),
      lastValue_(false),
      lastReportMs_(0) {}

void ZigbeeBoolReportScheduler::configure(uint16_t attrId,
                                          uint16_t minIntervalSec,
                                          uint16_t maxIntervalSec,
                                          bool currentValue,
                                          uint32_t nowMs) {
  configured_ = true;
  attrId_ = attrId;
  minIntervalSec_ = minIntervalSec;
  maxIntervalSec_ = maxIntervalSec;
  lastValue_ = currentValue;
  lastReportMs_ = nowMs;
}

void ZigbeeBoolReportScheduler::disable() {
  configured_ = false;
}

bool ZigbeeBoolReportScheduler::shouldReport(bool currentValue,
                                             uint32_t nowMs) const {
  if (!configured_) return false;
  if (maxIntervalSec_ == 0xFFFF) return false;

  uint32_t elapsed = nowMs - lastReportMs_;
  if (currentValue != lastValue_) {
    return elapsed >= secondsToMs(minIntervalSec_);
  }

  return maxIntervalSec_ > 0 && elapsed >= secondsToMs(maxIntervalSec_);
}

void ZigbeeBoolReportScheduler::markReported(bool currentValue,
                                             uint32_t nowMs) {
  lastValue_ = currentValue;
  lastReportMs_ = nowMs;
}

uint8_t ZigbeeBoolReportScheduler::buildReportCommand(uint8_t zclSequence,
                                                      bool currentValue,
                                                      uint8_t* out,
                                                      uint8_t outMax) const {
  if (!configured_) return 0;
  uint8_t payload[4];
  uint8_t payloadLen = ZigbeeZcl::buildReportBoolAttributePayload(
      payload, sizeof(payload), attrId_, currentValue);
  if (payloadLen == 0) return 0;
  return ZigbeeZcl::buildCommandFrame(
      out, outMax, ZCL_FRAME_PROFILE_WIDE, zclSequence, ZCL_CMD_REPORT_ATTRIBUTES,
      payload, payloadLen, ZCL_DIRECTION_SERVER_TO_CLIENT);
}

}  // namespace nzb
