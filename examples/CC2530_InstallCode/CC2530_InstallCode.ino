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

void testReferenceVector() {
  Serial.println("Reference vector (real Zigbee install code):");
  // Canonical install code: 16 data bytes + a 2-byte little-endian CRC (C3 B5).
  // CRC-16/X-25 of the data is 0xB5C3, stored low byte first per the spec.
  const uint8_t code[18] = {0x83,0xFE,0xD3,0x40,0x7A,0x93,0x97,0x23,
                            0xA5,0xC6,0x39,0xB2,0x69,0x16,0xD5,0x05,
                            0xC3,0xB5};
  check(ZigbeeInstallCode::crc16(code, 16) == 0xB5C3, "CRC value == 0xB5C3");
  check(ZigbeeInstallCode::validate(code, 18),
        "real install code passes (little-endian CRC)");
  const uint8_t expectKey[16] = {0x66,0xB6,0x90,0x09,0x81,0xE1,0xEE,0x3C,
                                 0xA4,0x20,0x6B,0x6B,0x86,0x1C,0x02,0xBB};
  uint8_t key[16];
  check(ZigbeeInstallCode::deriveLinkKey(code, 18, key) && equal16(key, expectKey),
        "derives link key 66B6900981E1EE3CA4206B6B861C02BB");
}

void testTcKeyStoreAndSecureJoin() {
  Serial.println("TC per-device key store + per-joiner secure join:");
  const uint64_t JOINER_IEEE = 0x00124B0001020304ULL;
  const uint64_t OTHER_IEEE = 0x00124B00AABBCCDDULL;
  const uint8_t code[18] = {0x83,0xFE,0xD3,0x40,0x7A,0x93,0x97,0x23,
                            0xA5,0xC6,0x39,0xB2,0x69,0x16,0xD5,0x05,
                            0xC3,0xB5};
  const uint8_t NETWORK_KEY[16] = {0x01,0x03,0x05,0x07,0x09,0x0B,0x0D,0x0F,
                                   0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0D};

  TcLinkKeyEntry store[4];
  ZigbeeTcLinkKeyStore ks;
  ks.begin(store, 4);
  check(ks.provisionInstallCode(JOINER_IEEE, code, 18),
        "TC provisions joiner from its install code");
  check(ks.hasPerDevice(JOINER_IEEE) && !ks.hasPerDevice(OTHER_IEEE),
        "per-device key only for the provisioned joiner");
  // An un-provisioned device falls back to the global "ZigBeeAlliance09" key.
  check(equal16(ks.keyFor(OTHER_IEEE), ZigbeeApsKey::defaultTcLinkKey()),
        "unknown device falls back to the global TC link key");

  // TC wraps the network key in a Transport-Key envelope under the joiner's
  // install-code link key (key-transport key derivation).
  uint8_t linkKey[16];
  ZigbeeInstallCode::deriveLinkKey(code, 18, linkKey);
  check(equal16(ks.keyFor(JOINER_IEEE), linkKey),
        "keyFor(joiner) == its install-code link key");
  uint8_t ktk[16];
  ZigbeeApsSecurity::deriveKeyTransportKey(ks.keyFor(JOINER_IEEE), ktk);

  const uint8_t apsHeader[3] = {0x21, 0x00, 0x05};  // representative APS header
  uint8_t wrapped[64];
  uint8_t wn = ZigbeeApsSecurity::secureCommand(
      apsHeader, sizeof(apsHeader), ktk, APS_SEC_KEY_KEY_TRANSPORT, 0xACE1ULL,
      /*frameCounter=*/7, NETWORK_KEY, 16, wrapped, sizeof(wrapped));
  check(wn > 0, "TC wraps the Transport-Key");

  // Joiner holding the SAME install code derives the same key and unwraps it.
  uint8_t jLink[16], jKtk[16];
  ZigbeeInstallCode::deriveLinkKey(code, 18, jLink);
  ZigbeeApsSecurity::deriveKeyTransportKey(jLink, jKtk);
  uint8_t recovered[32];
  uint8_t rn = ZigbeeApsSecurity::openCommand(wrapped, wn, sizeof(apsHeader),
                                              jKtk, recovered, sizeof(recovered));
  check(rn == 16 && equal16(recovered, NETWORK_KEY),
        "joiner recovers the network key");

  // A joiner with the WRONG install code cannot open it.
  uint8_t badCode[18];
  memcpy(badCode, code, 18); badCode[0] ^= 0x01;
  // Fix the CRC so it validates but is a different code -> different key.
  uint16_t bc = ZigbeeInstallCode::crc16(badCode, 16);
  badCode[16] = (uint8_t)(bc & 0xFF); badCode[17] = (uint8_t)(bc >> 8);
  uint8_t bLink[16], bKtk[16];
  ZigbeeInstallCode::deriveLinkKey(badCode, 18, bLink);
  ZigbeeApsSecurity::deriveKeyTransportKey(bLink, bKtk);
  uint8_t bad[32];
  check(ZigbeeApsSecurity::openCommand(wrapped, wn, sizeof(apsHeader), bKtk, bad,
                                       sizeof(bad)) == 0,
        "wrong install code is rejected (MIC fail)");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee install-code self-test ===");

  testCrc();
  testValidateAndBuild();
  testDeriveLinkKey();
  testReferenceVector();
  testTcKeyStoreAndSecureJoin();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() {
  // Re-emit the result so a serial monitor attached after boot still sees it.
  static uint32_t last = 0;
  if (millis() - last > 2000) {
    last = millis();
    Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
    Serial.print(fails); Serial.println(" failed");
  }
}
