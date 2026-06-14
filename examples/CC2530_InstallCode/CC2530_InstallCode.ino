/*
  CC2530_InstallCode - Zigbee install-code -> link key self-test.

  A Zigbee 3.0 device can be commissioned with a per-device install code (a
  printed secret + 16-bit CRC) instead of the global TC link key. The Trust
  Center derives the device's unique link key from the install code (AES-MMO-128
  hash of the full code) and uses it to APS-encrypt the network key. This sketch
  self-tests the CRC, code validation/build, and link-key derivation.

  The CRC implementation is checked against the standard CRC-16/X-25 vector for
  "123456789" (0x906E); the derivation against the raw AES-MMO hash. No radio
  traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testCrc() {
  Serial.println("CRC-16/X-25:");
  const uint8_t check9[9] = {'1','2','3','4','5','6','7','8','9'};
  check(ZigbeeInstallCode::crc16(check9, 9) == 0x906E,
        "\"123456789\" -> 0x906E (standard X-25 check)");
}

void testValidateAndBuild() {
  Serial.println("Install-code build + validate:");
  // A 16-byte install-code body; build() appends the correct 2-byte CRC.
  uint8_t body[16];
  for (uint8_t i = 0; i < 16; ++i) body[i] = (uint8_t)(0x11 * (i + 1));
  uint8_t code[18];
  uint8_t n = ZigbeeInstallCode::build(body, 16, code, sizeof(code));
  check(n == 18, "18-byte code (16 body + 2 CRC)");
  check(ZigbeeInstallCode::validate(code, 18), "built code passes CRC validation");

  // A flipped bit fails the CRC.
  uint8_t bad[18];
  memcpy(bad, code, 18);
  bad[5] ^= 0x01;
  check(!ZigbeeInstallCode::validate(bad, 18), "corrupted code fails validation");

  // A short 6-byte body -> 8-byte code is also valid.
  uint8_t body6[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34};
  uint8_t code8[8];
  uint8_t m = ZigbeeInstallCode::build(body6, 6, code8, sizeof(code8));
  check(m == 8 && ZigbeeInstallCode::validate(code8, 8), "8-byte install code round-trip");

  // An invalid length is rejected.
  check(!ZigbeeInstallCode::validate(code, 9), "odd/invalid length rejected");
}

static bool equal16(const uint8_t* a, const uint8_t* b) {
  for (uint8_t i = 0; i < 16; ++i) if (a[i] != b[i]) return false;
  return true;
}

void testDeriveLinkKey() {
  Serial.println("Link-key derivation:");
  uint8_t body[16];
  for (uint8_t i = 0; i < 16; ++i) body[i] = (uint8_t)(0xA0 + i);
  uint8_t code[18];
  ZigbeeInstallCode::build(body, 16, code, sizeof(code));

  uint8_t key[16], key2[16];
  check(ZigbeeInstallCode::deriveLinkKey(code, 18, key), "derive link key");
  check(ZigbeeInstallCode::deriveLinkKey(code, 18, key2) && equal16(key, key2),
        "derivation is deterministic");

  // The link key is the AES-MMO hash of the whole code (data + CRC).
  uint8_t expect[16];
  ZigbeeApsSecurity::aesMmoHash(code, 18, expect);
  check(equal16(key, expect), "link key == AES-MMO(install code)");

  // A different install code yields a different key.
  uint8_t body2[16];
  memcpy(body2, body, 16); body2[0] ^= 0x01;
  uint8_t code2[18], keyB[16];
  ZigbeeInstallCode::build(body2, 16, code2, sizeof(code2));
  ZigbeeInstallCode::deriveLinkKey(code2, 18, keyB);
  check(!equal16(key, keyB), "different install code -> different link key");

  // Deriving from a CRC-invalid code fails.
  uint8_t bad[18];
  memcpy(bad, code, 18); bad[17] ^= 0xFF;
  check(!ZigbeeInstallCode::deriveLinkKey(bad, 18, keyB),
        "derive rejects a bad-CRC code");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee install-code self-test ===");

  testCrc();
  testValidateAndBuild();
  testDeriveLinkKey();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
