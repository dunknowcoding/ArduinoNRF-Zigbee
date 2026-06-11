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
  ZB_NWK_STATE_LEAVING = 5
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
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_NETWORK_H
