/*
  ZigbeeNetwork.h - local Zigbee PRO network state primitives.

  This is not the over-the-air association/join protocol yet. It holds the local
  network identity, permit-join window, short-address allocator, and child/parent
  bookkeeping that the next join/routing layers will use.
*/
#ifndef NIUS_ZIGBEE_NETWORK_H
#define NIUS_ZIGBEE_NETWORK_H

#include <Arduino.h>
#include "ZigbeeTables.h"
#include "ZigbeeMac.h"
#include "ZigbeeNwk.h"

namespace nzb {

static const uint16_t ZB_NWK_ADDR_COORDINATOR = 0x0000;
static const uint16_t ZB_NWK_ADDR_INVALID = 0xFFFF;
static const uint16_t ZB_NWK_ADDR_RESERVED_MIN = 0xFFF8;

struct ZigbeeNetworkInfo {
  bool joined;
  uint8_t state;
  uint8_t deviceType;
  uint16_t panId;
  uint64_t extendedPanId;
  uint8_t channel;
  uint16_t nwkAddress;
  uint16_t parentAddress;
  uint8_t depth;
  uint8_t updateId;
  uint32_t outgoingFrameCounter;
};

enum ZigbeeNetworkState : uint8_t {
  ZB_NWK_STATE_IDLE = 0,
  ZB_NWK_STATE_COORDINATOR = 1,
  ZB_NWK_STATE_JOINING = 2,
  ZB_NWK_STATE_ROUTER = 3,
  ZB_NWK_STATE_END_DEVICE = 4,
  ZB_NWK_STATE_LEAVING = 5,
  ZB_NWK_STATE_SCANNING = 6
};

/** One network discovered by an active (beacon) scan. */
struct ZigbeeParentCandidate {
  bool used;
  uint8_t channel;
  uint16_t panId;
  uint64_t extendedPanId;
  uint16_t shortAddress;   ///< beacon sender = potential parent
  uint8_t depth;           ///< parent's tree depth (we would be depth+1)
  uint8_t lqi;
  int8_t rssi;
  bool permitJoining;      ///< MAC association-permit bit from the beacon
  bool routerCapacity;
  bool endDeviceCapacity;
  uint8_t stackProfile;
  uint8_t updateId;
};

struct ZigbeeAssociationDecision {
  bool accepted;
  uint16_t assignedAddress;
  uint8_t status;
  uint8_t deviceType;
  bool rxOnWhenIdle;
};

class ZigbeePermitJoin {
 public:
  ZigbeePermitJoin();

  void open(uint8_t durationSeconds, uint32_t nowMs = millis());
  void close();
  bool isOpen(uint32_t nowMs = millis()) const;
  uint8_t remainingSeconds(uint32_t nowMs = millis()) const;
  uint8_t durationSeconds() const { return durationSeconds_; }

 private:
  bool open_;
  uint8_t durationSeconds_;
  uint32_t openedMs_;
};

class ZigbeeAddressAllocator {
 public:
  ZigbeeAddressAllocator();
  ZigbeeAddressAllocator(uint16_t firstAddress, uint16_t lastAddress);

  void begin(uint16_t firstAddress, uint16_t lastAddress);
  uint16_t firstAddress() const { return firstAddress_; }
  uint16_t lastAddress() const { return lastAddress_; }
  uint16_t nextCandidate() const { return nextCandidate_; }

  bool isUsable(uint16_t address) const;
  bool isInPool(uint16_t address) const;
  uint16_t allocate(const ZigbeeNeighborTable& neighbors);

 private:
  uint16_t firstAddress_;
  uint16_t lastAddress_;
  uint16_t nextCandidate_;

  uint16_t advance(uint16_t address) const;
};

class ZigbeeNetwork {
 public:
  ZigbeeNetwork();

  void attachNeighborTable(ZigbeeNeighborTable& neighbors);
  void configureAddressPool(uint16_t firstAddress, uint16_t lastAddress);

  void beginCoordinator(uint16_t panId, uint64_t extendedPanId,
                        uint8_t channel, uint8_t updateId = 0);
  void beginJoinedDevice(uint8_t deviceType, uint16_t panId,
                         uint64_t extendedPanId, uint8_t channel,
                         uint16_t nwkAddress, uint16_t parentAddress,
                         uint8_t depth, uint8_t updateId = 0);
  void beginJoining(uint8_t deviceType, uint16_t panId,
                    uint64_t extendedPanId, uint8_t channel,
                    uint16_t parentAddress, uint8_t updateId = 0);
  bool completeJoin(uint16_t nwkAddress, uint16_t parentAddress,
                    uint8_t parentDepth = 0);
  void leave();

