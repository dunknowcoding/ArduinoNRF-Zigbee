/*
  ZigbeePanIdConflict.h - NWK PAN ID conflict detection + resolution frames.

  Two networks within radio range can end up sharing the same 16-bit PAN ID
  (they are picked at formation and only the 64-bit extended PAN ID is truly
  unique). Zigbee resolves that: a device that hears another network using its
  PAN ID reports it to the network manager with a Network Report (NWK command
  0x09, "PAN ID conflict"), and the manager picks a fresh PAN ID and announces
  it network-wide with a Network Update (0x0A, "PAN ID update") carrying an
  incrementing update id so every node switches together.

  This builds/parses those two command payloads and provides the conflict test
  (same short PAN ID, different extended PAN ID = a real conflict). Driving them
  from heard beacons + applying the new PAN ID is the integration step.
*/
#ifndef NIUS_ZIGBEE_PAN_ID_CONFLICT_H
#define NIUS_ZIGBEE_PAN_ID_CONFLICT_H

#include <Arduino.h>

namespace nzb {

enum NwkReportType : uint8_t { NWK_REPORT_PAN_ID_CONFLICT = 0x00 };
enum NwkUpdateType : uint8_t { NWK_UPDATE_PAN_ID = 0x00 };

struct NwkNetworkReport {
  uint8_t reportType;        // NwkReportType (high 3 bits of the options byte)
  uint64_t extendedPanId;    // our network's EPID
  uint8_t count;             // number of conflicting PAN IDs (0..kMaxPanIds)
  static const uint8_t kMaxPanIds = 8;
  uint16_t panIds[kMaxPanIds];

  NwkNetworkReport() : reportType(0), extendedPanId(0), count(0) {}
};

struct NwkNetworkUpdate {
  uint8_t updateType;        // NwkUpdateType
  uint64_t extendedPanId;    // the network's EPID
  uint8_t updateId;          // nwkUpdateId, incremented each change
  uint16_t newPanId;         // the PAN ID every node must adopt

  NwkNetworkUpdate() : updateType(0), extendedPanId(0), updateId(0), newPanId(0) {}
};

class ZigbeePanIdConflict {
 public:
  /** A real PAN ID conflict: another network advertises our 16-bit PAN ID but
      a different 64-bit extended PAN ID. (Same EPID = our own network.) */
  static bool isConflict(uint16_t ourPanId, uint64_t ourExtPanId,
                         uint16_t heardPanId, uint64_t heardExtPanId) {
    return heardPanId == ourPanId && heardExtPanId != ourExtPanId;
  }

  // ------------------------------------------------------ Network Report (0x09)
  /** Build a Network Report "PAN ID conflict": options(1) [type<<5 | count] +
      EPID(8) + count*PAN ID(2). @return length, or 0 on error. */
  static uint8_t buildReport(uint8_t* out, uint8_t outMax,
                             const NwkNetworkReport& r) {
    if (!out || r.count > NwkNetworkReport::kMaxPanIds) return 0;
    uint8_t need = (uint8_t)(1 + 8 + r.count * 2);
    if (outMax < need) return 0;
    out[0] = (uint8_t)(((r.reportType & 0x07) << 5) | (r.count & 0x1F));
    putLe64(&out[1], r.extendedPanId);
    uint8_t p = 9;
    for (uint8_t i = 0; i < r.count; ++i) { putLe16(&out[p], r.panIds[i]); p += 2; }
    return need;
  }

  static bool parseReport(const uint8_t* payload, uint8_t len,
                          NwkNetworkReport& r) {
    r = NwkNetworkReport();
    if (!payload || len < 9) return false;
    r.reportType = (uint8_t)((payload[0] >> 5) & 0x07);
    r.count = (uint8_t)(payload[0] & 0x1F);
    if (r.count > NwkNetworkReport::kMaxPanIds) return false;
    if (len < (uint8_t)(9 + r.count * 2)) return false;
    r.extendedPanId = getLe64(&payload[1]);
    uint8_t p = 9;
    for (uint8_t i = 0; i < r.count; ++i) { r.panIds[i] = getLe16(&payload[p]); p += 2; }
    return true;
  }

  // ------------------------------------------------------ Network Update (0x0A)
  /** Build a Network Update "PAN ID update": options(1) [type<<5 | 1] +
      EPID(8) + update id(1) + new PAN ID(2) = 12 bytes. */
  static uint8_t buildUpdate(uint8_t* out, uint8_t outMax,
                             const NwkNetworkUpdate& u) {
    if (!out || outMax < 12) return 0;
    out[0] = (uint8_t)(((u.updateType & 0x07) << 5) | 0x01);  // info count = 1
    putLe64(&out[1], u.extendedPanId);
    out[9] = u.updateId;
    putLe16(&out[10], u.newPanId);
    return 12;
  }

  static bool parseUpdate(const uint8_t* payload, uint8_t len,
                          NwkNetworkUpdate& u) {
    u = NwkNetworkUpdate();
    if (!payload || len < 12) return false;
    u.updateType = (uint8_t)((payload[0] >> 5) & 0x07);
    u.extendedPanId = getLe64(&payload[1]);
    u.updateId = payload[9];
    u.newPanId = getLe16(&payload[10]);
    return true;
  }

  /** Update-id comparison with 8-bit wraparound: is @p incoming newer than
      @p current (the standard "(a-b) in 1..127" half-window test)? */
  static bool updateIdIsNewer(uint8_t incoming, uint8_t current) {
    return (uint8_t)(incoming - current) != 0 &&
           (uint8_t)(incoming - current) < 128;
  }

 private:
  static void putLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
  }
  static uint16_t getLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  }
  static void putLe64(uint8_t* p, uint64_t v) {
    for (uint8_t i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (8 * i));
  }
  static uint64_t getLe64(const uint8_t* p) {
    uint64_t v = 0;
    for (uint8_t i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (8 * i);
    return v;
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_PAN_ID_CONFLICT_H
