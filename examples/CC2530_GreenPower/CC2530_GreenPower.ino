/*
  CC2530_GreenPower - Zigbee Green Power self-test.

  Green Power devices (GPDs) are battery-less switches/sensors that emit Green
  Power Data Frames (GPDFs) - a stub NWK frame with a GPD source id, frame
  counter, command, and an AES-CCM* MIC. A GP sink commissions a GPD (stores its
  key) and then decrypts and acts on its frames. This sketch self-tests the
  GPDF (unsecured + secured), the GP frame security round trip, the
  commissioning command, and the sink table with replay protection.

  The GP security reuses the hardware-verified CCM* core (NrfEcb); runs on
  board1 via J-Link, no radio traffic.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

static const uint8_t GPD_KEY[16] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                                    0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};
static const uint32_t SRC_ID = 0x01234567;

void testUnsecured() {
  Serial.println("Unsecured GPDF (commissioning):");
  GpCommissioningCommand c;
  c.deviceId = 0x02;            // on/off switch
  c.options = 0x20;             // GPD key present
  c.keyPresent = true;
  memcpy(c.key, GPD_KEY, 16);
  c.outgoingCounterPresent = false;
  uint8_t cmd[24];
  uint8_t cn = ZigbeeGreenPower::buildCommissioning(cmd, sizeof(cmd), c);
  check(cn == 2 + 16, "commissioning command = 2 + 16 (key)");

  uint8_t gpdf[40];
  uint8_t n = ZigbeeGreenPower::buildUnsecured(gpdf, sizeof(gpdf), SRC_ID,
                                               GPD_CMD_COMMISSIONING, cmd, cn);
  GpdfFrame f;
  check(ZigbeeGreenPower::parse(gpdf, n, f), "parse unsecured GPDF");
  check(f.srcId == SRC_ID && f.securityLevel == GP_SEC_NONE &&
            f.commandId == GPD_CMD_COMMISSIONING,
        "src id / level 0 / command id");

  GpCommissioningCommand pc;
  check(ZigbeeGreenPower::parseCommissioning(f.payload, f.payloadLen, pc) &&
            pc.deviceId == 0x02 && pc.keyPresent,
        "parsed commissioning carries the GPD key");
  bool keyOk = true;
  for (uint8_t i = 0; i < 16; ++i) if (pc.key[i] != GPD_KEY[i]) keyOk = false;
  check(keyOk, "GPD key round-trip");
}

void testSecured() {
  Serial.println("Secured GPDF (level 3, encrypted command + MIC):");
  const uint8_t payload[2] = {0x55, 0xAA};
  uint8_t gpdf[40];
  uint8_t n = ZigbeeGreenPower::secure(gpdf, sizeof(gpdf), GPD_KEY, SRC_ID,
                                       /*frameCounter=*/100, GPD_CMD_TOGGLE,
                                       payload, 2);
  check(n == 10 + 3 + 4, "secured GPDF = header(10) + enc(3) + MIC(4)");
  // The command must NOT be visible in the clear in the encrypted region.
  check(gpdf[10] != GPD_CMD_TOGGLE, "command id is encrypted on the wire");

  GpdfFrame f;
  check(ZigbeeGreenPower::open(gpdf, n, GPD_KEY, f), "open (verify+decrypt)");
  check(f.commandId == GPD_CMD_TOGGLE && f.frameCounter == 100 &&
            f.payloadLen == 2 && f.payload[0] == 0x55 && f.payload[1] == 0xAA,
        "recovered command / counter / payload");

  // Tamper -> MIC must fail.
  gpdf[11] ^= 0x01;
  GpdfFrame bad;
  check(!ZigbeeGreenPower::open(gpdf, n, GPD_KEY, bad), "tampered frame rejected");

  // Wrong key -> MIC must fail.
  uint8_t wrong[16];
  memcpy(wrong, GPD_KEY, 16); wrong[0] ^= 0xFF;
  gpdf[11] ^= 0x01;  // undo tamper
  check(!ZigbeeGreenPower::open(gpdf, n, wrong, bad), "wrong key rejected");
}

void testSinkTable() {
  Serial.println("GP sink table (commission + replay):");
  GpSinkEntry storage[4];
  ZigbeeGpSinkTable sink;
  sink.begin(storage, 4);

  check(sink.find(SRC_ID) == nullptr, "GPD unknown before commissioning");
  check(sink.commission(SRC_ID, 0x02, GPD_KEY) != nullptr, "commission GPD");
  GpSinkEntry* e = sink.find(SRC_ID);
  check(e && e->deviceId == 0x02, "GPD found after commissioning");

  check(sink.checkAndUpdateCounter(SRC_ID, 100), "first frame counter accepted");
  check(sink.checkAndUpdateCounter(SRC_ID, 101), "advancing counter accepted");
  check(!sink.checkAndUpdateCounter(SRC_ID, 101), "replayed counter rejected");
  check(!sink.checkAndUpdateCounter(SRC_ID, 50), "older counter rejected");
  check(!sink.checkAndUpdateCounter(0xDEADBEEF, 1), "unknown GPD rejected");

  // End-to-end: a secured frame from a commissioned GPD, decrypted with the
  // stored key, then its counter checked against the sink table.
  uint8_t gpdf[24];
  uint8_t n = ZigbeeGreenPower::secure(gpdf, sizeof(gpdf), e->key, SRC_ID, 200,
                                       GPD_CMD_ON, nullptr, 0);
  GpdfFrame f;
  bool ok = ZigbeeGreenPower::open(gpdf, n, e->key, f) &&
            sink.checkAndUpdateCounter(f.srcId, f.frameCounter);
  check(ok && f.commandId == GPD_CMD_ON, "sink decrypts + accepts a live GPD frame");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee Green Power self-test ===");

  testUnsecured();
  testSecured();
  testSinkTable();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
