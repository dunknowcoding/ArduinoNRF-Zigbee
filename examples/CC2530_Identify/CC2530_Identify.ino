/*
  CC2530_Identify - ZCL Identify cluster self-test.

  Identify makes a device show which one it is (blink) for a number of seconds.
  This sketch self-tests the Identify command payloads and the IdentifyTime
  countdown behavior. No radio traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testFrames() {
  Serial.println("Identify command payloads:");
  uint8_t buf[4];
  uint8_t n = ZigbeeIdentifyCluster::buildIdentify(buf, sizeof(buf), 30);
  check(n == 2, "Identify = identify time(2)");
  uint16_t s = 0;
  check(ZigbeeIdentifyCluster::parseIdentify(buf, n, s) && s == 30,
        "identify time round-trip (30 s)");

  n = ZigbeeIdentifyCluster::buildQueryResponse(buf, sizeof(buf), 12);
  check(n == 2, "Identify Query Response = timeout(2)");

  n = ZigbeeIdentifyCluster::buildTriggerEffect(buf, sizeof(buf),
                                                IDENTIFY_EFFECT_BLINK, 0);
  check(n == 2 && buf[0] == IDENTIFY_EFFECT_BLINK, "Trigger Effect = id + variant");
}

void testCountdown() {
  Serial.println("IdentifyTime countdown:");
  uint16_t t = 0;
  check(!ZigbeeIdentifyCluster::isIdentifying(t), "not identifying initially");

  uint8_t cmd[2];
  ZigbeeIdentifyCluster::buildIdentify(cmd, sizeof(cmd), 3);
  check(ZigbeeIdentifyCluster::applyIdentify(cmd, 2, t) && t == 3,
        "Identify(3) starts a 3 s countdown");

  check(ZigbeeIdentifyCluster::tick(t) && t == 2, "tick -> 2 s, still identifying");
  check(ZigbeeIdentifyCluster::tick(t) && t == 1, "tick -> 1 s");
  check(!ZigbeeIdentifyCluster::tick(t) && t == 0, "tick -> 0, done identifying");
  check(!ZigbeeIdentifyCluster::isIdentifying(t), "no longer identifying");
  check(!ZigbeeIdentifyCluster::tick(t), "ticking at 0 stays done");

  // Identify(0) stops immediately.
  ZigbeeIdentifyCluster::buildIdentify(cmd, sizeof(cmd), 10);
  ZigbeeIdentifyCluster::applyIdentify(cmd, 2, t);
  ZigbeeIdentifyCluster::buildIdentify(cmd, sizeof(cmd), 0);
  check(!ZigbeeIdentifyCluster::applyIdentify(cmd, 2, t) && t == 0,
        "Identify(0) stops identifying");

  // A larger elapsed than remaining clamps to 0.
  ZigbeeIdentifyCluster::buildIdentify(cmd, sizeof(cmd), 5);
  ZigbeeIdentifyCluster::applyIdentify(cmd, 2, t);
  check(!ZigbeeIdentifyCluster::tick(t, 10) && t == 0, "elapsed > remaining -> 0");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee Identify self-test ===");

  testFrames();
  testCountdown();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
