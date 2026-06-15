/*
  ZigbeeThermostatCluster.h - ZCL Thermostat cluster (0x0201).

  A thermostat reports the measured LocalTemperature and holds heating/cooling
  setpoints and a system mode (off / auto / cool / heat). A controller nudges
  the setpoints with Setpoint Raise/Lower. Temperatures and setpoints are signed
  16-bit values in 0.01 C; the raise/lower amount is in 0.1 C steps.

  This header builds/parses the Setpoint Raise/Lower command and applies it to a
  local ThermostatState, plus the system mode.
*/
#ifndef NIUS_ZIGBEE_THERMOSTAT_CLUSTER_H
#define NIUS_ZIGBEE_THERMOSTAT_CLUSTER_H

#include <Arduino.h>

namespace nzb {

enum ZclThermostatCommandId : uint8_t {
  THERMOSTAT_CMD_SETPOINT_RAISE_LOWER = 0x00,
};

enum ZclThermostatSetpointMode : uint8_t {
  THERMOSTAT_SETPOINT_HEAT = 0x00,
  THERMOSTAT_SETPOINT_COOL = 0x01,
  THERMOSTAT_SETPOINT_BOTH = 0x02,
};

enum ZclThermostatSystemMode : uint8_t {
  THERMOSTAT_MODE_OFF = 0x00,
  THERMOSTAT_MODE_AUTO = 0x01,
  THERMOSTAT_MODE_COOL = 0x03,
  THERMOSTAT_MODE_HEAT = 0x04,
};

struct ThermostatState {
  int16_t localTemperature;     // 0.01 C
  int16_t heatingSetpoint;      // 0.01 C
  int16_t coolingSetpoint;      // 0.01 C
  uint8_t systemMode;
  ThermostatState()
      : localTemperature(2100), heatingSetpoint(2000), coolingSetpoint(2600),
        systemMode(THERMOSTAT_MODE_HEAT) {}
};

class ZigbeeThermostatCluster {
 public:
  static const uint16_t kAttrLocalTemperature = 0x0000;
  static const uint16_t kAttrOccupiedCoolingSetpoint = 0x0011;
  static const uint16_t kAttrOccupiedHeatingSetpoint = 0x0012;
  static const uint16_t kAttrSystemMode = 0x001C;

  /** Setpoint Raise/Lower: mode(1) + amount(1, signed, 0.1 C steps). */
  static uint8_t buildSetpointRaiseLower(uint8_t* out, uint8_t outMax,
                                         uint8_t mode, int8_t amount) {
    if (!out || outMax < 2) return 0;
    out[0] = mode;
    out[1] = (uint8_t)amount;
    return 2;
  }

  /** Apply a Setpoint Raise/Lower to @p state (amount is 0.1 C; setpoints are
      0.01 C, so each step moves a setpoint by amount*10). @return true if a
      setpoint changed. */
  static bool applyCommand(uint8_t commandId, const uint8_t* payload,
                           uint8_t payloadLen, ThermostatState& state) {
    if (commandId != THERMOSTAT_CMD_SETPOINT_RAISE_LOWER || payloadLen < 2)
      return false;
    uint8_t mode = payload[0];
    int16_t delta = (int16_t)((int8_t)payload[1]) * 10;  // 0.1 C -> 0.01 C
    bool changed = false;
    if (mode == THERMOSTAT_SETPOINT_HEAT || mode == THERMOSTAT_SETPOINT_BOTH) {
      state.heatingSetpoint = (int16_t)(state.heatingSetpoint + delta);
      changed = true;
    }
    if (mode == THERMOSTAT_SETPOINT_COOL || mode == THERMOSTAT_SETPOINT_BOTH) {
      state.coolingSetpoint = (int16_t)(state.coolingSetpoint + delta);
      changed = true;
    }
    return changed;
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_THERMOSTAT_CLUSTER_H
