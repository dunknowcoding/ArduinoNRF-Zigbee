/*
  ZigbeeSourceRouteTable.h - concentrator-side source route store (Zigbee PRO
  many-to-one routing).

  Zigbee PRO's many-to-one routing avoids one route discovery per device pair.
  A concentrator (typically the coordinator) periodically broadcasts a
  many-to-one route request (NWK Route Request with the many-to-one option, to
  0xFFFC); every router installs a cheap "route toward the concentrator" and,
  when it has upstream data, sends a Route Record (NWK command 0x05) that
  accumulates the relay path it travelled. The concentrator stores that path so
  it can SOURCE-ROUTE replies back down the same hops without discovering a
  route of its own.

  This is that store: install a path from a received Route Record (relays are
  listed in travel order, device -> ... -> concentrator), look it up to build a
  downstream source route (reversed: concentrator -> ... -> device), and age out
  stale paths. Header-only, no radio I/O. Building the NWK source-route subframe
  and forwarding along it is the integration step.
*/
#ifndef NIUS_ZIGBEE_SOURCE_ROUTE_TABLE_H
#define NIUS_ZIGBEE_SOURCE_ROUTE_TABLE_H

#include <Arduino.h>

namespace nzb {

struct SourceRouteEntry {
  bool used;
  uint16_t destAddr;       // the device this path reaches
  uint8_t relayCount;      // number of intermediate relays (0 = direct neighbor)
  static const uint8_t kMaxRelays = 8;
  uint16_t relays[kMaxRelays];  // stored device->concentrator order (as received)
  uint32_t updatedMs;

  SourceRouteEntry() : used(false), destAddr(0xFFFF), relayCount(0), updatedMs(0) {}
};

class ZigbeeSourceRouteTable {
 public:
  // A many-to-one route / source route is refreshed by periodic Route Records;
  // drop one not refreshed within this window by default.
  static const uint32_t kDefaultMaxAgeMs = 60000;

  ZigbeeSourceRouteTable() : entries_(nullptr), capacity_(0) {}
  ZigbeeSourceRouteTable(SourceRouteEntry* storage, uint8_t capacity) {
    begin(storage, capacity);
  }
  void begin(SourceRouteEntry* storage, uint8_t capacity) {
    entries_ = storage;
    capacity_ = capacity;
    for (uint8_t i = 0; i < capacity_; ++i) entries_[i] = SourceRouteEntry();
  }

  uint8_t capacity() const { return capacity_; }
  SourceRouteEntry* slot(uint8_t i) {
    return (entries_ && i < capacity_) ? &entries_[i] : nullptr;
  }

  /** Install/refresh the source route to @p destAddr from a received Route
      Record. @p relays is the relay list in travel order (device ->
      concentrator), exactly as the Route Record carries it. Re-installing the
      same device replaces its path and refreshes the timer. @return false if
      the path is too long or the table is full. */
  bool install(uint16_t destAddr, const uint16_t* relays, uint8_t relayCount,
               uint32_t nowMs) {
    if (!entries_ || relayCount > SourceRouteEntry::kMaxRelays) return false;
    if (relayCount > 0 && !relays) return false;
    SourceRouteEntry* e = find(destAddr);
    if (!e) e = firstFree();
    if (!e) e = oldest();
    if (!e) return false;
    e->used = true;
    e->destAddr = destAddr;
    e->relayCount = relayCount;
    for (uint8_t i = 0; i < relayCount; ++i) e->relays[i] = relays[i];
    e->updatedMs = nowMs;
    return true;
  }

  bool has(uint16_t destAddr) { return find(destAddr) != nullptr; }
  const SourceRouteEntry* lookup(uint16_t destAddr) { return find(destAddr); }

  /** Fill @p out with the downstream source route to @p destAddr - the relay
      list REVERSED into concentrator -> ... -> device order, which is what the
      NWK source-route subframe carries. @return the relay count written, or
      255 if there is no stored route (distinct from a 0-relay direct route). */
  uint8_t downstreamRoute(uint16_t destAddr, uint16_t* out, uint8_t outMax) {
    SourceRouteEntry* e = find(destAddr);
    if (!e) return 255;
    if (outMax < e->relayCount) return 255;
    for (uint8_t i = 0; i < e->relayCount; ++i)
      out[i] = e->relays[e->relayCount - 1 - i];
    return e->relayCount;
  }

  bool remove(uint16_t destAddr) {
    SourceRouteEntry* e = find(destAddr);
    if (!e) return false;
    *e = SourceRouteEntry();
    return true;
  }

  /** Drop paths not refreshed within @p maxAgeMs. @return how many. */
  uint8_t expire(uint32_t nowMs, uint32_t maxAgeMs = kDefaultMaxAgeMs) {
    if (!entries_) return 0;
    uint8_t freed = 0;
    for (uint8_t i = 0; i < capacity_; ++i) {
      if (entries_[i].used &&
          (uint32_t)(nowMs - entries_[i].updatedMs) >= maxAgeMs) {
        entries_[i] = SourceRouteEntry();
        ++freed;
      }
    }
    return freed;
  }

  uint8_t activeCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < capacity_; ++i)
      if (entries_[i].used) ++n;
    return n;
  }

 private:
  SourceRouteEntry* find(uint16_t destAddr) {
    if (!entries_) return nullptr;
    for (uint8_t i = 0; i < capacity_; ++i)
      if (entries_[i].used && entries_[i].destAddr == destAddr)
        return &entries_[i];
    return nullptr;
  }
  SourceRouteEntry* firstFree() {
    for (uint8_t i = 0; i < capacity_; ++i)
      if (!entries_[i].used) return &entries_[i];
    return nullptr;
  }
  SourceRouteEntry* oldest() {
    SourceRouteEntry* o = nullptr;
    for (uint8_t i = 0; i < capacity_; ++i) {
      if (!entries_[i].used) continue;
      if (!o || (int32_t)(entries_[i].updatedMs - o->updatedMs) < 0) o = &entries_[i];
    }
    return o;
  }

  SourceRouteEntry* entries_;
  uint8_t capacity_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_SOURCE_ROUTE_TABLE_H
