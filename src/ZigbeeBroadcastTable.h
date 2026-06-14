/*
  ZigbeeBroadcastTable.h - Zigbee NWK Broadcast Transaction Table (BTT).

  A NWK broadcast (RREQ, many-to-one route request, Device_annce, Link Status
  to 0xFFFC, etc.) floods the mesh: every router that hears it rebroadcasts it
  once. Two things keep that flood from melting down, both tracked per
  broadcast transaction = (source address, NWK sequence number):

    1. Deduplication. A router rebroadcasts a given transaction exactly once.
       When it hears the SAME transaction again (a neighbor's copy), it must
       neither reprocess nor rebroadcast it.

    2. Passive acknowledgement. After a router rebroadcasts, it listens: each
       neighbor that rebroadcasts the same transaction is a passive ack that
       the flood reached that neighbor. If too few neighbors are heard
       rebroadcasting within the retry window, the router rebroadcasts again
       (up to a small retry limit) to cover a lost copy.

  This is the data structure for both. It holds no frames and does no radio
  I/O - the sketch/driver records what it sees and acts on due() / find().
  Entries expire after nwkNetworkBroadcastDeliveryTime (~9 s) so the table
  does not fill with stale transactions.
*/
#ifndef NIUS_ZIGBEE_BROADCAST_TABLE_H
#define NIUS_ZIGBEE_BROADCAST_TABLE_H

#include <Arduino.h>

namespace nzb {

struct BroadcastEntry {
  bool used;
  uint16_t source;       // NWK source address of the broadcast
  uint8_t sequence;      // NWK sequence number (the transaction id with source)
  uint32_t firstSeenMs;  // when we first saw this transaction
  uint32_t lastTxMs;     // when we last (re)broadcast it
  uint8_t retries;       // number of (re)broadcasts we have done
  uint8_t passiveAcks;   // neighbors heard rebroadcasting it (passive acks)

  BroadcastEntry()
      : used(false), source(0), sequence(0), firstSeenMs(0), lastTxMs(0),
        retries(0), passiveAcks(0) {}
};

class ZigbeeBroadcastTable {
 public:
  // nwkNetworkBroadcastDeliveryTime is ~9 s; a transaction is meaningless
  // after that and the slot is reclaimed.
  static const uint32_t kDefaultExpiryMs = 9000;

  ZigbeeBroadcastTable() : entries_(nullptr), capacity_(0) {}
  ZigbeeBroadcastTable(BroadcastEntry* storage, uint8_t capacity) {
    begin(storage, capacity);
  }
  void begin(BroadcastEntry* storage, uint8_t capacity) {
    entries_ = storage;
    capacity_ = capacity;
    for (uint8_t i = 0; i < capacity_; ++i) entries_[i] = BroadcastEntry();
  }

  uint8_t capacity() const { return capacity_; }
  BroadcastEntry* slot(uint8_t i) {
    return (entries_ && i < capacity_) ? &entries_[i] : nullptr;
  }

  BroadcastEntry* find(uint16_t source, uint8_t sequence) {
    if (!entries_) return nullptr;
    for (uint8_t i = 0; i < capacity_; ++i) {
      if (entries_[i].used && entries_[i].source == source &&
          entries_[i].sequence == sequence) {
        return &entries_[i];
      }
    }
    return nullptr;
  }

  /** Record a broadcast frame we just received off-air. Returns true if this
      is a NEW transaction (first time seen -> the caller should process and
      rebroadcast it once); returns false if it is a DUPLICATE (already in the
      table -> do NOT reprocess; it is counted as a passive ack instead). */
  bool recordIncoming(uint16_t source, uint8_t sequence, uint32_t nowMs) {
    BroadcastEntry* e = find(source, sequence);
    if (e) {
      // A repeat of a transaction we already hold: a neighbor's rebroadcast,
      // i.e. a passive ack. Not reprocessed.
      if (e->passiveAcks < 0xFF) ++e->passiveAcks;
      return false;
    }
    e = allocate(nowMs);
    if (!e) return true;  // table full: still treat as new (process it)
    e->used = true;
    e->source = source;
    e->sequence = sequence;
    e->firstSeenMs = nowMs;
    e->lastTxMs = nowMs;  // the caller rebroadcasts now; markRebroadcast updates
    e->retries = 0;
    e->passiveAcks = 0;
    return true;
  }

