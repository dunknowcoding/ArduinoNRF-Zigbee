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
enum NwkUpdateType : uint8_t { NWK_UPDATE_PAN_ID = 0x00, NWK_UPDATE_CHANNEL = 0x01 };

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
  uint16_t newPanId;         // the PAN ID every node must adopt (PAN ID update)
  uint8_t newChannel;        // the channel every node must adopt (channel update)

  NwkNetworkUpdate()
      : updateType(0), extendedPanId(0), updateId(0), newPanId(0),
        newChannel(0) {}
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

  // ----------------------------------------- Network Update (0x0A), channel
  /** Build a Network Update "channel change": options(1) [type<<5 | 1] +
      EPID(8) + update id(1) + channel(1) = 11 bytes. The whole network moves to
      @p newChannel together, tracked by the incrementing update id. */
  static uint8_t buildChannelUpdate(uint8_t* out, uint8_t outMax,
                                    const NwkNetworkUpdate& u) {
    if (!out || outMax < 11) return 0;
    out[0] = (uint8_t)(((NWK_UPDATE_CHANNEL & 0x07) << 5) | 0x01);
    putLe64(&out[1], u.extendedPanId);
    out[9] = u.updateId;
    out[10] = u.newChannel;
    return 11;
  }

  static bool parseChannelUpdate(const uint8_t* payload, uint8_t len,
                                 NwkNetworkUpdate& u) {
    u = NwkNetworkUpdate();
    if (!payload || len < 11) return false;
    u.updateType = (uint8_t)((payload[0] >> 5) & 0x07);
    if (u.updateType != NWK_UPDATE_CHANNEL) return false;
    u.extendedPanId = getLe64(&payload[1]);
    u.updateId = payload[9];
    u.newChannel = payload[10];
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

/*
  ZigbeeNetworkManager - the stateful driver that turns the PAN-ID-conflict /
  network-update frames into real behavior: detect a conflict from heard beacons,
  let the network manager pick a fresh PAN ID, announce a PAN-ID or channel change
  with an incrementing update id, and apply an incoming change (freshness-checked,
  EPID-scoped) to the local network identity.
*/
class ZigbeeNetworkManager {
 public:
  static const uint8_t kMaxHeardPanIds = 16;

  void begin(uint16_t panId, uint64_t extPanId, uint8_t channel,
             uint8_t nwkUpdateId = 0) {
    panId_ = panId;
    extPanId_ = extPanId;
    channel_ = channel;
    updateId_ = nwkUpdateId;
    heardCount_ = 0;
  }

  uint16_t panId() const { return panId_; }
  uint64_t extPanId() const { return extPanId_; }
  uint8_t channel() const { return channel_; }
  uint8_t nwkUpdateId() const { return updateId_; }

  /** Note a heard beacon. Records its PAN ID (for later new-PAN selection) and
      returns true if it conflicts with ours (same PAN ID, different EPID). */
  bool noteBeacon(uint16_t heardPanId, uint64_t heardExtPanId) {
    recordHeardPanId(heardPanId);
    return ZigbeePanIdConflict::isConflict(panId_, extPanId_, heardPanId,
                                           heardExtPanId);
  }

  /** Pick a fresh 16-bit PAN ID that is not our current one and not any PAN ID
      we have heard. @p seed lets a caller vary the search start (e.g. from a
      random source). Avoids 0x0000 and the 0xFFFF broadcast. */
  uint16_t selectNewPanId(uint16_t seed = 0x1A62) const {
    for (uint16_t i = 0; i < 0xFFFE; ++i) {
      uint16_t cand = (uint16_t)(seed + i);
      if (cand == 0x0000 || cand == 0xFFFF) continue;
      if (cand == panId_) continue;
      if (!isHeard(cand)) return cand;
    }
    return panId_;  // nothing free (pathological)
  }

  /** Build a PAN ID change Network Update for @p newPanId, bumping the update id.
      Does not change our state yet (the manager keeps serving on the old PAN
      until the change takes effect); call applyUpdate() with the same frame, or
      commitPanId(newPanId) after the announcement window. */
  uint8_t buildPanIdChange(uint8_t* out, uint8_t outMax, uint16_t newPanId) {
    NwkNetworkUpdate u;
    u.updateType = NWK_UPDATE_PAN_ID;
    u.extendedPanId = extPanId_;
    u.updateId = (uint8_t)(updateId_ + 1);
    u.newPanId = newPanId;
    return ZigbeePanIdConflict::buildUpdate(out, outMax, u);
  }

  /** Build a channel change Network Update for @p newChannel, bumping update id. */
  uint8_t buildChannelChange(uint8_t* out, uint8_t outMax, uint8_t newChannel) {
    NwkNetworkUpdate u;
    u.updateType = NWK_UPDATE_CHANNEL;
    u.extendedPanId = extPanId_;
    u.updateId = (uint8_t)(updateId_ + 1);
    u.newChannel = newChannel;
    return ZigbeePanIdConflict::buildChannelUpdate(out, outMax, u);
  }

  /** Apply a received Network Update (PAN ID or channel) if it is for our network
      (EPID match) and fresher than what we have (update-id half-window test).
      Adopts the new PAN ID / channel and the new update id. @return true if the
      change was applied. */
  bool applyUpdate(const NwkNetworkUpdate& u) {
    if (u.extendedPanId != extPanId_) return false;
    if (!ZigbeePanIdConflict::updateIdIsNewer(u.updateId, updateId_)) return false;
    if (u.updateType == NWK_UPDATE_PAN_ID) {
      panId_ = u.newPanId;
    } else if (u.updateType == NWK_UPDATE_CHANNEL) {
      channel_ = u.newChannel;
    } else {
      return false;
    }
    updateId_ = u.updateId;
    return true;
  }

  /** Manager-side commit of a PAN ID change it announced (bumps the update id to
      match the announced one). */
  void commitPanId(uint16_t newPanId) {
    panId_ = newPanId;
    ++updateId_;
  }
  void commitChannel(uint8_t newChannel) {
    channel_ = newChannel;
    ++updateId_;
  }

 private:
  void recordHeardPanId(uint16_t panId) {
    if (isHeard(panId)) return;
    if (heardCount_ < kMaxHeardPanIds) {
      heard_[heardCount_++] = panId;
    } else {
      heard_[evict_] = panId;  // ring-recycle when full
      evict_ = (uint8_t)((evict_ + 1) % kMaxHeardPanIds);
    }
  }
  bool isHeard(uint16_t panId) const {
    for (uint8_t i = 0; i < heardCount_; ++i)
      if (heard_[i] == panId) return true;
    return false;
  }

  uint16_t panId_ = 0;
  uint64_t extPanId_ = 0;
  uint8_t channel_ = 11;
  uint8_t updateId_ = 0;
  uint16_t heard_[kMaxHeardPanIds] = {0};
  uint8_t heardCount_ = 0;
  uint8_t evict_ = 0;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_PAN_ID_CONFLICT_H
