/*
  ZigbeeWindowCoveringCluster.h - ZCL Window Covering cluster (0x0102).

  A motorized blind/shade/curtain: a controller raises (Up/Open) or lowers
  (Down/Close) it, Stops it, or drives it to a lift/tilt percentage. The device
  reports its CurrentPositionLiftPercentage / CurrentPositionTiltPercentage
  (0 = fully open, 100 = fully closed).

  This header builds the commands and applies them to a local
  WindowCoveringState.
*/
#ifndef NIUS_ZIGBEE_WINDOW_COVERING_CLUSTER_H
#define NIUS_ZIGBEE_WINDOW_COVERING_CLUSTER_H

#include <Arduino.h>

namespace nzb {

enum ZclWindowCoveringCommandId : uint8_t {
  WINDOW_CMD_UP_OPEN = 0x00,
  WINDOW_CMD_DOWN_CLOSE = 0x01,
  WINDOW_CMD_STOP = 0x02,
  WINDOW_CMD_GO_TO_LIFT_PERCENTAGE = 0x05,
  WINDOW_CMD_GO_TO_TILT_PERCENTAGE = 0x08,
};

struct WindowCoveringState {
  uint8_t liftPercentage;   // 0 = open, 100 = closed
  uint8_t tiltPercentage;
  bool moving;
  WindowCoveringState() : liftPercentage(0), tiltPercentage(0), moving(false) {}
};

class ZigbeeWindowCoveringCluster {
 public:
  static const uint16_t kAttrCurrentPositionLiftPercentage = 0x0008;
  static const uint16_t kAttrCurrentPositionTiltPercentage = 0x0009;

  /** Go to Lift Percentage: percentage(1, 0..100). */
  static uint8_t buildGoToLiftPercentage(uint8_t* out, uint8_t outMax,
                                         uint8_t percent) {
    if (!out || outMax < 1) return 0;
    out[0] = percent > 100 ? 100 : percent;
    return 1;
  }
  /** Go to Tilt Percentage: percentage(1, 0..100). */
  static uint8_t buildGoToTiltPercentage(uint8_t* out, uint8_t outMax,
                                         uint8_t percent) {
    if (!out || outMax < 1) return 0;
    out[0] = percent > 100 ? 100 : percent;
    return 1;
  }
  // Up/Open, Down/Close, Stop carry no payload.

  /** Apply a Window Covering command to @p state. Up/Open drives lift to 0
      (open), Down/Close to 100 (closed), Go to ... sets the percentage, Stop
      halts. @return true if the command was understood. */
  static bool applyCommand(uint8_t commandId, const uint8_t* payload,
                           uint8_t payloadLen, WindowCoveringState& state) {
    switch (commandId) {
      case WINDOW_CMD_UP_OPEN:
        state.liftPercentage = 0;
        state.moving = false;
        return true;
      case WINDOW_CMD_DOWN_CLOSE:
        state.liftPercentage = 100;
        state.moving = false;
        return true;
      case WINDOW_CMD_STOP:
        state.moving = false;
        return true;
      case WINDOW_CMD_GO_TO_LIFT_PERCENTAGE:
        if (payloadLen < 1) return false;
        state.liftPercentage = payload[0] > 100 ? 100 : payload[0];
        state.moving = false;
        return true;
      case WINDOW_CMD_GO_TO_TILT_PERCENTAGE:
        if (payloadLen < 1) return false;
        state.tiltPercentage = payload[0] > 100 ? 100 : payload[0];
        return true;
      default:
        return false;
    }
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_WINDOW_COVERING_CLUSTER_H