  // -- active scan / parent selection --------------------------------------

  static const uint8_t kMaxParentCandidates = 8;

  /** Enter SCANNING: clears the candidate list; @p deviceType is what we
      will join as (used by capacity filtering). */
  void beginScan(uint8_t deviceType);
  bool isScanning() const { return info_.state == ZB_NWK_STATE_SCANNING; }

  /** Record a received beacon as a parent candidate (deduplicated on
      channel+PAN+sender; keeps the best LQI). When @p requiredExtendedPanId
      is nonzero, beacons from other networks are ignored. */
  bool noteBeacon(uint8_t channel, const MacBeaconFrame& beacon,
                  const NwkBeaconPayload& payload, int8_t rssi, uint8_t lqi,
                  uint64_t requiredExtendedPanId = 0);

  uint8_t candidateCount() const;
  const ZigbeeParentCandidate* candidate(uint8_t index) const;

  /** Pick the best joinable candidate for the scan's device type: must
      permit joining, have capacity for us, and run our stack profile; ties
      break on LQI (higher) then depth (shallower). nullptr if none. */
  const ZigbeeParentCandidate* selectParent() const;

  /** Move SCANNING -> JOINING toward @p parent (keeps the scan device type). */
  bool beginJoiningCandidate(const ZigbeeParentCandidate& parent);

  /** After a lost parent: re-enter JOINING toward the remembered parent and
      network so the caller can re-associate without a fresh scan. */
  bool rejoinParent();

  // join-attempt bookkeeping for the caller's retry/backoff loop
  uint8_t joinAttempts() const { return joinAttempts_; }
  void noteJoinAttempt() { ++joinAttempts_; }
  void resetJoinAttempts() { joinAttempts_ = 0; }

  const ZigbeeNetworkInfo& info() const { return info_; }
  bool isJoined() const { return info_.joined; }
  bool isCoordinator() const {
    return info_.joined && info_.deviceType == ZB_DEVICE_COORDINATOR;
  }
  bool isRouter() const {
    return info_.joined && info_.deviceType == ZB_DEVICE_ROUTER;
  }
  bool isEndDevice() const {
    return info_.joined && info_.deviceType == ZB_DEVICE_END_DEVICE;
  }
  bool isJoining() const { return info_.state == ZB_NWK_STATE_JOINING; }

  void permitJoining(uint8_t durationSeconds, uint32_t nowMs = millis()) {
    permitJoin_.open(durationSeconds, nowMs);
  }
  void closeJoining() { permitJoin_.close(); }
  bool isJoiningPermitted(uint32_t nowMs = millis()) const {
    return permitJoin_.isOpen(nowMs);
  }
  uint8_t joiningSecondsRemaining(uint32_t nowMs = millis()) const {
    return permitJoin_.remainingSeconds(nowMs);
  }

  uint32_t nextFrameCounter();

  ZigbeeNeighbor* acceptChild(uint64_t ieeeAddress, uint8_t deviceType,
                              bool rxOnWhenIdle, uint8_t lqi,
                              uint32_t nowMs = millis());
  ZigbeeAssociationDecision handleAssociationRequest(
      uint64_t ieeeAddress, const MacAssociationRequest& request,
      uint8_t lqi, uint32_t nowMs = millis());
  bool noteParent(uint16_t nwkAddress, uint64_t ieeeAddress,
                  uint8_t deviceType, uint8_t depth, uint8_t lqi,
                  bool permitJoining, uint32_t nowMs = millis());

 private:
  ZigbeeNetworkInfo info_;
  ZigbeeNeighborTable* neighbors_;
  ZigbeePermitJoin permitJoin_;
  ZigbeeAddressAllocator allocator_;
  ZigbeeParentCandidate candidates_[kMaxParentCandidates];
  uint8_t scanDeviceType_;
  uint8_t joinAttempts_;

  void clearCandidates();
  bool candidateUsableFor(const ZigbeeParentCandidate& c,
                          uint8_t deviceType) const;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_NETWORK_H
