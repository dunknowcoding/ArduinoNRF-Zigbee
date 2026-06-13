/*
  ZigbeeApsRetransmit.h - APS end-to-end acknowledged delivery for NiusZigbee.

  Zigbee gives a unicast APS data frame an "ack request" bit; the final
  recipient answers with an APS ACK carrying the same APS counter. Unlike the
  CC2530's per-hop MAC auto-ack, this is END TO END across the routed mesh, so
  it recovers a multi-hop round trip that lost a frame on any single hop.

  This class is the sender side: it remembers each acked APDU (a copy, so it
  can be re-sent through the NWK layer unchanged), matches incoming ACKs by
  APS counter + endpoint, and reports which entries are due for retransmit.
  It owns no radio - the sketch resends through its CC2530Radio so routing and
  NWK security apply to every attempt.
*/
#ifndef NIUS_ZIGBEE_APS_RETRANSMIT_H
#define NIUS_ZIGBEE_APS_RETRANSMIT_H

#include <Arduino.h>
#include "ZigbeeAps.h"

namespace nzb {

struct ApsPending {
  bool used;
  uint16_t dstShort;       // final NWK destination of the acked frame
  uint8_t apsCounter;      // value the matching ACK will carry
  uint8_t srcEndpoint;     // our endpoint (ACK's dstEndpoint echoes it)
  uint32_t nextAttemptMs;  // when to (re)transmit next
  uint16_t intervalMs;     // retransmit backoff interval
  uint8_t triesLeft;       // attempts remaining before giving up
  uint8_t apdu[ZigbeeAps::kMaxFrame];
  uint8_t apduLen;
};

struct ApsRetransmitStats {
  uint32_t queued;       // frames registered for acked delivery
  uint32_t delivered;    // ACK matched before running out of tries
  uint32_t retransmits;  // re-sends after the first attempt
  uint32_t dropped;      // gave up (no ACK within the retry budget)
};

class ZigbeeApsRetransmit {
 public:
  ZigbeeApsRetransmit() : entries_(nullptr), capacity_(0), stats_() {}

  void begin(ApsPending* storage, uint8_t capacity) {
    entries_ = storage;
    capacity_ = capacity;
    for (uint8_t i = 0; i < capacity_; ++i) entries_[i] = ApsPending();
    stats_ = ApsRetransmitStats();
  }

  /** Register an APDU just transmitted with the ack-request bit set. Copies
      the APDU so the caller's buffer can be reused. @return slot or nullptr
      if the table is full. The first transmit is the caller's; the table
      schedules retransmits at nowMs + intervalMs. */
  ApsPending* add(uint16_t dstShort, uint8_t apsCounter, uint8_t srcEndpoint,
                  const uint8_t* apdu, uint8_t apduLen, uint8_t maxRetries,
                  uint16_t intervalMs, uint32_t nowMs) {
    if (!entries_ || !apdu || apduLen > ZigbeeAps::kMaxFrame) return nullptr;
    ApsPending* slot = nullptr;
    for (uint8_t i = 0; i < capacity_; ++i) {
      if (!entries_[i].used) { slot = &entries_[i]; break; }
    }
    if (!slot) return nullptr;
    slot->used = true;
    slot->dstShort = dstShort;
    slot->apsCounter = apsCounter;
    slot->srcEndpoint = srcEndpoint;
    slot->nextAttemptMs = nowMs + intervalMs;
    slot->intervalMs = intervalMs;
    slot->triesLeft = maxRetries;
    slot->apduLen = apduLen;
    for (uint8_t i = 0; i < apduLen; ++i) slot->apdu[i] = apdu[i];
    ++stats_.queued;
    return slot;
  }

  /** Match an incoming ACK (its dstEndpoint echoes our srcEndpoint, and its
      counter our apsCounter). Clears the pending entry. @return true if a
      pending frame was confirmed. */
  bool onAck(uint8_t apsCounter, uint8_t ackDstEndpoint) {
    if (!entries_) return false;
    for (uint8_t i = 0; i < capacity_; ++i) {
      ApsPending& e = entries_[i];
      if (e.used && e.apsCounter == apsCounter &&
          e.srcEndpoint == ackDstEndpoint) {
        e = ApsPending();
        ++stats_.delivered;
        return true;
      }
    }
    return false;
  }

  /** Return the next entry whose retransmit time has arrived, or nullptr.
      The caller re-sends entry->apdu through the radio, then calls
      markAttempted(entry, nowMs). Entries out of tries are dropped here. */
  ApsPending* due(uint32_t nowMs) {
    if (!entries_) return nullptr;
    for (uint8_t i = 0; i < capacity_; ++i) {
      ApsPending& e = entries_[i];
      if (!e.used) continue;
      if ((int32_t)(nowMs - e.nextAttemptMs) < 0) continue;
      if (e.triesLeft == 0) {
        e = ApsPending();
        ++stats_.dropped;
        continue;
      }
      return &e;
    }
    return nullptr;
  }

  /** Account a retransmit the caller just performed and reschedule it. */
  void markAttempted(ApsPending* entry, uint32_t nowMs) {
    if (!entry || !entry->used) return;
    if (entry->triesLeft > 0) --entry->triesLeft;
    entry->nextAttemptMs = nowMs + entry->intervalMs;
    ++stats_.retransmits;
  }

  uint8_t pending() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < capacity_; ++i)
      if (entries_[i].used) ++n;
    return n;
  }

  const ApsRetransmitStats& stats() const { return stats_; }

 private:
  ApsPending* entries_;
  uint8_t capacity_;
  ApsRetransmitStats stats_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_APS_RETRANSMIT_H
