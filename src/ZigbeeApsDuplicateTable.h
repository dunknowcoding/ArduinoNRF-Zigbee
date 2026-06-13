/*
  ZigbeeApsDuplicateTable.h - APS receive-side duplicate rejection.

  When end-to-end APS acks are in use, a lost ACK makes the sender retransmit
  a frame the destination already received. The destination must still ACK
  the retransmit (the sender is waiting for it), but it must NOT hand the
  duplicate up to the application a second time. This small table remembers
  the last APS counter seen from each (source short address, source endpoint)
  pair and reports whether an arriving frame is new.

  The APS counter is 8-bit and wraps. We accept any counter different from the
  last one as new (and record it). That rejects the immediate retransmits this
  is meant to catch; a full sliding-window BTT-style check is overkill for a
  single outstanding acked frame per peer.
*/
#ifndef NIUS_ZIGBEE_APS_DUPLICATE_TABLE_H
#define NIUS_ZIGBEE_APS_DUPLICATE_TABLE_H

#include <Arduino.h>

namespace nzb {

struct ApsDupeEntry {
  bool used;
  uint16_t srcShort;
  uint8_t srcEndpoint;
  uint8_t lastCounter;
  uint32_t lastSeenMs;
};

class ZigbeeApsDuplicateTable {
 public:
  ZigbeeApsDuplicateTable() : entries_(nullptr), capacity_(0) {}

  void begin(ApsDupeEntry* storage, uint8_t capacity) {
    entries_ = storage;
    capacity_ = capacity;
    for (uint8_t i = 0; i < capacity_; ++i) entries_[i] = ApsDupeEntry();
  }

  /** Record an arriving APS frame and report whether it is new (not a
      duplicate of the last counter seen from this source+endpoint). A brand
      new peer is always new. The entry is updated either way. */
  bool checkAndRecord(uint16_t srcShort, uint8_t srcEndpoint, uint8_t counter,
                      uint32_t nowMs) {
    if (!entries_) return true;

    ApsDupeEntry* slot = nullptr;
    for (uint8_t i = 0; i < capacity_; ++i) {
      if (entries_[i].used && entries_[i].srcShort == srcShort &&
          entries_[i].srcEndpoint == srcEndpoint) {
        bool isNew = entries_[i].lastCounter != counter;
        entries_[i].lastCounter = counter;
        entries_[i].lastSeenMs = nowMs;
        return isNew;
      }
      if (!slot && !entries_[i].used) slot = &entries_[i];
    }

    // New peer: claim a free slot, or evict the least-recently-seen one.
    if (!slot) {
      slot = &entries_[0];
      for (uint8_t i = 1; i < capacity_; ++i) {
        if (entries_[i].lastSeenMs < slot->lastSeenMs) slot = &entries_[i];
      }
    }
    slot->used = true;
    slot->srcShort = srcShort;
    slot->srcEndpoint = srcEndpoint;
    slot->lastCounter = counter;
    slot->lastSeenMs = nowMs;
    return true;
  }

 private:
  ApsDupeEntry* entries_;
  uint8_t capacity_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_APS_DUPLICATE_TABLE_H
