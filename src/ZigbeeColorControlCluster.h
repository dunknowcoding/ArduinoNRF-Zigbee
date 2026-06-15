/*
  ZigbeeColorControlCluster.h - ZCL Color Control cluster (0x0300) tooling +
  behavior.

  Color Control is the third lighting cluster, on top of On/Off and Level: it
  sets a light's color, either as hue+saturation, as CIE xy, or as a color
  temperature (in mireds). A controller drives Move to Hue and Saturation /
  Move to Color / Move to Color Temperature (each with a transition time).

  This header builds those command payloads and applies a received command to a
  local color state (CurrentHue / CurrentSaturation / CurrentX / CurrentY /
  ColorTemperatureMireds). With On/Off + Level Control this covers a full color
  smart bulb's clusters.
*/
#ifndef NIUS_ZIGBEE_COLOR_CONTROL_CLUSTER_H
#define NIUS_ZIGBEE_COLOR_CONTROL_CLUSTER_H

#include <Arduino.h>

namespace nzb {

enum ZclColorCommandId : uint8_t {
  COLOR_CMD_MOVE_TO_HUE = 0x00,
  COLOR_CMD_MOVE_TO_SATURATION = 0x03,
  COLOR_CMD_MOVE_TO_HUE_AND_SATURATION = 0x06,
  COLOR_CMD_MOVE_TO_COLOR = 0x07,
  COLOR_CMD_MOVE_TO_COLOR_TEMPERATURE = 0x0A,
};

// Which color representation a state currently reflects.
enum ZclColorMode : uint8_t {
  COLOR_MODE_HUE_SAT = 0x00,
  COLOR_MODE_XY = 0x01,
  COLOR_MODE_TEMPERATURE = 0x02,
};

struct ColorState {
  uint8_t hue;             // CurrentHue, 0..254
  uint8_t saturation;      // CurrentSaturation, 0..254
  uint16_t x;              // CurrentX (CIE), 0..65279
  uint16_t y;              // CurrentY (CIE)
  uint16_t temperatureMireds;  // ColorTemperatureMireds
  uint8_t mode;            // ZclColorMode reflecting the last command

  ColorState()
      : hue(0), saturation(0), x(0), y(0), temperatureMireds(0),
        mode(COLOR_MODE_HUE_SAT) {}
};

class ZigbeeColorControlCluster {
 public:
  static const uint16_t kAttrCurrentHue = 0x0000;
  static const uint16_t kAttrCurrentSaturation = 0x0001;
  static const uint16_t kAttrCurrentX = 0x0003;
  static const uint16_t kAttrCurrentY = 0x0004;
  static const uint16_t kAttrColorTemperatureMireds = 0x0007;

  /** Move to Hue and Saturation: hue(1) + saturation(1) + transition time(2). */
  static uint8_t buildMoveToHueSat(uint8_t* out, uint8_t outMax, uint8_t hue,
                                   uint8_t saturation, uint16_t transitionTime) {
    if (!out || outMax < 4) return 0;
    out[0] = hue;
    out[1] = saturation;
    putLe16(&out[2], transitionTime);
    return 4;
  }
  /** Move to Color (CIE xy): x(2) + y(2) + transition time(2). */
  static uint8_t buildMoveToColor(uint8_t* out, uint8_t outMax, uint16_t x,
                                  uint16_t y, uint16_t transitionTime) {
    if (!out || outMax < 6) return 0;
    putLe16(&out[0], x);
    putLe16(&out[2], y);
    putLe16(&out[4], transitionTime);
    return 6;
  }
  /** Move to Color Temperature: color temperature mireds(2) + transition(2). */
  static uint8_t buildMoveToColorTemperature(uint8_t* out, uint8_t outMax,
                                             uint16_t mireds,
                                             uint16_t transitionTime) {
    if (!out || outMax < 4) return 0;
    putLe16(&out[0], mireds);
    putLe16(&out[2], transitionTime);
    return 4;
  }

  /** Apply a received Color Control command to @p state. @return true if the
      color changed (unknown commands return false). */
  static bool applyCommand(uint8_t commandId, const uint8_t* payload,
                           uint8_t payloadLen, ColorState& state) {
    switch (commandId) {
      case COLOR_CMD_MOVE_TO_HUE:
        if (payloadLen < 1) return false;
        state.hue = payload[0];
        state.mode = COLOR_MODE_HUE_SAT;
        return true;
      case COLOR_CMD_MOVE_TO_SATURATION:
        if (payloadLen < 1) return false;
        state.saturation = payload[0];
        state.mode = COLOR_MODE_HUE_SAT;
        return true;
      case COLOR_CMD_MOVE_TO_HUE_AND_SATURATION:
        if (payloadLen < 2) return false;
        state.hue = payload[0];
        state.saturation = payload[1];
        state.mode = COLOR_MODE_HUE_SAT;
        return true;
      case COLOR_CMD_MOVE_TO_COLOR:
        if (payloadLen < 4) return false;
        state.x = getLe16(&payload[0]);
        state.y = getLe16(&payload[2]);
        state.mode = COLOR_MODE_XY;
        return true;
      case COLOR_CMD_MOVE_TO_COLOR_TEMPERATURE:
        if (payloadLen < 2) return false;
        state.temperatureMireds = getLe16(&payload[0]);
        state.mode = COLOR_MODE_TEMPERATURE;
        return true;
      default:
        return false;
    }
  }

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

#endif  // NIUS_ZIGBEE_COLOR_CONTROL_CLUSTER_H
