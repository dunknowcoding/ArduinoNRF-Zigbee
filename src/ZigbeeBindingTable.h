/*
  ZigbeeBindingTable.h - Zigbee APS source binding table.

  A binding ties a local (source endpoint, cluster) to a remote destination
  (an IEEE address + endpoint, or a group). Once bound, the application sends
  to "whatever is bound on this endpoint/cluster" instead of an explicit
  address - the foundation of Zigbee's bind-then-report / bind-then-control
  model (e.g. a switch bound to a lamp).

  This is the local table plus lookup; the sketch (or a future APSDE helper)
  walks the matching bindings and unicasts/multicasts to each destination.
  ZDO Bind_req / Unbind_req frame tooling lives in ZigbeeZdo.
*/
#ifndef NIUS_ZIGBEE_BINDING_TABLE_H
#define NIUS_ZIGBEE_BINDING_TABLE_H

#include <Arduino.h>

namespace nzb {

enum ZigbeeBindAddrMode : uint8_t {
  ZB_BIND_ADDR_GROUP = 0x01,  // destination is a 16-bit group address
  ZB_BIND_ADDR_IEEE = 0x03,   // destination is a 64-bit IEEE + endpoint
};

struct ZigbeeBinding {
  bool used;
  uint64_t srcIeee;       // source device (usually this node)
  uint8_t srcEndpoint;
  uint16_t clusterId;
  uint8_t dstAddrMode;    // ZB_BIND_ADDR_GROUP / _IEEE
  uint16_t dstGroup;      // valid when dstAddrMode == GROUP
  uint64_t dstIeee;       // valid when dstAddrMode == IEEE
  uint8_t dstEndpoint;    // valid when dstAddrMode == IEEE
};

class ZigbeeBindingTable {
 public:
  ZigbeeBindingTable() : entries_(nullptr), capacity_(0) {}
  ZigbeeBindingTable(ZigbeeBinding* storage, uint8_t capacity)
      : entries_(nullptr), capacity_(0) {
    begin(storage, capacity);
  }

  void begin(ZigbeeBinding* storage, uint8_t capacity) {
    entries_ = storage;
    capacity_ = capacity;
    for (uint8_t i = 0; i < capacity_; ++i) entries_[i] = ZigbeeBinding();
  }

  uint8_t capacity() const { return capacity_; }

  uint8_t count() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < capacity_; ++i)
      if (entries_[i].used) ++n;
    return n;
  }

  const ZigbeeBinding* slot(uint8_t index) const {
    return (entries_ && index < capacity_) ? &entries_[index] : nullptr;
  }

  /** Add a binding (idempotent - an identical binding is not duplicated).
      @return true on success or if already present, false if the table is
      full. */
  bool add(const ZigbeeBinding& b) {
    if (!entries_) return false;
    if (find(b) != nullptr) return true;
    for (uint8_t i = 0; i < capacity_; ++i) {
      if (!entries_[i].used) {
        entries_[i] = b;
        entries_[i].used = true;
        return true;
      }
    }
    return false;
  }

  /** Remove a binding matching src endpoint/cluster + destination. */
  bool remove(const ZigbeeBinding& b) {
    ZigbeeBinding* e = find(b);
    if (!e) return false;
    *e = ZigbeeBinding();
    return true;
  }

  /** Find an exact binding (matches src ep/cluster + destination). */
  ZigbeeBinding* find(const ZigbeeBinding& b) {
    if (!entries_) return nullptr;
    for (uint8_t i = 0; i < capacity_; ++i) {
      ZigbeeBinding& e = entries_[i];
      if (!e.used) continue;
      if (e.srcEndpoint != b.srcEndpoint || e.clusterId != b.clusterId) continue;
      if (e.dstAddrMode != b.dstAddrMode) continue;
      if (b.dstAddrMode == ZB_BIND_ADDR_GROUP) {
        if (e.dstGroup == b.dstGroup) return &e;
      } else {
        if (e.dstIeee == b.dstIeee && e.dstEndpoint == b.dstEndpoint) return &e;
      }
    }
    return nullptr;
  }

  /** Iterate bindings for a given source endpoint + cluster. Pass a 0 cursor
      to start; returns the next matching binding and advances the cursor, or
      nullptr when done. Lets a sender deliver to every bound destination. */
  const ZigbeeBinding* next(uint8_t srcEndpoint, uint16_t clusterId,
                            uint8_t& cursor) const {
    if (!entries_) return nullptr;
    while (cursor < capacity_) {
      const ZigbeeBinding& e = entries_[cursor++];
      if (e.used && e.srcEndpoint == srcEndpoint && e.clusterId == clusterId) {
        return &e;
      }
    }
    return nullptr;
  }

 private:
  ZigbeeBinding* entries_;
  uint8_t capacity_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_BINDING_TABLE_H
