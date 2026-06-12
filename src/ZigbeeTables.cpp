#include "ZigbeeTables.h"

namespace nzb {

ZigbeeNeighborTable::ZigbeeNeighborTable()
    : entries_(nullptr), capacity_(0) {}

ZigbeeNeighborTable::ZigbeeNeighborTable(ZigbeeNeighbor* entries,
                                         uint8_t capacity)
    : entries_(entries), capacity_(capacity) {
  clear();
}

void ZigbeeNeighborTable::begin(ZigbeeNeighbor* entries, uint8_t capacity) {
  entries_ = entries;
  capacity_ = capacity;
  clear();
}

void ZigbeeNeighborTable::clear() {
  if (!entries_) return;
  for (uint8_t i = 0; i < capacity_; ++i) entries_[i] = ZigbeeNeighbor();
}

uint8_t ZigbeeNeighborTable::count() const {
  uint8_t n = 0;
  if (!entries_) return 0;
  for (uint8_t i = 0; i < capacity_; ++i) {
    if (entries_[i].used) ++n;
  }
  return n;
}

ZigbeeNeighbor* ZigbeeNeighborTable::findByNwk(uint16_t nwkAddress) {
  if (!entries_) return nullptr;
  for (uint8_t i = 0; i < capacity_; ++i) {
    if (entries_[i].used && entries_[i].nwkAddress == nwkAddress) return &entries_[i];
  }
  return nullptr;
}

const ZigbeeNeighbor* ZigbeeNeighborTable::findByNwk(uint16_t nwkAddress) const {
  return const_cast<ZigbeeNeighborTable*>(this)->findByNwk(nwkAddress);
}

ZigbeeNeighbor* ZigbeeNeighborTable::findByIeee(uint64_t ieeeAddress) {
  if (!entries_) return nullptr;
  for (uint8_t i = 0; i < capacity_; ++i) {
    if (entries_[i].used && entries_[i].ieeeAddress == ieeeAddress) return &entries_[i];
  }
  return nullptr;
}

const ZigbeeNeighbor* ZigbeeNeighborTable::findByIeee(uint64_t ieeeAddress) const {
  return const_cast<ZigbeeNeighborTable*>(this)->findByIeee(ieeeAddress);
}

ZigbeeNeighbor* ZigbeeNeighborTable::firstFree() {
  if (!entries_) return nullptr;
  for (uint8_t i = 0; i < capacity_; ++i) {
    if (!entries_[i].used) return &entries_[i];
  }
  return nullptr;
}

ZigbeeNeighbor* ZigbeeNeighborTable::leastRecentlySeen() {
  if (!entries_ || capacity_ == 0) return nullptr;
  ZigbeeNeighbor* best = nullptr;
  for (uint8_t i = 0; i < capacity_; ++i) {
    if (!entries_[i].used) continue;
    if (!best || entries_[i].lastSeenMs < best->lastSeenMs) best = &entries_[i];
  }
  return best;
}

ZigbeeNeighbor* ZigbeeNeighborTable::upsert(
    uint16_t nwkAddress, uint64_t ieeeAddress, uint8_t deviceType,
    uint8_t relationship, uint8_t depth, uint8_t lqi, bool rxOnWhenIdle,
    bool permitJoining, uint32_t nowMs) {
  ZigbeeNeighbor* entry = findByNwk(nwkAddress);
  if (!entry && ieeeAddress != 0) entry = findByIeee(ieeeAddress);
  if (!entry) entry = firstFree();
  if (!entry) entry = leastRecentlySeen();
  if (!entry) return nullptr;

  bool fresh = !entry->used || entry->nwkAddress != nwkAddress;
  entry->used = true;
  entry->nwkAddress = nwkAddress;
  entry->ieeeAddress = ieeeAddress;
  entry->deviceType = deviceType;
  entry->relationship = relationship;
  entry->depth = depth;
  entry->lqi = lqi;
  if (fresh) {
    entry->incomingCost = 0;
    entry->outgoingCost = 0;
  }
  entry->rxOnWhenIdle = rxOnWhenIdle;
  entry->permitJoining = permitJoining;
  entry->lastSeenMs = nowMs;
  return entry;
}

uint8_t ZigbeeNeighborTable::removeStaleRouters(uint32_t cutoffMs,
                                                uint16_t spareAddress) {
  uint8_t removed = 0;
  if (!entries_) return 0;
  for (uint8_t i = 0; i < capacity_; ++i) {
    ZigbeeNeighbor& n = entries_[i];
    if (!n.used || n.nwkAddress == spareAddress) continue;
    if (n.deviceType != ZB_DEVICE_ROUTER &&
        n.deviceType != ZB_DEVICE_COORDINATOR) {
      continue;
    }
    if (n.lastSeenMs < cutoffMs) {
      n = ZigbeeNeighbor();
      ++removed;
    }
  }
  return removed;
}

bool ZigbeeNeighborTable::removeByNwk(uint16_t nwkAddress) {
  ZigbeeNeighbor* entry = findByNwk(nwkAddress);
  if (!entry) return false;
  *entry = ZigbeeNeighbor();
  return true;
}

uint8_t ZigbeeNeighborTable::removeOlderThan(uint32_t cutoffMs) {
  uint8_t removed = 0;
  if (!entries_) return 0;
  for (uint8_t i = 0; i < capacity_; ++i) {
    if (entries_[i].used && entries_[i].lastSeenMs < cutoffMs) {
      entries_[i] = ZigbeeNeighbor();
      ++removed;
    }
  }
  return removed;
}

const ZigbeeNeighbor* ZigbeeNeighborTable::bestParentCandidate() const {
  const ZigbeeNeighbor* best = nullptr;
  if (!entries_) return nullptr;
  for (uint8_t i = 0; i < capacity_; ++i) {
    const ZigbeeNeighbor& e = entries_[i];
    if (!e.used || !e.permitJoining) continue;
    if (e.deviceType != ZB_DEVICE_COORDINATOR && e.deviceType != ZB_DEVICE_ROUTER) {
      continue;
    }
    if (!best || e.lqi > best->lqi ||
        (e.lqi == best->lqi && e.depth < best->depth)) {
      best = &e;
    }
  }
  return best;
}

ZigbeeRouteTable::ZigbeeRouteTable() : entries_(nullptr), capacity_(0) {}

ZigbeeRouteTable::ZigbeeRouteTable(ZigbeeRoute* entries, uint8_t capacity)
    : entries_(entries), capacity_(capacity) {
  clear();
}

void ZigbeeRouteTable::begin(ZigbeeRoute* entries, uint8_t capacity) {
  entries_ = entries;
  capacity_ = capacity;
  clear();
}

void ZigbeeRouteTable::clear() {
  if (!entries_) return;
  for (uint8_t i = 0; i < capacity_; ++i) entries_[i] = ZigbeeRoute();
}

uint8_t ZigbeeRouteTable::count() const {
  uint8_t n = 0;
  if (!entries_) return 0;
  for (uint8_t i = 0; i < capacity_; ++i) {
    if (entries_[i].used) ++n;
  }
  return n;
}

ZigbeeRoute* ZigbeeRouteTable::find(uint16_t dstAddress) {
  if (!entries_) return nullptr;
  for (uint8_t i = 0; i < capacity_; ++i) {
    if (entries_[i].used && entries_[i].dstAddress == dstAddress) return &entries_[i];
  }
  return nullptr;
}

const ZigbeeRoute* ZigbeeRouteTable::find(uint16_t dstAddress) const {
  return const_cast<ZigbeeRouteTable*>(this)->find(dstAddress);
}

ZigbeeRoute* ZigbeeRouteTable::firstFree() {
  if (!entries_) return nullptr;
  for (uint8_t i = 0; i < capacity_; ++i) {
    if (!entries_[i].used) return &entries_[i];
  }
  return nullptr;
}

ZigbeeRoute* ZigbeeRouteTable::leastRecentlyUpdated() {
  if (!entries_ || capacity_ == 0) return nullptr;
  ZigbeeRoute* best = nullptr;
  for (uint8_t i = 0; i < capacity_; ++i) {
    if (!entries_[i].used) continue;
    if (!best || entries_[i].updatedMs < best->updatedMs) best = &entries_[i];
  }
  return best;
}

ZigbeeRoute* ZigbeeRouteTable::upsert(uint16_t dstAddress, uint16_t nextHop,
                                      uint8_t status, bool manyToOne,
                                      bool routeRecordRequired,
                                      uint32_t nowMs) {
  ZigbeeRoute* entry = find(dstAddress);
  if (!entry) entry = firstFree();
  if (!entry) entry = leastRecentlyUpdated();
  if (!entry) return nullptr;

  entry->used = true;
  entry->dstAddress = dstAddress;
  entry->nextHop = nextHop;
  entry->status = status;
  entry->manyToOne = manyToOne;
  entry->routeRecordRequired = routeRecordRequired;
  entry->updatedMs = nowMs;
  return entry;
}

bool ZigbeeRouteTable::remove(uint16_t dstAddress) {
  ZigbeeRoute* entry = find(dstAddress);
  if (!entry) return false;
  *entry = ZigbeeRoute();
  return true;
}

uint8_t ZigbeeRouteTable::removeInactive() {
  uint8_t removed = 0;
  if (!entries_) return 0;
  for (uint8_t i = 0; i < capacity_; ++i) {
    if (entries_[i].used && entries_[i].status != ZB_ROUTE_ACTIVE) {
      entries_[i] = ZigbeeRoute();
      ++removed;
    }
  }
  return removed;
}

}  // namespace nzb
