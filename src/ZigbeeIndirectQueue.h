/*
  ZigbeeIndirectQueue.h - parent-side indirect transmission queue (sleepy
  end devices).

  A sleepy (rx-off-when-idle) end device keeps its radio off to save power, so
  its parent cannot deliver a frame the moment it arrives. Instead the parent
  BUFFERS the frame and waits: when the child wakes it sends a MAC Data Request
  (ZigbeeMac::buildDataRequest) polling its parent, the parent's acknowledgement
  carries the frame-pending bit, and the parent then transmits the buffered
  frame. Buffered frames that are never collected expire after
  macTransactionPersistenceTime (~7.68 s).

  This is the parent's queue for that exchange: enqueue a frame for a child,
  answer a poll (pending / fetch), and age out stale entries. It holds the bytes
  to transmit and does no radio I/O - the driver/sketch enqueues, watches for
  Data Requests, and transmits what pending() returns. Setting the pending bit
  in the ack on air needs CC2530 firmware support (or relaxed-timing host
  emulation); hasPending() is the host-side input to that decision.
*/
#ifndef NIUS_ZIGBEE_INDIRECT_QUEUE_H
#define NIUS_ZIGBEE_INDIRECT_QUEUE_H

#include <Arduino.h>

namespace nzb {

struct IndirectEntry {
  bool used;
  uint16_t childShort;    // the sleepy child this frame is buffered for
  uint32_t expiryMs;      // when this transaction times out
  uint8_t length;         // buffered frame length
  static const uint8_t kMaxPayload = 100;
  uint8_t payload[kMaxPayload];

  IndirectEntry() : used(false), childShort(0xFFFF), expiryMs(0), length(0) {}
};

class ZigbeeIndirectQueue {
 public:
  // macTransactionPersistenceTime: 0x01F4 base superframe durations ~= 7.68 s.
  static const uint32_t kDefaultPersistenceMs = 7680;

  ZigbeeIndirectQueue() : entries_(nullptr), capacity_(0) {}
  ZigbeeIndirectQueue(IndirectEntry* storage, uint8_t capacity) {
    begin(storage, capacity);
  }
  void begin(IndirectEntry* storage, uint8_t capacity) {
    entries_ = storage;
    capacity_ = capacity;
    for (uint8_t i = 0; i < capacity_; ++i) entries_[i] = IndirectEntry();
  }

  uint8_t capacity() const { return capacity_; }
  IndirectEntry* slot(uint8_t i) {
    return (entries_ && i < capacity_) ? &entries_[i] : nullptr;
  }

  /** Buffer a frame for a sleepy child until it polls. A device with an entry
      already queued has it REPLACED (a parent keeps the latest pending frame
      per child, refreshing the timeout). @return false only if the table is
      full and no slot can be reused. */
  bool enqueue(uint16_t childShort, const uint8_t* data, uint8_t len,
               uint32_t nowMs, uint32_t persistenceMs = kDefaultPersistenceMs) {
    if (!entries_ || !data || len > IndirectEntry::kMaxPayload) return false;
    IndirectEntry* e = find(childShort);
    if (!e) e = firstFree();
    if (!e) e = oldest(nowMs);
    if (!e) return false;
    e->used = true;
    e->childShort = childShort;
    e->expiryMs = nowMs + persistenceMs;
    e->length = len;
    memcpy(e->payload, data, len);
    return true;
  }

  /** A frame is buffered for @p childShort (the input to the ack pending-bit
      decision when that child's Data Request arrives). */
  bool hasPending(uint16_t childShort) { return find(childShort) != nullptr; }

  /** The frame buffered for @p childShort, for the caller to transmit, or
      nullptr if none. Does NOT remove it; call dequeue() once the transmission
      is confirmed (so a lost frame can be re-polled). */
  IndirectEntry* pending(uint16_t childShort) { return find(childShort); }

  /** Remove a delivered entry. */
  void dequeue(IndirectEntry* e) {
    if (e) *e = IndirectEntry();
  }
  bool dequeue(uint16_t childShort) {
    IndirectEntry* e = find(childShort);
    if (!e) return false;
    *e = IndirectEntry();
    return true;
  }

  /** Drop transactions whose persistence time has elapsed. @return how many. */
  uint8_t expire(uint32_t nowMs) {
    if (!entries_) return 0;
    uint8_t freed = 0;
    for (uint8_t i = 0; i < capacity_; ++i) {
      if (entries_[i].used &&
          (int32_t)(nowMs - entries_[i].expiryMs) >= 0) {
        entries_[i] = IndirectEntry();
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
  IndirectEntry* find(uint16_t childShort) {
    if (!entries_) return nullptr;
    for (uint8_t i = 0; i < capacity_; ++i)
      if (entries_[i].used && entries_[i].childShort == childShort)
        return &entries_[i];
    return nullptr;
  }
  IndirectEntry* firstFree() {
    for (uint8_t i = 0; i < capacity_; ++i)
      if (!entries_[i].used) return &entries_[i];
    return nullptr;
  }
  IndirectEntry* oldest(uint32_t nowMs) {
    IndirectEntry* o = nullptr;
    for (uint8_t i = 0; i < capacity_; ++i) {
      if (!entries_[i].used) continue;
      if (!o || (int32_t)(entries_[i].expiryMs - o->expiryMs) < 0) o = &entries_[i];
    }
    (void)nowMs;
    return o;
  }

  IndirectEntry* entries_;
  uint8_t capacity_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_INDIRECT_QUEUE_H
