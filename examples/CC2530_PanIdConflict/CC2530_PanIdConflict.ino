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

void testChannelUpdate() {
  Serial.println("Network Update (channel change):");
  NwkNetworkUpdate u;
  u.updateType = NWK_UPDATE_CHANNEL;
  u.extendedPanId = 0x1A62195E00000000ULL;
  u.updateId = 7;
  u.newChannel = 20;

  uint8_t buf[16];
  uint8_t n = ZigbeePanIdConflict::buildChannelUpdate(buf, sizeof(buf), u);
  check(n == 11, "channel update length = 11 bytes");

  NwkNetworkUpdate parsed;
  check(ZigbeePanIdConflict::parseChannelUpdate(buf, n, parsed), "parse channel update");
  check(parsed.updateType == NWK_UPDATE_CHANNEL && parsed.updateId == 7 &&
            parsed.newChannel == 20 && parsed.extendedPanId == u.extendedPanId,
        "round-trip type + update id + channel");
}

void testNetworkManager() {
  Serial.println("Network manager (detect from beacon, select, apply):");
  const uint64_t OUR_EPID = 0x1A62195E00000000ULL;
  const uint64_t OTHER_EPID = 0xAABBCCDD00000000ULL;
  ZigbeeNetworkManager mgr;
  mgr.begin(0x1A62, OUR_EPID, /*channel=*/15, /*updateId=*/3);

  // Our own beacon (same EPID) is not a conflict; a foreign one with our PAN ID is.
  check(!mgr.noteBeacon(0x1A62, OUR_EPID), "our own beacon: no conflict");
  check(mgr.noteBeacon(0x1A62, OTHER_EPID), "foreign beacon on our PAN ID: conflict");
  mgr.noteBeacon(0x2222, OTHER_EPID);  // record another heard PAN ID

  // The manager selects a fresh PAN ID that avoids ours and all heard PAN IDs.
  uint16_t fresh = mgr.selectNewPanId(0x1A62);
  check(fresh != 0x1A62 && fresh != 0x2222 && fresh != 0x0000 && fresh != 0xFFFF,
        "selected PAN ID avoids ours, heard ones, and reserved");

  // Announce a PAN ID change; a peer applies it (fresher update id, our EPID).
  uint8_t buf[16];
  uint8_t n = mgr.buildPanIdChange(buf, sizeof(buf), fresh);
  NwkNetworkUpdate u;
  check(ZigbeePanIdConflict::parseUpdate(buf, n, u) && u.updateId == 4 &&
            u.newPanId == fresh,
        "PAN ID change carries bumped update id (3 -> 4)");

  ZigbeeNetworkManager peer;
  peer.begin(0x1A62, OUR_EPID, 15, 3);
  check(peer.applyUpdate(u) && peer.panId() == fresh && peer.nwkUpdateId() == 4,
        "peer adopts the new PAN ID + update id");
  // Replaying the same (now stale) update is ignored.
  check(!peer.applyUpdate(u), "stale update (not newer) ignored");
  // An update for a different network (EPID mismatch) is ignored.
  NwkNetworkUpdate foreign = u; foreign.extendedPanId = OTHER_EPID; foreign.updateId = 9;
  check(!peer.applyUpdate(foreign), "update for a different EPID ignored");

  // The manager commits its own PAN ID change (its update id now matches 4).
  mgr.commitPanId(fresh);
  check(mgr.panId() == fresh && mgr.nwkUpdateId() == 4, "manager commits the PAN ID change");

  // A channel change propagates the same way (next update id, 5).
  uint8_t cn = mgr.buildChannelChange(buf, sizeof(buf), 25);
  NwkNetworkUpdate cu;
  check(ZigbeePanIdConflict::parseChannelUpdate(buf, cn, cu), "parse our channel change");
  check(peer.applyUpdate(cu) && peer.channel() == 25,
        "peer adopts the new channel");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee PAN ID conflict self-test ===");

  testDetection();
  testReport();
  testUpdate();
  testChannelUpdate();
  testNetworkManager();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
