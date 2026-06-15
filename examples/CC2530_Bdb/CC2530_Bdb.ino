/*
  CC2530_Bdb - Base Device Behaviour commissioning state machine self-test.

  Zigbee 3.0 commissioning runs a set of modes in a fixed order: touchlink ->
  network steering -> network formation -> finding & binding. ZigbeeBdb
  orchestrates them: the app asks for a mode bitmask, the state machine hands
  out one mode at a time in precedence order, the host runs it and reports the
  status, and a failed network step ends commissioning. This sketch self-tests
  those transitions for the coordinator (form + F&B), router (steer + F&B), and
  failure paths.

  No radio traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testCoordinator() {
  Serial.println("Coordinator: formation + finding & binding:");
  ZigbeeBdb bdb;
  uint8_t mode = bdb.start(BDB_MODE_NETWORK_FORMATION | BDB_MODE_FINDING_BINDING);
  check(mode == BDB_MODE_NETWORK_FORMATION, "formation runs before finding&binding");
  mode = bdb.reportResult(BDB_STATUS_SUCCESS);
  check(mode == BDB_MODE_FINDING_BINDING, "finding & binding runs after formation");
  mode = bdb.reportResult(BDB_STATUS_SUCCESS);
  check(mode == 0 && bdb.state() == BDB_DONE, "commissioning complete");
  check(bdb.overallStatus() == BDB_STATUS_SUCCESS, "overall status SUCCESS");
}

void testRouterSteering() {
  Serial.println("Router: steering + finding & binding:");
  ZigbeeBdb bdb;
  uint8_t mode = bdb.start(BDB_MODE_FINDING_BINDING | BDB_MODE_NETWORK_STEERING);
  // Requested out of order, but precedence puts steering first.
  check(mode == BDB_MODE_NETWORK_STEERING, "steering runs first regardless of bit order");
  mode = bdb.reportResult(BDB_STATUS_SUCCESS);
  check(mode == BDB_MODE_FINDING_BINDING && bdb.isActive(),
        "finding & binding follows successful steering");
  bdb.reportResult(BDB_STATUS_SUCCESS);
  check(bdb.overallStatus() == BDB_STATUS_SUCCESS, "router commissioned");
}

void testNoNetworkAborts() {
  Serial.println("Steering fails -> finding & binding skipped:");
  ZigbeeBdb bdb;
  bdb.start(BDB_MODE_NETWORK_STEERING | BDB_MODE_FINDING_BINDING);
  uint8_t mode = bdb.reportResult(BDB_STATUS_NO_NETWORK);
  check(mode == 0 && bdb.state() == BDB_DONE,
        "no network ends commissioning (no F&B)");
  check(bdb.overallStatus() == BDB_STATUS_NO_NETWORK, "overall status NO_NETWORK");
}

void testTouchlinkFirst() {
  Serial.println("Full mode set runs in BDB precedence order:");
  ZigbeeBdb bdb;
  uint8_t mode = bdb.start(BDB_MODE_FINDING_BINDING | BDB_MODE_NETWORK_FORMATION |
                           BDB_MODE_INITIATOR_TOUCHLINK | BDB_MODE_NETWORK_STEERING);
  check(mode == BDB_MODE_INITIATOR_TOUCHLINK, "touchlink first");
  mode = bdb.reportResult(BDB_STATUS_SUCCESS);
  check(mode == BDB_MODE_NETWORK_STEERING, "then steering");
  mode = bdb.reportResult(BDB_STATUS_SUCCESS);
  check(mode == BDB_MODE_NETWORK_FORMATION, "then formation");
  mode = bdb.reportResult(BDB_STATUS_SUCCESS);
  check(mode == BDB_MODE_FINDING_BINDING, "then finding & binding");
  mode = bdb.reportResult(BDB_STATUS_SUCCESS);
  check(mode == 0 && bdb.overallStatus() == BDB_STATUS_SUCCESS, "all four done");
}

void testFindingBindingFailure() {
  Serial.println("Finding & binding with no identify response:");
  ZigbeeBdb bdb;
  bdb.start(BDB_MODE_FINDING_BINDING);
  uint8_t mode = bdb.reportResult(BDB_STATUS_NO_IDENTIFY_QUERY_RESPONSE);
  check(mode == 0 && bdb.state() == BDB_DONE, "commissioning ends");
  check(bdb.overallStatus() == BDB_STATUS_NO_IDENTIFY_QUERY_RESPONSE,
        "F&B failure surfaces (network kept, just no binding target)");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee BDB commissioning self-test ===");

  testCoordinator();
  testRouterSteering();
  testNoNetworkAborts();
  testTouchlinkFirst();
  testFindingBindingFailure();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
