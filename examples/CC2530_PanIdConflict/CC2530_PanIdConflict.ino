/*
  CC2530_PanIdConflict - NWK PAN ID conflict detection + resolution self-test.

  Two co-located networks can share a 16-bit PAN ID (only the 64-bit extended
  PAN ID is unique). A device that hears the clash reports it to the network
  manager (Network Report, NWK 0x09); the manager picks a new PAN ID and
  announces it network-wide (Network Update, NWK 0x0A) with an incrementing
  update id. This sketch self-tests the conflict test and both command payloads.

  No radio traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testDetection() {
  Serial.println("Conflict detection:");
  const uint64_t ourEpid = 0x1A62195E00000000ULL;
  const uint64_t otherEpid = 0xAABBCCDD00000000ULL;
  // Same short PAN ID, different EPID -> conflict.
  check(ZigbeePanIdConflict::isConflict(0x1A62, ourEpid, 0x1A62, otherEpid),
        "same PAN ID + different EPID = conflict");
  // Same short PAN ID, same EPID -> our own network, not a conflict.
  check(!ZigbeePanIdConflict::isConflict(0x1A62, ourEpid, 0x1A62, ourEpid),
        "same PAN ID + same EPID = our network (no conflict)");
  // Different PAN ID -> no conflict.
  check(!ZigbeePanIdConflict::isConflict(0x1A62, ourEpid, 0x2B73, otherEpid),
        "different PAN ID = no conflict");
}

void testReport() {
  Serial.println("Network Report (PAN ID conflict):");
  NwkNetworkReport r;
  r.reportType = NWK_REPORT_PAN_ID_CONFLICT;
  r.extendedPanId = 0x1A62195E00000000ULL;
  r.count = 2;
  r.panIds[0] = 0x1A62;
  r.panIds[1] = 0x7777;

  uint8_t buf[32];
  uint8_t n = ZigbeePanIdConflict::buildReport(buf, sizeof(buf), r);
  check(n == 1 + 8 + 2 * 2, "report length = options + EPID + 2 PAN IDs (13)");
  check((buf[0] >> 5) == NWK_REPORT_PAN_ID_CONFLICT && (buf[0] & 0x1F) == 2,
        "options byte: type + count");

  NwkNetworkReport parsed;
  check(ZigbeePanIdConflict::parseReport(buf, n, parsed), "parse report");
  check(parsed.reportType == NWK_REPORT_PAN_ID_CONFLICT &&
            parsed.extendedPanId == r.extendedPanId && parsed.count == 2,
        "round-trip type + EPID + count");
  check(parsed.panIds[0] == 0x1A62 && parsed.panIds[1] == 0x7777,
        "conflicting PAN IDs round-trip");

  // A truncated report (claims 2 PAN IDs but is short) must be rejected.
  check(!ZigbeePanIdConflict::parseReport(buf, 10, parsed),
        "truncated report rejected");
}

void testUpdate() {
  Serial.println("Network Update (PAN ID update):");
  NwkNetworkUpdate u;
  u.updateType = NWK_UPDATE_PAN_ID;
  u.extendedPanId = 0x1A62195E00000000ULL;
  u.updateId = 5;
  u.newPanId = 0x3C4D;

  uint8_t buf[16];
  uint8_t n = ZigbeePanIdConflict::buildUpdate(buf, sizeof(buf), u);
  check(n == 12, "update length = 12 bytes");

  NwkNetworkUpdate parsed;
  check(ZigbeePanIdConflict::parseUpdate(buf, n, parsed), "parse update");
  check(parsed.updateType == NWK_UPDATE_PAN_ID &&
            parsed.extendedPanId == u.extendedPanId && parsed.updateId == 5 &&
            parsed.newPanId == 0x3C4D,
        "round-trip type + EPID + update id + new PAN ID");

  // Update-id freshness with 8-bit wraparound.
  check(ZigbeePanIdConflict::updateIdIsNewer(6, 5), "6 newer than 5");
  check(!ZigbeePanIdConflict::updateIdIsNewer(5, 5), "5 not newer than 5");
  check(ZigbeePanIdConflict::updateIdIsNewer(2, 250), "2 newer than 250 (wrap)");
  check(!ZigbeePanIdConflict::updateIdIsNewer(250, 2), "250 not newer than 2 (wrap)");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee PAN ID conflict self-test ===");

  testDetection();
  testReport();
  testUpdate();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
