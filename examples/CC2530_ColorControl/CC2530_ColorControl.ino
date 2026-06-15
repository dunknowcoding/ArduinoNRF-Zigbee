/*
  CC2530_ColorControl - ZCL Color Control cluster self-test.

  Color Control is the third lighting cluster (with On/Off and Level): it sets a
  light's color as hue+saturation, CIE xy, or color temperature. This sketch
  self-tests the command payloads and applying them to a local color state. No
  radio traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testBuild() {
  Serial.println("Color Control command payloads:");
  uint8_t buf[8];
  uint8_t n = ZigbeeColorControlCluster::buildMoveToHueSat(buf, sizeof(buf),
                                                           120, 200, 5);
  check(n == 4 && buf[0] == 120 && buf[1] == 200,
        "Move to Hue+Sat = hue(1) + sat(1) + transition(2)");

  n = ZigbeeColorControlCluster::buildMoveToColor(buf, sizeof(buf), 0x4B2D,
                                                  0x1234, 0);
  check(n == 6 && buf[0] == 0x2D && buf[1] == 0x4B, "Move to Color = x(2)+y(2)+t(2)");

  n = ZigbeeColorControlCluster::buildMoveToColorTemperature(buf, sizeof(buf),
                                                             370, 10);
  check(n == 4, "Move to Color Temperature = mireds(2) + transition(2)");
}

void testApply() {
  Serial.println("Apply to color state:");
  ColorState state;
  check(state.mode == COLOR_MODE_HUE_SAT && state.hue == 0, "initial state");

  uint8_t cmd[8];
  uint8_t n = ZigbeeColorControlCluster::buildMoveToHueSat(cmd, sizeof(cmd),
                                                           100, 254, 0);
  check(ZigbeeColorControlCluster::applyCommand(
            COLOR_CMD_MOVE_TO_HUE_AND_SATURATION, cmd, n, state) &&
            state.hue == 100 && state.saturation == 254 &&
            state.mode == COLOR_MODE_HUE_SAT,
        "Move to Hue+Sat sets hue/sat + hue-sat mode");

  n = ZigbeeColorControlCluster::buildMoveToColor(cmd, sizeof(cmd), 0xABCD,
                                                  0x1234, 0);
  check(ZigbeeColorControlCluster::applyCommand(COLOR_CMD_MOVE_TO_COLOR, cmd, n,
                                                state) &&
            state.x == 0xABCD && state.y == 0x1234 && state.mode == COLOR_MODE_XY,
        "Move to Color sets x/y + xy mode");

  n = ZigbeeColorControlCluster::buildMoveToColorTemperature(cmd, sizeof(cmd),
                                                             370, 0);
  check(ZigbeeColorControlCluster::applyCommand(
            COLOR_CMD_MOVE_TO_COLOR_TEMPERATURE, cmd, n, state) &&
            state.temperatureMireds == 370 &&
            state.mode == COLOR_MODE_TEMPERATURE,
        "Move to Color Temperature sets mireds + temperature mode");

  // An unknown command leaves the state untouched.
  check(!ZigbeeColorControlCluster::applyCommand(0x55, cmd, n, state),
        "unknown command ignored");
  // A truncated payload is rejected.
  check(!ZigbeeColorControlCluster::applyCommand(
            COLOR_CMD_MOVE_TO_HUE_AND_SATURATION, cmd, 1, state),
        "truncated Move to Hue+Sat rejected");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee Color Control self-test ===");

  testBuild();
  testApply();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
