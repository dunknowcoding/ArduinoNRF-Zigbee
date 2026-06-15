/*
  CC2530_Touchlink - Touchlink (ZLL) commissioning self-test.

  Touchlink is Zigbee 3.0 proximity commissioning: an initiator scans
  inter-PAN, a target answers, the initiator makes it identify, then sends a
  Network Join carrying the network key encrypted under a ZLL key. This sketch
  self-tests the commissioning command frames and the ZLL key transport,
  including a full encrypt-on-the-initiator / recover-on-the-target round trip.

  The network-key recovery uses the software AES-128 inverse cipher
  (ZigbeeAes128Decrypt) because the nRF ECB peripheral is encrypt-only; the
  test cross-checks that against the hardware ECB and the FIPS-197 vector.

  No radio traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>
#include <NrfCrypto.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}
static bool eq16(const uint8_t* a, const uint8_t* b) {
  for (uint8_t i = 0; i < 16; ++i) if (a[i] != b[i]) return false;
  return true;
}

void testAesDecrypt() {
  Serial.println("Software AES-128 decrypt:");
  // FIPS-197 known-answer: key 000102..0f, plaintext 00112233..ff ->
  // ciphertext 69c4e0d86a7b0430d8cdb78070b4c55a.
  uint8_t key[16], pt[16];
  for (uint8_t i = 0; i < 16; ++i) { key[i] = i; pt[i] = (uint8_t)(0x00 + i * 0x11); }
  static const uint8_t fips[16] = {0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,
                                   0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
  uint8_t ct[16];
  NrfEcb::encrypt(key, pt, ct);  // hardware encrypt
  check(eq16(ct, fips), "hardware ECB matches FIPS-197 vector");

  uint8_t back[16];
  ZigbeeAes128Decrypt::decryptBlock(key, ct, back);  // software decrypt
  check(eq16(back, pt), "software decrypt recovers the FIPS-197 plaintext");

  // Cross-check on a second random-ish block.
  uint8_t k2[16], p2[16], c2[16], b2[16];
  for (uint8_t i = 0; i < 16; ++i) { k2[i] = (uint8_t)(0xA0 ^ i); p2[i] = (uint8_t)(i * 7 + 3); }
  NrfEcb::encrypt(k2, p2, c2);
  ZigbeeAes128Decrypt::decryptBlock(k2, c2, b2);
  check(eq16(b2, p2), "hw-encrypt -> sw-decrypt round trip");
}

void testCommissioningFrames() {
  Serial.println("Touchlink commissioning frames:");
  uint8_t buf[40];

  uint8_t n = ZigbeeTouchlink::buildScanRequest(buf, sizeof(buf), 0x12345678,
                                                0x12, 0x33);
  uint32_t tid; uint8_t zb, zll;
  check(n == 6 && ZigbeeTouchlink::parseScanRequest(buf, n, tid, zb, zll) &&
            tid == 0x12345678 && zb == 0x12 && zll == 0x33,
        "Scan Request round-trip");

  ZllScanResponse sr;
  sr.transactionId = 0x12345678; sr.rssiCorrection = -5; sr.zigbeeInfo = 0x01;
  sr.zllInfo = 0x12; sr.keyBitmask = 0x0010; sr.responseId = 0xCAFEBABE;
  sr.extendedPanId = 0x1A62195E00000001ULL; sr.nwkUpdateId = 2;
  sr.logicalChannel = 15; sr.panId = 0x1A62; sr.networkAddress = 0x0000;
  sr.numberSubDevices = 1; sr.totalGroupIds = 0;
  n = ZigbeeTouchlink::buildScanResponse(buf, sizeof(buf), sr);
  ZllScanResponse pr;
  check(n == 28 && ZigbeeTouchlink::parseScanResponse(buf, n, pr) &&
            pr.responseId == 0xCAFEBABE && pr.extendedPanId == sr.extendedPanId &&
            pr.logicalChannel == 15 && pr.panId == 0x1A62,
        "Scan Response round-trip (response id, ext PAN, channel)");

  n = ZigbeeTouchlink::buildIdentifyRequest(buf, sizeof(buf), 0x12345678, 3);
  uint16_t dur;
  check(n == 6 && ZigbeeTouchlink::parseIdentifyRequest(buf, n, tid, dur) &&
            dur == 3,
        "Identify Request round-trip");
}

void testKeyTransport() {
  Serial.println("ZLL network-key transport:");
  static const uint8_t masterKey[16] = {0x9F,0x55,0x95,0xF1,0x02,0x57,0xC8,0xA4,
                                        0x69,0xCB,0xF4,0x2B,0xC9,0x3F,0xEE,0x31};
  uint8_t networkKey[16];
  for (uint8_t i = 0; i < 16; ++i) networkKey[i] = (uint8_t)(0xC0 + i);
  uint32_t transId = 0x12345678, respId = 0xCAFEBABE;

  // Initiator encrypts the network key; target recovers it.
  uint8_t enc[16];
  check(ZigbeeTouchlink::encryptNetworkKey(masterKey, transId, respId, networkKey,
                                           enc),
        "initiator encrypts the network key");
  check(!eq16(enc, networkKey), "encrypted key differs from plaintext");

  uint8_t recovered[16];
  check(ZigbeeTouchlink::decryptNetworkKey(masterKey, transId, respId, enc,
                                           recovered),
        "target decrypts the network key");
  check(eq16(recovered, networkKey), "recovered network key == original");

  // A different transaction/response id yields a different transport key, so
  // the same encrypted blob no longer recovers the key (anti-replay across
  // sessions).
  uint8_t wrong[16];
  ZigbeeTouchlink::decryptNetworkKey(masterKey, transId ^ 1, respId, enc, wrong);
  check(!eq16(wrong, networkKey), "wrong transaction id does not recover the key");

  // Put it inside a Network Join Router Request and round-trip the frame.
  ZllNetworkJoinRequest j;
  j.transactionId = transId; j.keyIndex = ZLL_KEY_MASTER;
  memcpy(j.encryptedNetworkKey, enc, 16);
  j.nwkUpdateId = 0; j.logicalChannel = 15; j.panId = 0x1A62;
  j.networkAddress = 0x0005;
  uint8_t buf[40];
  uint8_t n = ZigbeeTouchlink::buildNetworkJoinRouterRequest(buf, sizeof(buf), j);
  ZllNetworkJoinRequest pj;
  check(n == 27 && ZigbeeTouchlink::parseNetworkJoinRouterRequest(buf, n, pj) &&
            eq16(pj.encryptedNetworkKey, enc) && pj.networkAddress == 0x0005 &&
            pj.logicalChannel == 15,
        "Network Join Router Request carries the encrypted key");
  uint8_t fromFrame[16];
  ZigbeeTouchlink::decryptNetworkKey(masterKey, pj.transactionId, respId,
                                     pj.encryptedNetworkKey, fromFrame);
  check(eq16(fromFrame, networkKey), "key recovered from the received join frame");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee Touchlink self-test ===");

  testAesDecrypt();
  testCommissioningFrames();
  testKeyTransport();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
