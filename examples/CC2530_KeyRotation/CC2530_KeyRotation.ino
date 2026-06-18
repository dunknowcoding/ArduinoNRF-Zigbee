/*
  CC2530_KeyRotation - NWK key rotation (Switch-Key) self-test.

  A Trust Center rotates the network key by distributing a new key with a fresh
  sequence number and then broadcasting Switch-Key. While both keys are held,
  ZigbeeSecurity decrypts each frame with the key whose sequence number matches
  its aux header, so traffic secured under either key is accepted across the
  switchover. This sketch self-tests that, plus that single-key behaviour is
  unchanged, plus the post-rekey replay resync: because frame counters restart
  when the key rotates, the replay identity is (source IEEE, key sequence), so a
  low counter under a freshly installed key is accepted instead of being rejected
  as a replay against the old key's high counter. Runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

const uint64_t IEEE = 0x1A62195E00000001ULL;
uint8_t plain[40];
uint8_t plainLen = 0;

void buildPlain() {
  const char* msg = "rotate me";
  plainLen = ZigbeeNwk::buildDataFrame(plain, sizeof(plain), 0x0002, 0x0000, 30,
                                       1, (const uint8_t*)msg, 9);
}

// Secure `plain` with `sec` at `counter`; returns the secured length in out.
uint8_t secure(ZigbeeSecurity& sec, uint32_t counter, uint8_t* out, uint8_t max) {
  return sec.secureNpdu(plain, plainLen, 8, IEEE, counter, out, max);
}
bool opensTo(ZigbeeSecurity& sec, const uint8_t* frame, uint8_t fn) {
  uint8_t out[40];
  uint8_t n = sec.openNpdu(frame, fn, 8, out, sizeof(out));
  if (n != plainLen) return false;
  return memcmp(out, plain, plainLen) == 0;
}

void testSingleKey() {
  Serial.println("Single key (regression - unchanged behavior):");
  uint8_t A[16]; for (uint8_t i = 0; i < 16; ++i) A[i] = (uint8_t)(0xA0 + i);
  ZigbeeSecurity sec;
  sec.setNetworkKey(A, 0);
  uint8_t f[64];
  uint8_t fn = secure(sec, 100, f, sizeof(f));
  check(fn > 0, "secure a NWK frame with the single key");
  sec.resetReplayTable();
  check(opensTo(sec, f, fn), "open recovers the original plaintext");
}

void testRotation() {
  Serial.println("Key rotation across a switch:");
  uint8_t A[16], B[16];
  for (uint8_t i = 0; i < 16; ++i) { A[i] = (uint8_t)(0xA0 + i); B[i] = (uint8_t)(0xB0 + i); }

  ZigbeeSecurity sec;
  sec.setNetworkKey(A, 0);                 // active = A, seq 0
  uint8_t fA[64];
  uint8_t fAn = secure(sec, 100, fA, sizeof(fA));  // secured under A, key seq 0
  check((fA[8 + 13]) == 0, "frame A carries key sequence 0");

  // The TC distributes key B (seq 1) and switches to it.
  sec.setAlternateKey(B, 1);
  check(sec.hasAlternateKey() && sec.alternateKeySequence() == 1,
        "alternate key B (seq 1) held");
  check(sec.switchKey(1) && sec.keySequence() == 1, "Switch-Key -> B is now active");

  uint8_t fB[64];
  uint8_t fBn = secure(sec, 101, fB, sizeof(fB));  // secured under B, key seq 1
  check((fB[8 + 13]) == 1, "frame B carries key sequence 1");

  // After the switch, BOTH frames must still decrypt: B with the active key, A
  // with the alternate (the old key kept for in-flight traffic).
  sec.resetReplayTable();
  check(opensTo(sec, fA, fAn), "old frame (key seq 0) still decrypts via alternate");
  check(opensTo(sec, fB, fBn), "new frame (key seq 1) decrypts via active key");

  // A frame whose key sequence matches neither key is rejected.
  uint8_t fbad[64]; memcpy(fbad, fA, fAn); fbad[8 + 13] = 9;  // unknown key seq
  uint8_t o[40];
  sec.resetReplayTable();
  check(sec.openNpdu(fbad, fAn, 8, o, sizeof(o)) == 0,
        "unknown key sequence rejected");

  // A node that only has the new key cannot read the old-key frame.
  ZigbeeSecurity newOnly;
  newOnly.setNetworkKey(B, 1);
  check(!opensTo(newOnly, fA, fAn), "new-key-only node can't read an old-key frame");
}

void testPostRekeyResync() {
  Serial.println("Post-rekey replay resync (no table reset across the switch):");
  uint8_t A[16], B[16];
  for (uint8_t i = 0; i < 16; ++i) { A[i] = (uint8_t)(0xA0 + i); B[i] = (uint8_t)(0xB0 + i); }

  ZigbeeSecurity sec;
  sec.setNetworkKey(A, 0);
  // The device has been running under key A with a high frame counter.
  uint8_t fA[64];
  uint8_t fAn = secure(sec, 5000, fA, sizeof(fA));
  check(opensTo(sec, fA, fAn), "frame under key A (counter 5000) opens");

  // The TC rotates to key B; the device's frame counter restarts low under the
  // new key sequence. The replay table is NOT reset - exactly the on-air case.
  sec.setAlternateKey(B, 1);
  check(sec.switchKey(1) && sec.keySequence() == 1, "Switch-Key -> B active");
  uint8_t fB[64];
  uint8_t fBn = secure(sec, 1, fB, sizeof(fB));  // low counter under key seq 1

  uint32_t replaysBefore = sec.stats().replays;
  check(opensTo(sec, fB, fBn),
        "low counter under the new key is accepted (per-key-seq resync)");
  check(sec.stats().replays == replaysBefore, "no replay drop across the rekey");

  // A genuine replay under the new key is still rejected.
  check(!opensTo(sec, fB, fBn), "replay of the new-key frame still rejected");
  check(sec.stats().replays == replaysBefore + 1, "the real replay is counted");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee key-rotation self-test ===");

  buildPlain();
  testSingleKey();
  testRotation();
  testPostRekeyResync();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
