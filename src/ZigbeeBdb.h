/*
  ZigbeeBdb.h - Base Device Behaviour (BDB) commissioning state machine.

  Zigbee 3.0 defines commissioning as a small set of modes a device runs, in a
  fixed precedence order, when told to commission:

    touchlink (as initiator) -> network steering -> network formation ->
    finding & binding

  The application asks for a bitmask of modes; the BDB state machine runs them
  one at a time in that order, hands each to the host (which performs the actual
  step using the rest of the stack - touchlink scan, join/scan, form a network,
  finding & binding), collects the per-step status, and stops early if a network
  step fails (no point doing finding & binding with no network). This is the
  orchestration layer that ties the individual commissioning pieces together
  into one procedure with one result.
*/
#ifndef NIUS_ZIGBEE_BDB_H
#define NIUS_ZIGBEE_BDB_H

#include <Arduino.h>

namespace nzb {

// Commissioning mode bitmask (bdbCommissioningMode).
enum BdbCommissioningMode : uint8_t {
  BDB_MODE_INITIATOR_TOUCHLINK = 0x01,
  BDB_MODE_NETWORK_STEERING = 0x02,
  BDB_MODE_NETWORK_FORMATION = 0x04,
  BDB_MODE_FINDING_BINDING = 0x08,
};

// Per-step / overall commissioning status (bdbCommissioningStatus).
enum BdbCommissioningStatus : uint8_t {
  BDB_STATUS_SUCCESS = 0,
  BDB_STATUS_IN_PROGRESS = 1,
  BDB_STATUS_NO_NETWORK = 2,
  BDB_STATUS_TL_TARGET_FAILURE = 3,
  BDB_STATUS_TL_NOT_AA_CAPABLE = 4,
  BDB_STATUS_TL_NO_SCAN_RESPONSE = 5,
  BDB_STATUS_NO_IDENTIFY_QUERY_RESPONSE = 7,
  BDB_STATUS_BINDING_TABLE_FULL = 8,
  BDB_STATUS_NO_SCAN_RESPONSE = 9,
  BDB_STATUS_NOT_PERMITTED = 10,
  BDB_STATUS_FORMATION_FAILURE = 12,
};

enum BdbState : uint8_t {
  BDB_IDLE = 0,
  BDB_IN_PROGRESS_STATE = 1,
  BDB_DONE = 2,
};

class ZigbeeBdb {
 public:
  ZigbeeBdb()
      : pending_(0), current_(0), state_(BDB_IDLE),
        lastStatus_(BDB_STATUS_SUCCESS) {}

  /** Begin commissioning with a bitmask of modes. The state machine advances to
      the first applicable mode (in BDB precedence order). @return the mode now
      to execute, or 0 if no modes were requested. */
  uint8_t start(uint8_t modes) {
    pending_ = modes;
    lastStatus_ = BDB_STATUS_SUCCESS;
    state_ = modes ? BDB_IN_PROGRESS_STATE : BDB_DONE;
    advance();
    return current_;
  }

  /** The mode the host should execute now (a single BdbCommissioningMode bit),
      or 0 when commissioning is complete. */
  uint8_t currentMode() const { return current_; }

  /** Report the outcome of the current mode. On success the next requested mode
      runs; a failed NETWORK step (steering/formation) ends commissioning (no
      network to continue on). @return the next mode to execute, or 0 if done. */
  uint8_t reportResult(uint8_t status) {
    lastStatus_ = status;
    uint8_t finished = current_;
    pending_ = (uint8_t)(pending_ & ~finished);  // this mode is done

    bool networkStep = (finished == BDB_MODE_NETWORK_STEERING ||
                        finished == BDB_MODE_NETWORK_FORMATION);
    if (status != BDB_STATUS_SUCCESS && networkStep) {
      // No network -> abandon the rest (e.g. finding & binding).
      pending_ = 0;
      current_ = 0;
      state_ = BDB_DONE;
      return 0;
    }
    advance();
    return current_;
  }

  BdbState state() const { return state_; }
  bool isActive() const { return state_ == BDB_IN_PROGRESS_STATE; }
  uint8_t lastStatus() const { return lastStatus_; }

  /** Overall result: SUCCESS only if commissioning finished and the last step
      succeeded; otherwise the status that stopped it. */
  uint8_t overallStatus() const {
    if (state_ != BDB_DONE) return BDB_STATUS_IN_PROGRESS;
    return lastStatus_;
  }

 private:
  // Pick the next pending mode in BDB precedence order.
  void advance() {
    static const uint8_t order[4] = {
        BDB_MODE_INITIATOR_TOUCHLINK, BDB_MODE_NETWORK_STEERING,
        BDB_MODE_NETWORK_FORMATION, BDB_MODE_FINDING_BINDING};
    for (uint8_t i = 0; i < 4; ++i) {
      if (pending_ & order[i]) {
        current_ = order[i];
        state_ = BDB_IN_PROGRESS_STATE;
        return;
      }
    }
    current_ = 0;
    state_ = BDB_DONE;
  }

  uint8_t pending_;
  uint8_t current_;
  BdbState state_;
  uint8_t lastStatus_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_BDB_H
