/*
  CC2530_LevelControl - ZCL Level Control cluster self-test (dimming).

  Level Control adds a CurrentLevel (0..254) to a light, driven by Move to
  Level / Step commands. This sketch self-tests the command payloads and the
  apply-to-local-level behavior (set, step up/down with clamping). No radio
  traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testBuild() {
  Serial.println("Level Control command payloads:");
  uint8_t buf[8];
  uint8_t n = ZigbeeLevelControlCluster::buildMoveToLevel(buf, sizeof(buf), 128, 10);
  check(n == 3 && buf[0] == 128, "Move to Level = level(1) + transition(2)");
  // Level is clamped to 254 on build.
  ZigbeeLevelControlCluster::buildMoveToLevel(buf, sizeof(buf), 255, 0);
  check(buf[0] == 254, "Move to Level clamps level to 254");

  n = ZigbeeLevelControlCluster::buildStep(buf, sizeof(buf), LEVEL_DIR_UP, 20, 5);
  check(n == 4 && buf[0] == LEVEL_DIR_UP && buf[1] == 20,
        "Step = mode(1) + size(1) + transition(2)");
  n = ZigbeeLevelControlCluster::buildMove(buf, sizeof(buf), LEVEL_DIR_DOWN, 0xFF);
  check(n == 2 && buf[0] == LEVEL_DIR_DOWN, "Move = mode(1) + rate(1)");
}

void testApply() {
  Serial.println("Apply to current level:");
  uint8_t level = 100;

  uint8_t cmd[4];
  uint8_t n = ZigbeeLevelControlCluster::buildMoveToLevel(cmd, sizeof(cmd), 200, 0);
  check(ZigbeeLevelControlCluster::applyCommand(LEVEL_CMD_MOVE_TO_LEVEL, cmd, n, level) &&
            level == 200,
        "Move to Level 200 sets the level");
  check(!ZigbeeLevelControlCluster::applyCommand(LEVEL_CMD_MOVE_TO_LEVEL, cmd, n, level),
        "Move to the same level reports no change");

  // Step up by 100 from 200 -> clamps at 254.
  n = ZigbeeLevelControlCluster::buildStep(cmd, sizeof(cmd), LEVEL_DIR_UP, 100, 0);
  check(ZigbeeLevelControlCluster::applyCommand(LEVEL_CMD_STEP, cmd, n, level) &&
            level == 254,
        "Step up clamps at 254");

  // Step down by 50 -> 204.
  n = ZigbeeLevelControlCluster::buildStep(cmd, sizeof(cmd), LEVEL_DIR_DOWN, 50, 0);
  check(ZigbeeLevelControlCluster::applyCommand(LEVEL_CMD_STEP, cmd, n, level) &&
            level == 204,
        "Step down by 50 -> 204");

  // Step down past 0 -> clamps at 0.
  n = ZigbeeLevelControlCluster::buildStep(cmd, sizeof(cmd), LEVEL_DIR_DOWN, 255, 0);
  check(ZigbeeLevelControlCluster::applyCommand(LEVEL_CMD_STEP, cmd, n, level) &&
            level == 0,
        "Step down past 0 clamps at 0");

  // Stop / Move are continuous: no discrete level change.
  check(!ZigbeeLevelControlCluster::applyCommand(LEVEL_CMD_STOP, nullptr, 0, level),
        "Stop does not change the level");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee Level Control self-test ===");

  testBuild();
  testApply();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
