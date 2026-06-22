/*
  CC2530_Speke - self-test for NiusZigbee's R23-style Dynamic Link Key (SPEKE over
  Curve25519, ZigbeeSpeke). Pure host computation - no radio involved.

  Verifies the password-authenticated key exchange's internal consistency:
    1. mutual agreement - both parties derive the same link key from the same secret;
    2. key confirmation  - the confirmation tags verify;
    3. mismatch rejection - a wrong password yields a different key (no agreement),
       which is the property that lets a TC reject a joiner with the wrong code.

  Built on NiusZigbee's own crypto (X25519 + AES-MMO), no external library.
  Prints "N/N PASS".
*/
#include <ZigbeeSpeke.h>
using nzb::ZigbeeSpeke;

// Fixed ephemeral scalars so the test is deterministic (real use draws these from
// the nRF TRNG; X25519 clamps them).
static const uint8_t aliceScalar[32] = {
  0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,0x01,
  0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x20};
static const uint8_t bobScalar[32] = {
  0xa0,0xb1,0xc2,0xd3,0xe4,0xf5,0x06,0x17,0x28,0x39,0x4a,0x5b,0x6c,0x7d,0x8e,0x9f,
  0xf0,0xe1,0xd2,0xc3,0xb4,0xa5,0x96,0x87,0x78,0x69,0x5a,0x4b,0x3c,0x2d,0x1e,0x0f};

static const uint8_t pw[]  = {'N','i','u','s','-','I','C','-','0','1','2','3','4','5','6','7'};
static const uint8_t pw2[] = {'N','i','u','s','-','I','C','-','9','9','9','9','9','9','9','9'};

static int passes = 0, total = 0;
static bool eq16(const uint8_t* a, const uint8_t* b) {
  for (int i = 0; i < 16; i++) if (a[i] != b[i]) return false; return true;
}
static void check(const char* name, bool ok) {
  ++total; if (ok) ++passes;
  Serial.print(ok ? "  PASS " : "  FAIL "); Serial.println(name);
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis(); while (!Serial && millis() - t0 < 3000) {}
  Serial.println("SPEKE / R23 dynamic link key self-test");

  // --- correct case: both use the same password ---
  uint8_t Xa[32], Xb[32], Ka[16], Kb[16];
  ZigbeeSpeke::publicShare(pw, sizeof(pw), aliceScalar, Xa);
  ZigbeeSpeke::publicShare(pw, sizeof(pw), bobScalar, Xb);
  ZigbeeSpeke::linkKey(aliceScalar, Xb, Ka);   // Alice: a * Xb
  ZigbeeSpeke::linkKey(bobScalar, Xa, Kb);     // Bob:   b * Xa
  check("mutual agreement (same password)", eq16(Ka, Kb));

  // key confirmation: initiator(Alice) tag verified by responder(Bob) and vice versa
  uint8_t tA[16], tA_chk[16], tB[16], tB_chk[16];
  ZigbeeSpeke::confirmTag(Ka, 1, Xa, Xb, tA);       // Alice sends (role 1)
  ZigbeeSpeke::confirmTag(Kb, 1, Xa, Xb, tA_chk);   // Bob recomputes
  ZigbeeSpeke::confirmTag(Kb, 2, Xa, Xb, tB);       // Bob sends (role 2)
  ZigbeeSpeke::confirmTag(Ka, 2, Xa, Xb, tB_chk);   // Alice recomputes
  check("key confirmation tags verify", eq16(tA, tA_chk) && eq16(tB, tB_chk));

  // --- wrong password: Bob uses a different code ---
  uint8_t Xb_wrong[32], Ka_w[16], Kb_w[16];
  ZigbeeSpeke::publicShare(pw2, sizeof(pw2), bobScalar, Xb_wrong);
  ZigbeeSpeke::linkKey(aliceScalar, Xb_wrong, Ka_w);  // Alice (pw)  * Bob(pw2) share
  ZigbeeSpeke::linkKey(bobScalar, Xa, Kb_w);          // Bob   (pw2) * Alice(pw) share
  check("wrong password -> no agreement", !eq16(Ka_w, Kb_w));

  Serial.print(passes); Serial.print("/"); Serial.print(total);
  Serial.println(passes == total ? " PASS" : " FAIL");
}

void loop() {}
