/*
  CC2530_Device - high-level ZigbeeLight device + IAS Zone sensor self-test.

  ZigbeeLight ties the lighting clusters (On/Off, Level, Color, Identify, Groups,
  Scenes) into one device: feed it received cluster-specific ZCL commands and
  read its state to drive hardware. This sketch drives a ZigbeeLight through a
  sequence of commands and checks its state, then exercises the IAS Zone sensor
  frames. No radio traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testLight() {
  Serial.println("ZigbeeLight device:");
  ZigbeeLight light;
  check(!light.isOn() && light.level() == 254, "initial: off, level 254");

  // On/Off Toggle -> on.
  uint8_t z[8];
  light.handleCommand(ZigbeeZcl::kClusterOnOff, ZCL_ON_OFF_CMD_TOGGLE, nullptr, 0);
  check(light.isOn(), "On/Off Toggle turned it on");

  // Level: Move to Level 100.
  uint8_t n = ZigbeeLevelControlCluster::buildMoveToLevel(z, sizeof(z), 100, 0);
  light.handleCommand(ZigbeeZcl::kClusterLevelControl, LEVEL_CMD_MOVE_TO_LEVEL, z, n);
  check(light.level() == 100, "Move to Level set level 100");

  // Color: Move to Hue and Saturation.
  n = ZigbeeColorControlCluster::buildMoveToHueSat(z, sizeof(z), 120, 200, 0);
  light.handleCommand(ZigbeeZcl::kClusterColorControl,
                      COLOR_CMD_MOVE_TO_HUE_AND_SATURATION, z, n);
  check(light.color().hue == 120 && light.color().saturation == 200,
        "Move to Hue+Sat set the color");

  // Identify for 5 s, then tick it down.
  n = ZigbeeIdentifyCluster::buildIdentify(z, sizeof(z), 5);
  light.handleCommand(ZigbeeZcl::kClusterIdentify, IDENTIFY_CMD_IDENTIFY, z, n);
  check(light.isIdentifying(), "Identify(5) -> identifying");
  for (uint8_t i = 0; i < 5; ++i) light.tickIdentify();
  check(!light.isIdentifying(), "stops identifying after 5 ticks");

  // Groups: Add Group 0x0007.
  n = ZigbeeGroupsCluster::buildAddGroup(z, sizeof(z), 0x0007);
  light.handleCommand(ZigbeeZcl::kClusterGroups, GROUPS_CMD_ADD, z, n);
  check(light.groups().isMember(0x0007), "Add Group joined the light to 0x0007");

  // Scenes: Store the current state, change it, then Recall.
  n = ZigbeeScenesCluster::buildGroupScene(z, sizeof(z), 0x0007, 1);
  light.handleCommand(ZigbeeZcl::kClusterScenes, SCENES_CMD_STORE, z, n);  // capture on, 100
  light.handleCommand(ZigbeeZcl::kClusterOnOff, ZCL_ON_OFF_CMD_OFF, nullptr, 0);
  light.setLevel(10);
  check(!light.isOn() && light.level() == 10, "state changed after Store");
  light.handleCommand(ZigbeeZcl::kClusterScenes, SCENES_CMD_RECALL, z, n);
  check(light.isOn() && light.level() == 100, "Recall restored on + level 100");

  check(!light.handleCommand(0x9999, 0, nullptr, 0), "unknown cluster ignored");
}

void testIasZone() {
  Serial.println("IAS Zone sensor:");
  uint8_t buf[8];

  // A contact sensor enrolls.
  uint8_t n = ZigbeeIasZoneCluster::buildEnrollRequest(buf, sizeof(buf),
                                                       IAS_ZONE_TYPE_CONTACT,
                                                       0x1234);
  uint16_t zt = 0, mc = 0;
  check(n == 4 && ZigbeeIasZoneCluster::parseEnrollRequest(buf, n, zt, mc) &&
            zt == IAS_ZONE_TYPE_CONTACT && mc == 0x1234,
        "Zone Enroll Request round-trip (contact sensor)");

  n = ZigbeeIasZoneCluster::buildEnrollResponse(buf, sizeof(buf),
                                                IAS_ENROLL_SUCCESS, 1);
  uint8_t code = 0xFF, zid = 0;
  check(ZigbeeIasZoneCluster::parseEnrollResponse(buf, n, code, zid) &&
            code == IAS_ENROLL_SUCCESS && zid == 1,
        "Zone Enroll Response round-trip (success, zone 1)");

  // It reports an alarm with a tamper bit.
  n = ZigbeeIasZoneCluster::buildStatusChangeNotification(
      buf, sizeof(buf), IAS_STATUS_ALARM1 | IAS_STATUS_TAMPER, 1, 0);
  uint16_t status = 0, delay = 0; uint8_t zone = 0;
  check(n == 6 && ZigbeeIasZoneCluster::parseStatusChangeNotification(
                      buf, n, status, zone, delay),
        "parse Zone Status Change Notification");
  check((status & IAS_STATUS_ALARM1) && (status & IAS_STATUS_TAMPER) && zone == 1,
        "status carries ALARM1 + TAMPER for zone 1");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee device (light + IAS sensor) self-test ===");

  testLight();
  testIasZone();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
