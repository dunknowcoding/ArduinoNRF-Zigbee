/*
  CC2530_FindingBinding - Zigbee 3.0 BDB Finding & Binding self-test.

  A switch (initiator) is paired to a light (target) by matching the switch's
  output (client) clusters against the light's input (server) clusters and
  creating a binding for each match. This sketch self-tests the cluster matching
  and the resulting bindings written into a ZigbeeBindingTable. No radio traffic;
  runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testMatch() {
  Serial.println("Cluster matching:");
  // A dimmer switch outputs On/Off + Level Control commands.
  const uint16_t switchOut[2] = {ZigbeeZcl::kClusterOnOff,
                                 ZigbeeZcl::kClusterLevelControl};
  // A dimmable light's input clusters: Basic, Identify, On/Off, Level Control.
  const uint16_t lightIn[4] = {ZigbeeZcl::kClusterBasic,
                               ZigbeeZcl::kClusterIdentify,
                               ZigbeeZcl::kClusterOnOff,
                               ZigbeeZcl::kClusterLevelControl};
  uint16_t matched[8];
  uint8_t n = ZigbeeFindingBinding::matchClusters(switchOut, 2, lightIn, 4,
                                                  matched, 8);
  check(n == 2, "switch out (OnOff,Level) matches light in -> 2 clusters");
  bool hasOnOff = false, hasLevel = false;
  for (uint8_t i = 0; i < n; ++i) {
    if (matched[i] == ZigbeeZcl::kClusterOnOff) hasOnOff = true;
    if (matched[i] == ZigbeeZcl::kClusterLevelControl) hasLevel = true;
  }
  check(hasOnOff && hasLevel, "matched clusters are On/Off and Level Control");

  // A cluster the target does not offer is not matched.
  const uint16_t switchOut2[2] = {ZigbeeZcl::kClusterOnOff,
                                  ZigbeeZcl::kClusterColorControl};
  n = ZigbeeFindingBinding::matchClusters(switchOut2, 2, lightIn, 4, matched, 8);
  check(n == 1 && matched[0] == ZigbeeZcl::kClusterOnOff,
        "only the cluster the light supports is matched");
}

void testBind() {
  Serial.println("Finding & Binding -> binding table:");
  ZigbeeBinding storage[4];
  ZigbeeBindingTable binds(storage, 4);

  const uint64_t switchIeee = 0x1A62195E00000001ULL;
  const uint64_t lightIeee = 0x1A62195E00000031ULL;
  const uint16_t switchOut[2] = {ZigbeeZcl::kClusterOnOff,
                                 ZigbeeZcl::kClusterLevelControl};
  const uint16_t lightIn[3] = {ZigbeeZcl::kClusterOnOff,
                               ZigbeeZcl::kClusterLevelControl,
                               ZigbeeZcl::kClusterBasic};

  uint8_t created = ZigbeeFindingBinding::bindMatching(
      binds, switchIeee, /*srcEndpoint=*/1, switchOut, 2, lightIeee,
      /*targetEndpoint=*/3, lightIn, 3);
  check(created == 2, "two bindings created (On/Off + Level)");

  // The bindings point the switch's endpoint 1 at the light's endpoint 3.
  uint8_t cur = 0;
  const ZigbeeBinding* b = binds.next(1, ZigbeeZcl::kClusterOnOff, cur);
  check(b != nullptr && b->dstIeee == lightIeee && b->dstEndpoint == 3 &&
            b->dstAddrMode == ZB_BIND_ADDR_IEEE,
        "On/Off binding targets the light's IEEE + endpoint 3");
  check(binds.next(1, ZigbeeZcl::kClusterOnOff, cur) == nullptr,
        "exactly one On/Off binding");
  cur = 0;
  check(binds.next(1, ZigbeeZcl::kClusterLevelControl, cur) != nullptr,
        "Level Control binding present");

  // Re-running is idempotent (no duplicate binds).
  uint8_t again = ZigbeeFindingBinding::bindMatching(
      binds, switchIeee, 1, switchOut, 2, lightIeee, 3, lightIn, 3);
  check(again == 2, "re-run returns the same 2 (idempotent add)");
  cur = 0;
  binds.next(1, ZigbeeZcl::kClusterOnOff, cur);
  check(binds.next(1, ZigbeeZcl::kClusterOnOff, cur) == nullptr,
        "still exactly one On/Off binding after re-run");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee Finding & Binding self-test ===");

  testMatch();
  testBind();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
