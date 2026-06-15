/*
  CC2530_HaClusters - Thermostat + Window Covering cluster self-test.

  Two more common Home Automation device clusters: a Thermostat (Setpoint
  Raise/Lower over heating/cooling setpoints) and a Window Covering (Up/Down/
  Stop + go-to lift/tilt percentage). This sketch self-tests the commands and
  applying them to local state. No radio traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testThermostat() {
  Serial.println("Thermostat:");
  ThermostatState t;  // heating 2000 (20.00 C), cooling 2600
  check(t.heatingSetpoint == 2000 && t.coolingSetpoint == 2600, "default setpoints");

  uint8_t cmd[4];
  // Raise the heating setpoint by 1.5 C (15 x 0.1 C) -> +150 (0.01 C).
  uint8_t n = ZigbeeThermostatCluster::buildSetpointRaiseLower(
      cmd, sizeof(cmd), THERMOSTAT_SETPOINT_HEAT, 15);
  check(ZigbeeThermostatCluster::applyCommand(THERMOSTAT_CMD_SETPOINT_RAISE_LOWER,
                                              cmd, n, t) &&
            t.heatingSetpoint == 2150 && t.coolingSetpoint == 2600,
        "raise heating by 1.5 C -> 21.50 C (cooling unchanged)");

  // Lower both by 2.0 C.
  n = ZigbeeThermostatCluster::buildSetpointRaiseLower(
      cmd, sizeof(cmd), THERMOSTAT_SETPOINT_BOTH, -20);
  ZigbeeThermostatCluster::applyCommand(THERMOSTAT_CMD_SETPOINT_RAISE_LOWER, cmd, n, t);
  check(t.heatingSetpoint == 1950 && t.coolingSetpoint == 2400,
        "lower both by 2.0 C");

  check(!ZigbeeThermostatCluster::applyCommand(0x55, cmd, n, t),
        "unknown thermostat command ignored");
}

void testWindowCovering() {
  Serial.println("Window Covering:");
  WindowCoveringState w;  // lift 0 = open
  check(w.liftPercentage == 0, "starts open (lift 0%)");

  ZigbeeWindowCoveringCluster::applyCommand(WINDOW_CMD_DOWN_CLOSE, nullptr, 0, w);
  check(w.liftPercentage == 100, "Down/Close -> 100% (closed)");

  ZigbeeWindowCoveringCluster::applyCommand(WINDOW_CMD_UP_OPEN, nullptr, 0, w);
  check(w.liftPercentage == 0, "Up/Open -> 0% (open)");

  uint8_t cmd[2];
  uint8_t n = ZigbeeWindowCoveringCluster::buildGoToLiftPercentage(cmd, sizeof(cmd), 40);
  check(ZigbeeWindowCoveringCluster::applyCommand(
            WINDOW_CMD_GO_TO_LIFT_PERCENTAGE, cmd, n, w) &&
            w.liftPercentage == 40,
        "Go to Lift 40% sets the position");

  // Percentage is clamped to 100.
  n = ZigbeeWindowCoveringCluster::buildGoToLiftPercentage(cmd, sizeof(cmd), 200);
  check(cmd[0] == 100, "lift percentage clamps to 100 on build");

  n = ZigbeeWindowCoveringCluster::buildGoToTiltPercentage(cmd, sizeof(cmd), 75);
  ZigbeeWindowCoveringCluster::applyCommand(WINDOW_CMD_GO_TO_TILT_PERCENTAGE, cmd, n, w);
  check(w.tiltPercentage == 75, "Go to Tilt 75% sets the tilt");

  check(ZigbeeWindowCoveringCluster::applyCommand(WINDOW_CMD_STOP, nullptr, 0, w),
        "Stop is accepted");
  check(!ZigbeeWindowCoveringCluster::applyCommand(0x55, nullptr, 0, w),
        "unknown window command ignored");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee HA clusters (thermostat + blinds) self-test ===");

  testThermostat();
  testWindowCovering();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
