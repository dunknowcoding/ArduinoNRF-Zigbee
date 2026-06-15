/*
  CC2530_InterPan - inter-PAN transmission framing self-test.

  Inter-PAN frames carry commissioning traffic (touchlink scans, etc.) before a
  device has joined a network: a MAC payload of a one-octet stub NWK header +
  a stripped-down APS header (frame control + optional group + cluster + profile)
  + the command. This sketch self-tests the inter-PAN APDU build/parse for
  broadcast / unicast / group delivery, and wraps a real touchlink Scan Request
  end-to-end (touchlink command -> inter-PAN broadcast -> parse -> recover the
  touchlink command).

  No radio traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

static const uint16_t kProfileZll = 0xC05E;
static const uint16_t kClusterTouchlink = 0x1000;

void testInterPanFrame() {
  Serial.println("Inter-PAN APDU build/parse:");
  const uint8_t payload[3] = {0xAA, 0xBB, 0xCC};
  uint8_t apdu[24];

  // Broadcast (the touchlink scan case).
  uint8_t n = ZigbeeInterPan::build(apdu, sizeof(apdu), INTERPAN_BROADCAST, 0,
                                    kClusterTouchlink, kProfileZll, payload, 3);
  check(n == 2 + 4 + 3, "broadcast inter-PAN = stubNwk+apsFc+cluster+profile+payload");
  check(apdu[0] == 0x0B, "stub NWK header octet = 0x0B");

  InterPanFrame f;
  check(ZigbeeInterPan::parse(apdu, n, f), "parse broadcast inter-PAN");
  check(f.deliveryMode == INTERPAN_BROADCAST && f.clusterId == kClusterTouchlink &&
            f.profileId == kProfileZll,
        "delivery / cluster / profile round-trip");
  check(f.payloadLen == 3 && f.payload[0] == 0xAA && f.payload[2] == 0xCC,
        "payload after the stub headers");

  // Group delivery carries a 2-byte group address before the cluster.
  n = ZigbeeInterPan::build(apdu, sizeof(apdu), INTERPAN_GROUP, 0xBEEF,
                            kClusterTouchlink, kProfileZll, payload, 3);
  check(ZigbeeInterPan::parse(apdu, n, f) && f.deliveryMode == INTERPAN_GROUP &&
            f.groupAddress == 0xBEEF && f.payloadLen == 3,
        "group inter-PAN carries the group address");

  // Unicast (no group address).
  n = ZigbeeInterPan::build(apdu, sizeof(apdu), INTERPAN_UNICAST, 0,
                            kClusterTouchlink, kProfileZll, payload, 3);
  check(ZigbeeInterPan::parse(apdu, n, f) && f.deliveryMode == INTERPAN_UNICAST &&
            f.payloadLen == 3,
        "unicast inter-PAN (no group address)");
}

void testTouchlinkOverInterPan() {
  Serial.println("Touchlink Scan Request over inter-PAN:");
  // Build a touchlink Scan Request, wrap it in an inter-PAN broadcast, then
  // unwrap and recover the original command - the real on-air path.
  uint8_t scan[8];
  uint8_t sn = ZigbeeTouchlink::buildScanRequest(scan, sizeof(scan), 0x12345678,
                                                 0x12, 0x33);
  uint8_t apdu[24];
  uint8_t n = ZigbeeInterPan::build(apdu, sizeof(apdu), INTERPAN_BROADCAST, 0,
                                    kClusterTouchlink, kProfileZll, scan, sn);

  InterPanFrame f;
  check(ZigbeeInterPan::parse(apdu, n, f) && f.clusterId == kClusterTouchlink &&
            f.profileId == kProfileZll,
        "inter-PAN carries the touchlink commissioning cluster");
  uint32_t tid; uint8_t zb, zll;
  check(ZigbeeTouchlink::parseScanRequest(f.payload, f.payloadLen, tid, zb, zll) &&
            tid == 0x12345678 && zb == 0x12 && zll == 0x33,
        "touchlink Scan Request recovered from the inter-PAN payload");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee inter-PAN self-test ===");

  testInterPanFrame();
  testTouchlinkOverInterPan();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
