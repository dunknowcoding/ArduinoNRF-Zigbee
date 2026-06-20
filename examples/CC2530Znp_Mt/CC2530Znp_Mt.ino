/*
  CC2530Znp_Mt - self-test for the Z-Stack ZNP MT framing primitives.

  Verifies the hardware-independent parts of the ZNP backend: the MT frame
  encode/decode and the FCS (SOF LEN CMD0 CMD1 DATA FCS, FCS = XOR of LEN..DATA).
  Runs on any board WITHOUT a ZNP module attached - it exercises the static
  helpers in CC2530ZnpRadio, not the UART. (The on-air bring-up test that needs
  real ZNP firmware is CC2530Znp_Info.)
*/
#include <CC2530ZnpRadio.h>

using nzb::CC2530ZnpRadio;

int passes = 0, fails = 0;

void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("CC2530Znp_Mt: Z-Stack MT framing self-test");

  // A SYS_PING SREQ: cmd0 = SREQ|SYS = 0x21, cmd1 = 0x01, no payload.
  const uint8_t cmd0 = CC2530ZnpRadio::kTypeSreq | CC2530ZnpRadio::kSubSys;
  check(cmd0 == 0x21, "cmd0 packing: SREQ|SYS == 0x21");

  uint8_t frame[16];
  uint8_t n = CC2530ZnpRadio::encodeFrame(frame, sizeof(frame), cmd0, 0x01,
                                          nullptr, 0);
  check(n == 5, "encode empty-payload frame is 5 bytes");
  check(frame[0] == CC2530ZnpRadio::kSof, "byte0 = SOF (0xFE)");
  check(frame[1] == 0x00, "LEN = 0");
  check(frame[2] == 0x21 && frame[3] == 0x01, "CMD0/CMD1 in place");
  check(frame[4] == (uint8_t)(0x00 ^ 0x21 ^ 0x01), "FCS of empty frame");

  // A frame with payload: AF_DATA_REQUEST-shaped bytes.
  const uint8_t payload[] = {0x00, 0x00, 0x01, 0x01, 0x06, 0x00, 0x07};
  const uint8_t afCmd0 = CC2530ZnpRadio::kTypeSreq | CC2530ZnpRadio::kSubAf;
  n = CC2530ZnpRadio::encodeFrame(frame, sizeof(frame), afCmd0, 0x01, payload,
                                  sizeof(payload));
  check(n == (uint8_t)(5 + sizeof(payload)), "encode sized frame length");

  uint8_t expFcs = CC2530ZnpRadio::computeFcs((uint8_t)sizeof(payload), afCmd0,
                                              0x01, payload);
  check(frame[4 + sizeof(payload)] == expFcs, "FCS over LEN..DATA");

  // Round-trip decode.
  uint8_t dCmd0 = 0, dCmd1 = 0, dLen = 0;
  const uint8_t* dData = nullptr;
  bool ok = CC2530ZnpRadio::decodeFrame(frame, n, dCmd0, dCmd1, &dData, dLen);
  check(ok && dCmd0 == afCmd0 && dCmd1 == 0x01 && dLen == sizeof(payload),
        "decode round-trips CMD0/CMD1/LEN");
  bool same = ok && dData;
  for (uint8_t i = 0; same && i < sizeof(payload); ++i)
    if (dData[i] != payload[i]) same = false;
  check(same, "decode round-trips the payload bytes");

  // A corrupted FCS must be rejected.
  frame[4 + sizeof(payload)] ^= 0xFF;
  check(!CC2530ZnpRadio::decodeFrame(frame, n, dCmd0, dCmd1, &dData, dLen),
        "decode rejects a bad FCS");
  frame[4 + sizeof(payload)] ^= 0xFF;  // restore

  // A wrong total length must be rejected (LEN says more than buffer holds).
  check(!CC2530ZnpRadio::decodeFrame(frame, (uint8_t)(n - 1), dCmd0, dCmd1,
                                     &dData, dLen),
        "decode rejects a truncated frame");

  // A missing SOF must be rejected.
  uint8_t bad = frame[0];
  frame[0] = 0x00;
  check(!CC2530ZnpRadio::decodeFrame(frame, n, dCmd0, dCmd1, &dData, dLen),
        "decode rejects a missing SOF");
  frame[0] = bad;

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
