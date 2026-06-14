/*
  ZigbeeEndDeviceTimeout.h - NWK End Device Timeout request/response (sleepy
  end-device keep-alive).

  A (sleepy) end device tells its parent how long it may go silent before the
  parent is allowed to forget it: it sends an End Device Timeout Request
  (NWK command 0x0B) carrying a timeout from a fixed enumerated set, and the
  parent answers with an End Device Timeout Response (0x0C) granting it and
  advertising which keep-alive methods the parent supports (MAC data poll
  and/or end-device timeout). The child then keeps the relationship alive by
  polling (a MAC Data Request - see ZigbeeIndirectQueue) within that window; if
  it does not, the parent ages it out.

  This builds/parses those two command payloads and maps the timeout
  enumeration to seconds. It pairs with ZigbeeIndirectQueue (the parent's
  buffered-frame store) to complete the sleepy-end-device story.
*/
#ifndef NIUS_ZIGBEE_END_DEVICE_TIMEOUT_H
#define NIUS_ZIGBEE_END_DEVICE_TIMEOUT_H

#include <Arduino.h>

namespace nzb {

// End Device Timeout Response status (Zigbee 3.6.10.6).
enum NwkEdTimeoutStatus : uint8_t {
  ED_TIMEOUT_SUCCESS = 0x00,
  ED_TIMEOUT_INCORRECT_VALUE = 0x01,
};

// Parent-information bitmask in the response (which keep-alive the parent keeps).
enum NwkEdParentInfo : uint8_t {
  ED_PARENT_MAC_DATA_POLL = 0x01,  // parent keeps the child alive on MAC polls
  ED_PARENT_ED_TIMEOUT = 0x02,     // parent honours the requested timeout
  ED_PARENT_POWER_NEGOTIATION = 0x04,
};

struct NwkEdTimeoutRequest {
  uint8_t timeoutIndex;     // enumerated timeout (0..14)
  uint8_t configuration;    // reserved/extension bits (0 in r21)
};

struct NwkEdTimeoutResponse {
  uint8_t status;           // NwkEdTimeoutStatus
  uint8_t parentInfo;       // NwkEdParentInfo bitmask
};

class ZigbeeEndDeviceTimeout {
 public:
  static const uint8_t kMaxTimeoutIndex = 14;

  /** Requested-timeout enumeration -> seconds (Zigbee Table 3-x): index 0 is
      10 s, then 2,4,8,... minutes doubling up to index 14. Returns 0 for an
      out-of-range index. */
  static uint32_t timeoutSeconds(uint8_t index) {
    if (index == 0) return 10;
    if (index > kMaxTimeoutIndex) return 0;
    // index 1 = 2 min, 2 = 4 min, ... n = 2^n min.
    return (uint32_t)60u * ((uint32_t)1u << index);
  }

  /** Build an End Device Timeout Request payload: timeout index (1) +
      configuration (1). @return 2, or 0 on error. */
  static uint8_t buildRequest(uint8_t* out, uint8_t outMax,
                              const NwkEdTimeoutRequest& req) {
    if (!out || outMax < 2 || req.timeoutIndex > kMaxTimeoutIndex) return 0;
    out[0] = req.timeoutIndex;
    out[1] = req.configuration;
    return 2;
  }

  static bool parseRequest(const uint8_t* payload, uint8_t len,
                           NwkEdTimeoutRequest& req) {
    req = NwkEdTimeoutRequest();
    if (!payload || len < 2) return false;
    if (payload[0] > kMaxTimeoutIndex) return false;
    req.timeoutIndex = payload[0];
    req.configuration = payload[1];
    return true;
  }

  /** Build an End Device Timeout Response payload: status (1) + parent
      information (1). @return 2, or 0 on error. */
  static uint8_t buildResponse(uint8_t* out, uint8_t outMax,
                               const NwkEdTimeoutResponse& rsp) {
    if (!out || outMax < 2) return 0;
    out[0] = rsp.status;
    out[1] = rsp.parentInfo;
    return 2;
  }

  static bool parseResponse(const uint8_t* payload, uint8_t len,
                            NwkEdTimeoutResponse& rsp) {
    rsp = NwkEdTimeoutResponse();
    if (!payload || len < 2) return false;
    rsp.status = payload[0];
    rsp.parentInfo = payload[1];
    return true;
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_END_DEVICE_TIMEOUT_H
