/*
  CC2530_ApsDataCrypt - APS-layer application-data encryption self-test.

  Every NiusZigbee frame is already encrypted at the NWK layer with the network
  key. APS-layer security adds a SECOND, end-to-end envelope between two devices
  that share a link key, so a relay (which has the network key) still cannot read
  the application payload. This sketch self-tests
  ZigbeeApsSecurity::secureDataFrame / openDataFrame: it builds an APS data
  frame, encrypts its payload under a link key, confirms the payload is
  ciphertext on the wire, and recovers the original frame - plus tamper and
  wrong-key rejection.

  Reuses the hardware-verified CCM* core (NrfEcb); runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

static const uint8_t LINK_KEY[16] = {0x5A,0x69,0x67,0x42,0x65,0x65,0x41,0x6C,
                                     0x6C,0x69,0x61,0x6E,0x63,0x65,0x30,0x39};
static const uint64_t SRC_IEEE = 0x1A62195E00000001ULL;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee APS app-data encryption self-test ===");

  // A plain APS data frame carrying an application payload.
  const char* msg = "secret";
  uint8_t apdu[32];
  uint8_t apduLen = ZigbeeAps::buildDataFrame(
      apdu, sizeof(apdu), /*dstEp=*/1, ZigbeeZcl::kClusterOnOff,
      ZigbeeAps::kProfileHomeAutomation, /*srcEp=*/1, /*counter=*/7,
      (const uint8_t*)msg, 6);
  check(apduLen == 8 + 6, "plain APS data frame built");

  // Encrypt the payload under the link key (APS-layer end-to-end).
  uint8_t secured[48];
  uint8_t sn = ZigbeeApsSecurity::secureDataFrame(apdu, apduLen, /*headerLen=*/8,
                                                  LINK_KEY, SRC_IEEE,
                                                  /*frameCounter=*/42, secured,
                                                  sizeof(secured));
  check(sn == 8 + 13 + 6 + 4, "secured = header(8) + aux(13) + cipher(6) + MIC(4)");
  check((secured[0] & 0x20) != 0, "APS frame-control security bit set");

  // The cleartext payload must not appear in the secured frame.
  bool leaked = false;
  for (uint8_t i = 0; i + 6 <= sn; ++i) {
    if (memcmp(&secured[i], msg, 6) == 0) leaked = true;
  }
  check(!leaked, "application payload is ciphertext on the wire");

  // The far end recovers the original APS data frame.
  uint8_t recovered[32];
  uint32_t fc = 0;
  uint8_t rn = ZigbeeApsSecurity::openDataFrame(secured, sn, 8, LINK_KEY,
                                                recovered, sizeof(recovered), &fc);
  check(rn == apduLen, "recovered frame length matches the original");
  check(fc == 42, "APS frame counter recovered from the aux header");
  check((recovered[0] & 0x20) == 0, "security bit cleared in the recovered frame");
  ApsDataFrame f;
  check(ZigbeeAps::parseDataFrame(recovered, rn, f) && f.payloadLen == 6 &&
            memcmp(f.payload, msg, 6) == 0 && f.clusterId == ZigbeeZcl::kClusterOnOff,
        "recovered APS frame parses with the original payload + cluster");

  // Tamper -> MIC fails.
  secured[10] ^= 0x01;
  check(ZigbeeApsSecurity::openDataFrame(secured, sn, 8, LINK_KEY, recovered,
                                         sizeof(recovered)) == 0,
        "tampered ciphertext rejected");
  secured[10] ^= 0x01;  // undo

  // Wrong link key -> MIC fails.
  uint8_t wrong[16]; memcpy(wrong, LINK_KEY, 16); wrong[0] ^= 0xFF;
  check(ZigbeeApsSecurity::openDataFrame(secured, sn, 8, wrong, recovered,
                                         sizeof(recovered)) == 0,
        "wrong link key rejected");

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
