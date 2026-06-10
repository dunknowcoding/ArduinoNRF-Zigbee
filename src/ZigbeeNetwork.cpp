#include "ZigbeeNetwork.h"

namespace nzb {

ZigbeePermitJoin::ZigbeePermitJoin()
    : open_(false), durationSeconds_(0), openedMs_(0) {}

void ZigbeePermitJoin::open(uint8_t durationSeconds, uint32_t nowMs) {
  durationSeconds_ = durationSeconds;
  openedMs_ = nowMs;
  open_ = durationSeconds != 0;
}

void ZigbeePermitJoin::close() {
  open_ = false;
  durationSeconds_ = 0;
  openedMs_ = 0;
}

bool ZigbeePermitJoin::isOpen(uint32_t nowMs) const {
  if (!open_) return false;
  if (durationSeconds_ == 0xFF) return true;
  uint32_t elapsedMs = nowMs - openedMs_;
  return elapsedMs < (uint32_t)durationSeconds_ * 1000UL;
}

uint8_t ZigbeePermitJoin::remainingSeconds(uint32_t nowMs) const {
  if (!isOpen(nowMs)) return 0;
  if (durationSeconds_ == 0xFF) return 0xFF;
  uint32_t elapsedSec = (nowMs - openedMs_) / 1000UL;
  if (elapsedSec >= durationSeconds_) return 0;
  return (uint8_t)(durationSeconds_ - elapsedSec);
}

ZigbeeAddressAllocator::ZigbeeAddressAllocator()
    : firstAddress_(0x0001), lastAddress_(0xFFF7), nextCandidate_(0x0001) {}

ZigbeeAddressAllocator::ZigbeeAddressAllocator(uint16_t firstAddress,
                                               uint16_t lastAddress) {
  begin(firstAddress, lastAddress);
}

void ZigbeeAddressAllocator::begin(uint16_t firstAddress,
                                   uint16_t lastAddress) {
  firstAddress_ = firstAddress;
  lastAddress_ = lastAddress;
  if (!isUsable(firstAddress_) || firstAddress_ > lastAddress_) {
    firstAddress_ = 0x0001;
  }
  if (!isUsable(lastAddress_) || lastAddress_ < firstAddress_) {
    lastAddress_ = 0xFFF7;
  }
  nextCandidate_ = firstAddress_;
}

bool ZigbeeAddressAllocator::isUsable(uint16_t address) const {
  return address != ZB_NWK_ADDR_COORDINATOR &&
         address < ZB_NWK_ADDR_RESERVED_MIN;
}

bool ZigbeeAddressAllocator::isInPool(uint16_t address) const {
  return isUsable(address) && address >= firstAddress_ && address <= lastAddress_;
}

uint16_t ZigbeeAddressAllocator::advance(uint16_t address) const {
  if (address >= lastAddress_) return firstAddress_;
  return (uint16_t)(address + 1);
}

uint16_t ZigbeeAddressAllocator::allocate(const ZigbeeNeighborTable& neighbors) {
  if (firstAddress_ > lastAddress_) return ZB_NWK_ADDR_INVALID;
  uint16_t candidate = nextCandidate_;
  uint16_t span = (uint16_t)(lastAddress_ - firstAddress_ + 1);
  for (uint16_t i = 0; i < span; ++i) {
    if (isInPool(candidate) && !neighbors.findByNwk(candidate)) {
      nextCandidate_ = advance(candidate);
      return candidate;
    }
    candidate = advance(candidate);
  }
  return ZB_NWK_ADDR_INVALID;
}

ZigbeeNetwork::ZigbeeNetwork()
    : info_(), neighbors_(nullptr), permitJoin_(), allocator_() {
  leave();
}

void ZigbeeNetwork::attachNeighborTable(ZigbeeNeighborTable& neighbors) {
  neighbors_ = &neighbors;
}

void ZigbeeNetwork::configureAddressPool(uint16_t firstAddress,
                                         uint16_t lastAddress) {
  allocator_.begin(firstAddress, lastAddress);
}

void ZigbeeNetwork::beginCoordinator(uint16_t panId, uint64_t extendedPanId,
                                     uint8_t channel, uint8_t updateId) {
  info_ = ZigbeeNetworkInfo();
  info_.joined = true;
  info_.deviceType = ZB_DEVICE_COORDINATOR;
  info_.panId = panId;
  info_.extendedPanId = extendedPanId;
  info_.channel = channel;
  info_.nwkAddress = ZB_NWK_ADDR_COORDINATOR;
  info_.parentAddress = ZB_NWK_ADDR_INVALID;
  info_.depth = 0;
  info_.updateId = updateId;
  info_.outgoingFrameCounter = 0;
}

void ZigbeeNetwork::beginJoinedDevice(uint8_t deviceType, uint16_t panId,
                                      uint64_t extendedPanId, uint8_t channel,
                                      uint16_t nwkAddress,
                                      uint16_t parentAddress, uint8_t depth,
                                      uint8_t updateId) {
  info_ = ZigbeeNetworkInfo();
  info_.joined = true;
  info_.deviceType = deviceType;
  info_.panId = panId;
  info_.extendedPanId = extendedPanId;
  info_.channel = channel;
  info_.nwkAddress = nwkAddress;
  info_.parentAddress = parentAddress;
  info_.depth = depth;
  info_.updateId = updateId;
  info_.outgoingFrameCounter = 0;
}

void ZigbeeNetwork::leave() {
  info_ = ZigbeeNetworkInfo();
  info_.joined = false;
  info_.deviceType = ZB_DEVICE_UNKNOWN;
  info_.panId = 0xFFFF;
  info_.extendedPanId = 0;
  info_.channel = 0;
  info_.nwkAddress = ZB_NWK_ADDR_INVALID;
  info_.parentAddress = ZB_NWK_ADDR_INVALID;
  info_.depth = 0xFF;
  info_.updateId = 0;
  info_.outgoingFrameCounter = 0;
  permitJoin_.close();
}

uint32_t ZigbeeNetwork::nextFrameCounter() {
  return ++info_.outgoingFrameCounter;
}

ZigbeeNeighbor* ZigbeeNetwork::acceptChild(uint64_t ieeeAddress,
                                           uint8_t deviceType,
                                           bool rxOnWhenIdle, uint8_t lqi,
                                           uint32_t nowMs) {
  if (!neighbors_ || !isCoordinator()) return nullptr;
  if (!permitJoin_.isOpen(nowMs)) return nullptr;
  ZigbeeNeighbor* existing = neighbors_->findByIeee(ieeeAddress);
  uint16_t shortAddress = existing ? existing->nwkAddress
                                   : allocator_.allocate(*neighbors_);
  if (shortAddress == ZB_NWK_ADDR_INVALID) return nullptr;
  return neighbors_->upsert(shortAddress, ieeeAddress, deviceType, ZB_REL_CHILD,
                            (uint8_t)(info_.depth + 1), lqi, rxOnWhenIdle,
                            false, nowMs);
}

bool ZigbeeNetwork::noteParent(uint16_t nwkAddress, uint64_t ieeeAddress,
                               uint8_t deviceType, uint8_t depth, uint8_t lqi,
                               bool permitJoining, uint32_t nowMs) {
  if (!neighbors_ || !info_.joined) return false;
  ZigbeeNeighbor* parent = neighbors_->upsert(
      nwkAddress, ieeeAddress, deviceType, ZB_REL_PARENT, depth, lqi, true,
      permitJoining, nowMs);
  if (!parent) return false;
  info_.parentAddress = nwkAddress;
  return true;
}

}  // namespace nzb
