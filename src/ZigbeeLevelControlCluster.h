/*
  ZigbeeLevelControlCluster.h - ZCL Level Control cluster (0x0008) tooling +
  behavior.

  Level Control is the dimming half of a dimmable light: on top of On/Off, it
  carries a CurrentLevel attribute (0..254) that a controller drives with Move
  to Level / Move / Step / Stop commands (each with an optional transition
  time, and an optional "with On/Off" variant that also flips the light on/off).

  This header builds those cluster-specific command payloads (the ZCL header
  comes from ZigbeeZcl::buildCommandFrame) and applies a received command to a
  local level - Move to Level sets it, Step nudges it (clamped to 0..254). The
  continuous Move/Stop commands carry no discrete level here; a real dimmer
  would ramp on a timer. Pairs with ZigbeeOnOffCluster for a full dimmable node.
*/
#ifndef NIUS_ZIGBEE_LEVEL_CONTROL_CLUSTER_H
#define NIUS_ZIGBEE_LEVEL_CONTROL_CLUSTER_H

#include <Arduino.h>

namespace nzb {

enum ZclLevelCommandId : uint8_t {
  LEVEL_CMD_MOVE_TO_LEVEL = 0x00,
  LEVEL_CMD_MOVE = 0x01,
  LEVEL_CMD_STEP = 0x02,
  LEVEL_CMD_STOP = 0x03,
  LEVEL_CMD_MOVE_TO_LEVEL_WITH_ONOFF = 0x04,
  LEVEL_CMD_MOVE_WITH_ONOFF = 0x05,
  LEVEL_CMD_STEP_WITH_ONOFF = 0x06,
  LEVEL_CMD_STOP_WITH_ONOFF = 0x07,
};

enum ZclLevelMode : uint8_t {
  LEVEL_DIR_UP = 0x00,
  LEVEL_DIR_DOWN = 0x01,
};

class ZigbeeLevelControlCluster {
 public:
  static const uint8_t kMaxLevel = 254;
  static const uint16_t kAttrCurrentLevel = 0x0000;

  /** Move to Level: level(1) + transition time(2, 1/10 s). */
  static uint8_t buildMoveToLevel(uint8_t* out, uint8_t outMax, uint8_t level,
                                  uint16_t transitionTime) {
    if (!out || outMax < 3) return 0;
    out[0] = level > kMaxLevel ? kMaxLevel : level;
    putLe16(&out[1], transitionTime);
    return 3;
  }
  /** Move: move mode(1) + rate(1, units/s; 0xFF = default). */
  static uint8_t buildMove(uint8_t* out, uint8_t outMax, uint8_t moveMode,
                           uint8_t rate) {
    if (!out || outMax < 2) return 0;
    out[0] = moveMode;
    out[1] = rate;
    return 2;
  }
  /** Step: step mode(1) + step size(1) + transition time(2). */
  static uint8_t buildStep(uint8_t* out, uint8_t outMax, uint8_t stepMode,
                           uint8_t stepSize, uint16_t transitionTime) {
    if (!out || outMax < 4) return 0;
    out[0] = stepMode;
    out[1] = stepSize;
    putLe16(&out[2], transitionTime);
    return 4;
  }
  // Stop carries no payload - send a ZCL command frame with command id
  // LEVEL_CMD_STOP and a zero-length payload (no builder needed).

  /** Apply a received Level Control command to @p currentLevel. Move to Level
      sets it; Step nudges it (clamped to 0..254). The "with On/Off" variants
      behave identically here for the level itself. Move/Stop are continuous and
      leave the level unchanged. @return true if the level changed. */
  static bool applyCommand(uint8_t commandId, const uint8_t* payload,
                           uint8_t payloadLen, uint8_t& currentLevel) {
    switch (commandId) {
      case LEVEL_CMD_MOVE_TO_LEVEL:
      case LEVEL_CMD_MOVE_TO_LEVEL_WITH_ONOFF: {
        if (payloadLen < 1) return false;
        uint8_t target = payload[0] > kMaxLevel ? kMaxLevel : payload[0];
        if (target == currentLevel) return false;
        currentLevel = target;
        return true;
      }
      case LEVEL_CMD_STEP:
      case LEVEL_CMD_STEP_WITH_ONOFF: {
        if (payloadLen < 2) return false;
        uint8_t mode = payload[0];
        uint8_t size = payload[1];
        uint16_t lvl = currentLevel;
        if (mode == LEVEL_DIR_UP) {
          lvl += size;
          if (lvl > kMaxLevel) lvl = kMaxLevel;
        } else {  // down
          lvl = (size >= lvl) ? 0 : (uint16_t)(lvl - size);
        }
        if ((uint8_t)lvl == currentLevel) return false;
        currentLevel = (uint8_t)lvl;
        return true;
      }
      default:  // Move / Stop (continuous) - no discrete change here
        return false;
    }
  }

 private:
  static void putLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_LEVEL_CONTROL_CLUSTER_H