  /** Record a broadcast WE originate, so the same retry/passive-ack tracking
      applies to our own floods. Idempotent: refreshes an existing entry. */
  BroadcastEntry* recordOutgoing(uint16_t source, uint8_t sequence,
                                 uint32_t nowMs) {
    BroadcastEntry* e = find(source, sequence);
    if (!e) {
      e = allocate(nowMs);
      if (!e) return nullptr;
      e->used = true;
      e->source = source;
      e->sequence = sequence;
      e->firstSeenMs = nowMs;
      e->retries = 0;
      e->passiveAcks = 0;
    }
    e->lastTxMs = nowMs;
    return e;
  }

  /** A neighbor was heard rebroadcasting source+sequence: a passive ack.
      Returns true if a matching entry was found. */
  bool markPassiveAck(uint16_t source, uint8_t sequence) {
    BroadcastEntry* e = find(source, sequence);
    if (!e) return false;
    if (e->passiveAcks < 0xFF) ++e->passiveAcks;
    return true;
  }

  /** The next transaction due for a rebroadcast: the retry window has elapsed
      since our last (re)broadcast, we have retries left, and we have heard
      fewer than @p neededAcks neighbors rebroadcast it. Returns nullptr if
      none is due. The caller rebroadcasts it, then calls markRebroadcast(). */
  BroadcastEntry* due(uint32_t nowMs, uint8_t neededAcks, uint32_t retryMs,
                      uint8_t maxRetries) {
    if (!entries_) return nullptr;
    for (uint8_t i = 0; i < capacity_; ++i) {
      BroadcastEntry& e = entries_[i];
      if (!e.used) continue;
      if (e.retries >= maxRetries) continue;
      if (e.passiveAcks >= neededAcks) continue;
      if ((uint32_t)(nowMs - e.lastTxMs) >= retryMs) return &e;
    }
    return nullptr;
  }

  void markRebroadcast(BroadcastEntry* e, uint32_t nowMs) {
    if (!e) return;
    if (e->retries < 0xFF) ++e->retries;
    e->lastTxMs = nowMs;
  }

  /** Reclaim entries older than @p expiryMs (default
      nwkNetworkBroadcastDeliveryTime). Returns the number reclaimed. */
  uint8_t expire(uint32_t nowMs, uint32_t expiryMs = kDefaultExpiryMs) {
    if (!entries_) return 0;
    uint8_t freed = 0;
    for (uint8_t i = 0; i < capacity_; ++i) {
      if (entries_[i].used &&
          (uint32_t)(nowMs - entries_[i].firstSeenMs) >= expiryMs) {
        entries_[i] = BroadcastEntry();
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
  // A free slot, or the oldest entry recycled when the table is full.
  BroadcastEntry* allocate(uint32_t nowMs) {
    if (!entries_) return nullptr;
    for (uint8_t i = 0; i < capacity_; ++i)
      if (!entries_[i].used) return &entries_[i];
    // Full: recycle the entry seen longest ago.
    BroadcastEntry* oldest = &entries_[0];
    for (uint8_t i = 1; i < capacity_; ++i) {
      if ((uint32_t)(nowMs - entries_[i].firstSeenMs) >
          (uint32_t)(nowMs - oldest->firstSeenMs)) {
        oldest = &entries_[i];
      }
    }
    *oldest = BroadcastEntry();
    return oldest;
  }

  BroadcastEntry* entries_;
  uint8_t capacity_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_BROADCAST_TABLE_H
