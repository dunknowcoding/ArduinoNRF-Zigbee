/*
  ZigbeeIdentifyCluster.h - ZCL Identify cluster (0x0003) tooling + behavior.

  Identify is how a commissioning tool says "show me which device you are": it
  sends Identify(time) and the device makes itself visible (blink an LED, etc.)
  for that many seconds, counting an IdentifyTime attribute down to zero. A tool
  can also broadcast Identify Query to find devices currently identifying, and
  trigger named effects (blink / breathe / okay).

  This header builds/parses the Identify command payloads and provides the
  countdown behavior: apply a received Identify command to a local IdentifyTime,
  tick it down once per second, and ask whether the device is still identifying.
*/
#ifndef NIUS_ZIGBEE_IDENTIFY_CLUSTER_H
#define NIUS_ZIGBEE_IDENTIFY_CLUSTER_H

#include <Arduino.h>

namespace nzb {

enum ZclIdentifyCommandId : uint8_t {
  IDENTIFY_CMD_IDENTIFY = 0x00,
  IDENTIFY_CMD_QUERY = 0x01,
  IDENTIFY_CMD_TRIGGER_EFFECT = 0x40,
};
// Server -> client response command id (Identify Query Response).
enum ZclIdentifyResponseId : uint8_t {
  IDENTIFY_RSP_QUERY = 0x00,
};
enum ZclIdentifyEffect : uint8_t {
  IDENTIFY_EFFECT_BLINK = 0x00,
  IDENTIFY_EFFECT_BREATHE = 0x01,
  IDENTIFY_EFFECT_OKAY = 0x02,
  IDENTIFY_EFFECT_FINISH = 0xFE,
  IDENTIFY_EFFECT_STOP = 0xFF,
};

class ZigbeeIdentifyCluster {
 public:
  static const uint16_t kAttrIdentifyTime = 0x0000;

  /** Identify: identify time(2, seconds). 0 stops identifying. */
  static uint8_t buildIdentify(uint8_t* out, uint8_t outMax, uint16_t seconds) {
    if (!out || outMax < 2) return 0;
    putLe16(out, seconds);
    return 2;
  }
  /** Identify Query: no payload (returns 0). Send with command id QUERY. */

  /** Identify Query Response: timeout(2, seconds remaining). */
  static uint8_t buildQueryResponse(uint8_t* out, uint8_t outMax,
                                    uint16_t timeoutSeconds) {
    if (!out || outMax < 2) return 0;
    putLe16(out, timeoutSeconds);
    return 2;
  }
  /** Trigger Effect: effect id(1) + effect variant(1). */
  static uint8_t buildTriggerEffect(uint8_t* out, uint8_t outMax,
                                    uint8_t effectId, uint8_t variant) {
    if (!out || outMax < 2) return 0;
    out[0] = effectId;
    out[1] = variant;
    return 2;
  }

  static bool parseIdentify(const uint8_t* payload, uint8_t len,
                            uint16_t& seconds) {
    if (!payload || len < 2) return false;
    seconds = getLe16(payload);
    return true;
  }

  /** Apply a received Identify command to @p identifyTime (seconds remaining).
      @return true if the device should now be identifying (time > 0). */
  static bool applyIdentify(const uint8_t* payload, uint8_t len,
                            uint16_t& identifyTime) {
    uint16_t s = 0;
    if (!parseIdentify(payload, len, s)) return identifyTime > 0;
    identifyTime = s;
    return identifyTime > 0;
  }

  /** Count @p identifyTime down by @p elapsedSeconds (call ~once per second).
      @return true while still identifying after the tick. */
  static bool tick(uint16_t& identifyTime, uint16_t elapsedSeconds = 1) {
    if (identifyTime == 0) return false;
    identifyTime = (elapsedSeconds >= identifyTime)
                       ? 0
                       : (uint16_t)(identifyTime - elapsedSeconds);
    return identifyTime > 0;
  }

  static bool isIdentifying(uint16_t identifyTime) { return identifyTime > 0; }

 private:
  static void putLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
  }
  static uint16_t getLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_IDENTIFY_CLUSTER_H
