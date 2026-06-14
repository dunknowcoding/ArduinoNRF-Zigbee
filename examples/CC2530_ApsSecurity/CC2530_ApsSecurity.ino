/*
  CC2530_ApsSecurity - APS-layer key-transport security self-test.

  The network key is handed to a joiner inside an APS Transport-Key command
  that is encrypted at the APS layer under a key derived from the link key
  (the key-transport key). This sketch self-tests the building blocks added in
  ZigbeeApsSecurity: the AES-MMO hash, HMAC-over-MMO, the specialized key
  derivation (key-transport / key-load keys), and the APS CCM* envelope
  (secureCommand / openCommand) - including a full Transport-Key wrap under the
  default Trust Center link key "ZigBeeAlliance09".

  The CCM* core is the same hardware-verified one the NWK layer uses, so the
  envelope tests (round-trip, tamper detection, wrong-key rejection) exercise
  proven crypto. The MMO hash is cross-checked against the raw AES-128 block so
  a padding/XOR/key-order bug would show.

  Runs on board1 via J-Link (no radio traffic needed).
*/

#include <CC2530Radio.h>
#include <NrfCrypto.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

static bool equal16(const uint8_t* a, const uint8_t* b) {
  for (uint8_t i = 0; i < 16; ++i) if (a[i] != b[i]) return false;
  return true;
}

// AES-MMO is cross-checked against the primitive for a message that pads to a
// single block: hash = AES_key=0(paddedBlock) XOR paddedBlock.
void testAesMmo() {
  Serial.println("AES-MMO hash:");

  // Empty message -> single padded block [0x80, 0x00 x13, len_hi, len_lo=0].
  uint8_t got[16];
  check(ZigbeeApsSecurity::aesMmoHash(nullptr, 0, got), "hash empty msg ok");
  uint8_t block[16];
  memset(block, 0, 16);
  block[0] = 0x80;  // padding for a 0-byte message, bit length 0 in last 2
  uint8_t zeroKey[16];
  memset(zeroKey, 0, 16);
  uint8_t enc[16];
  NrfEcb::encrypt(zeroKey, block, enc);
  uint8_t expect[16];
  for (uint8_t i = 0; i < 16; ++i) expect[i] = enc[i] ^ block[i];
  check(equal16(got, expect), "empty-msg MMO == AES(0,pad) XOR pad");

  // 3-byte message pads to one block too: M | 0x80 | 0..0 | 0x00 0x18.
  const uint8_t msg[3] = {0x11, 0x22, 0x33};
  check(ZigbeeApsSecurity::aesMmoHash(msg, 3, got), "hash 3-byte msg ok");
  memset(block, 0, 16);
  block[0] = 0x11; block[1] = 0x22; block[2] = 0x33;
  block[3] = 0x80;
  block[15] = 0x18;  // bit length = 24
  NrfEcb::encrypt(zeroKey, block, enc);
  for (uint8_t i = 0; i < 16; ++i) expect[i] = enc[i] ^ block[i];
  check(equal16(got, expect), "3-byte MMO == AES(0,pad) XOR pad");

  // Determinism + avalanche: same input same hash, one bit flip differs.
  uint8_t a[16], b[16];
  const uint8_t long1[20] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20};
  uint8_t long2[20];
  memcpy(long2, long1, 20); long2[19] ^= 0x01;
  ZigbeeApsSecurity::aesMmoHash(long1, 20, a);
  ZigbeeApsSecurity::aesMmoHash(long1, 20, b);
  check(equal16(a, b), "multi-block deterministic");
  ZigbeeApsSecurity::aesMmoHash(long2, 20, b);
  check(!equal16(a, b), "multi-block 1-bit flip changes hash");
}

void testHmacAndSpecializedKeys() {
  Serial.println("HMAC-MMO + specialized keys:");
  const uint8_t* linkKey = ZigbeeApsKey::defaultTcLinkKey();

  uint8_t kt[16], kt2[16], kl[16];
  check(ZigbeeApsSecurity::deriveKeyTransportKey(linkKey, kt),
        "derive key-transport key");
  check(ZigbeeApsSecurity::deriveKeyTransportKey(linkKey, kt2),
        "derive again");
  check(equal16(kt, kt2), "key-transport derivation deterministic");
  check(ZigbeeApsSecurity::deriveKeyLoadKey(linkKey, kl), "derive key-load key");
  check(!equal16(kt, kl), "key-transport != key-load (input 0x00 vs 0x02)");
  check(!equal16(kt, linkKey), "key-transport != link key");
  check(!equal16(kl, linkKey), "key-load != link key");

  // HMAC is key-dependent: a different link key yields a different transport key.
  uint8_t otherKey[16];
  for (uint8_t i = 0; i < 16; ++i) otherKey[i] = (uint8_t)(linkKey[i] ^ 0xFF);
  uint8_t ktOther[16];
  ZigbeeApsSecurity::deriveKeyTransportKey(otherKey, ktOther);
  check(!equal16(kt, ktOther), "different link key -> different transport key");
}

