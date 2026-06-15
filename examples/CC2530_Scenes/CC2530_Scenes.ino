/*
  CC2530_Scenes - ZCL Scenes cluster self-test.

  A scene captures a device's state (On/Off + Level) under a (group, scene) id
  so it can be recalled later. This sketch self-tests the scene store and the
  Store / Recall / Remove / Remove-All command handling, including capturing the
  current state on Store and writing it back on Recall. No radio traffic; runs
  on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testTable() {
  Serial.println("Scene table:");
  SceneEntry storage[4];
  ZigbeeSceneTable scenes(storage, 4);

  check(scenes.store(0x0001, 1, true, 200), "store scene (g1,s1) = on, level 200");
  check(scenes.store(0x0001, 2, false, 0), "store scene (g1,s2) = off");
  check(scenes.store(0x0002, 1, true, 80), "store scene (g2,s1)");
  check(scenes.count() == 3, "three scenes stored");

  bool on = false; uint8_t lvl = 0;
  check(scenes.recall(0x0001, 1, on, lvl) && on && lvl == 200,
        "recall (g1,s1) -> on, 200");
  check(scenes.recall(0x0001, 2, on, lvl) && !on, "recall (g1,s2) -> off");
  check(!scenes.recall(0x0009, 9, on, lvl), "recall unknown scene fails");

  // Re-store replaces.
  scenes.store(0x0001, 1, false, 10);
  scenes.recall(0x0001, 1, on, lvl);
  check(!on && lvl == 10, "re-store replaced the snapshot");
  check(scenes.count() == 3, "count unchanged after replace");

  // Remove all scenes of a group.
  uint8_t ids[4];
  check(scenes.scenesForGroup(0x0001, ids, 4) == 2, "group 1 has 2 scenes");
  check(scenes.removeAllForGroup(0x0001) == 2, "remove all of group 1 -> 2 gone");
  check(scenes.count() == 1 && scenes.has(0x0002, 1), "only group 2 scene remains");
}

void testCommands() {
  Serial.println("Scenes cluster commands:");
  SceneEntry storage[2];
  ZigbeeSceneTable scenes(storage, 2);

  // The device's current state.
  bool onOff = true;
  uint8_t level = 150;
  uint8_t status = 0xFF;

  // Store the current state into scene (g5, s3).
  uint8_t cmd[4];
  uint8_t n = ZigbeeScenesCluster::buildGroupScene(cmd, sizeof(cmd), 0x0005, 3);
  check(n == 3, "Store/Recall payload = group(2) + scene(1)");
  check(ZigbeeScenesCluster::handle(scenes, SCENES_CMD_STORE, cmd, n, onOff,
                                    level, status) &&
            status == SCENES_STATUS_SUCCESS && scenes.has(0x0005, 3),
        "Store captured the current state");

  // Change the device state, then Recall -> handle writes the stored state back.
  onOff = false; level = 0;
  check(ZigbeeScenesCluster::handle(scenes, SCENES_CMD_RECALL, cmd, n, onOff,
                                    level, status) &&
            onOff == true && level == 150,
        "Recall restored on + level 150 into the device state");

  // Recall a missing scene -> NOT_FOUND.
  ZigbeeScenesCluster::buildGroupScene(cmd, sizeof(cmd), 0x0005, 9);
  ZigbeeScenesCluster::handle(scenes, SCENES_CMD_RECALL, cmd, 3, onOff, level, status);
  check(status == SCENES_STATUS_NOT_FOUND, "Recall of unknown scene -> NOT_FOUND");

  // Remove the scene.
  ZigbeeScenesCluster::buildGroupScene(cmd, sizeof(cmd), 0x0005, 3);
  ZigbeeScenesCluster::handle(scenes, SCENES_CMD_REMOVE, cmd, 3, onOff, level, status);
  check(status == SCENES_STATUS_SUCCESS && !scenes.has(0x0005, 3),
        "Remove deleted the scene");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee Scenes self-test ===");

  testTable();
  testCommands();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