void testApsEnvelope() {
  Serial.println("APS CCM* envelope (round-trip / tamper / wrong key):");
  const uint8_t* linkKey = ZigbeeApsKey::defaultTcLinkKey();
  uint8_t key[16];
  ZigbeeApsSecurity::deriveKeyTransportKey(linkKey, key);

  // Build a real Transport-Key (network key) command as the plaintext.
  ApsTransportKey t;
  t.keyType = APS_KEY_STANDARD_NETWORK;
  for (uint8_t i = 0; i < 16; ++i) t.key[i] = (uint8_t)(0xC0 + i);
  t.keySeqNumber = 5;
  t.destAddress = 0x1A62195E00000031ULL;
  t.srcAddress = 0x1A62195E00000000ULL;
  uint8_t cmd[40];
  uint8_t cmdLen = ZigbeeApsKey::buildTransportNetworkKey(cmd, sizeof(cmd), t);
  check(cmdLen == 35, "Transport-Key payload is 35 bytes");

  // APS command-frame header that precedes the aux header (AAD prefix): an APS
  // command frame control byte + APS counter (illustrative).
  const uint8_t apsHeader[2] = {0x21, 0x42};  // frameType=cmd, security set; counter
  const uint64_t srcIeee = 0x1A62195E00000000ULL;
  const uint32_t fc = 0x00001234;

  uint8_t secured[80];
  uint8_t securedLen = ZigbeeApsSecurity::secureCommand(
      apsHeader, sizeof(apsHeader), key, APS_SEC_KEY_KEY_TRANSPORT, srcIeee, fc,
      cmd, cmdLen, secured, sizeof(secured));
  check(securedLen == sizeof(apsHeader) + 13 + cmdLen + 4,
        "secured len = header + aux(13) + payload + mic(4)");
  // The ciphertext must differ from the plaintext (it is actually encrypted).
  check(memcmp(secured + sizeof(apsHeader) + 13, cmd, cmdLen) != 0,
        "payload is encrypted (ciphertext != plaintext)");
  // On-air security level is zeroed in the aux control byte.
  check((secured[sizeof(apsHeader)] & 0x07) == 0, "on-air security level zeroed");

  // Round-trip: open recovers the exact plaintext.
  uint8_t plain[40];
  uint32_t gotFc = 0;
  uint64_t gotIeee = 0;
  uint8_t plainLen = ZigbeeApsSecurity::openCommand(
      secured, securedLen, sizeof(apsHeader), key, plain, sizeof(plain),
      &gotFc, &gotIeee);
  check(plainLen == cmdLen, "open returns the original length");
  check(plainLen == cmdLen && memcmp(plain, cmd, cmdLen) == 0,
        "decrypted payload matches original");
  check(gotFc == fc && gotIeee == srcIeee, "aux frame counter + src IEEE recovered");

  // Parse the recovered Transport-Key back out.
  ApsTransportKey parsed;
  check(ZigbeeApsKey::parseTransportNetworkKey(plain, plainLen, parsed) &&
            parsed.keySeqNumber == t.keySeqNumber &&
            parsed.destAddress == t.destAddress,
        "recovered payload parses as the Transport-Key");

  // Tamper detection: flip one ciphertext byte -> MIC must fail.
  uint8_t tampered[80];
  memcpy(tampered, secured, securedLen);
  tampered[sizeof(apsHeader) + 13 + 2] ^= 0x01;
  check(ZigbeeApsSecurity::openCommand(tampered, securedLen, sizeof(apsHeader),
                                       key, plain, sizeof(plain)) == 0,
        "flipped ciphertext byte rejected (MIC)");

  // Tamper the AAD header -> MIC must fail.
  memcpy(tampered, secured, securedLen);
  tampered[0] ^= 0x01;
  check(ZigbeeApsSecurity::openCommand(tampered, securedLen, sizeof(apsHeader),
                                       key, plain, sizeof(plain)) == 0,
        "flipped header (AAD) byte rejected (MIC)");

  // Wrong key -> MIC must fail.
  uint8_t wrongKey[16];
  memcpy(wrongKey, key, 16); wrongKey[0] ^= 0x01;
  check(ZigbeeApsSecurity::openCommand(secured, securedLen, sizeof(apsHeader),
                                       wrongKey, plain, sizeof(plain)) == 0,
        "wrong key rejected (MIC)");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee APS-security self-test ===");

  testAesMmo();
  testHmacAndSpecializedKeys();
  testApsEnvelope();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
