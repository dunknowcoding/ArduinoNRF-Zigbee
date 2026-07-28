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
    const ZigbeeNeighbor* occupant = neighbors.findByNwk(candidate);
    // Link Status can teach us a short-address-only sibling before that
    // device associates. Such an IEEE-unknown observation is not an address
    // lease owned by this parent and must not consume its child pool.
    const bool allocatedChild =
        occupant && occupant->ieeeAddress != 0 &&
        (occupant->relationship == ZB_REL_CHILD ||
         occupant->relationship == ZB_REL_PREVIOUS_CHILD);
    if (isInPool(candidate) && !allocatedChild) {
      nextCandidate_ = advance(candidate);
      return candidate;
    }
    candidate = advance(candidate);
  }
  return ZB_NWK_ADDR_INVALID;
}

ZigbeeNetwork::ZigbeeNetwork()
    : info_(), neighbors_(nullptr), permitJoin_(), allocator_(),
      scanDeviceType_(ZB_DEVICE_UNKNOWN), joinAttempts_(0),
      lastLinkStatusMs_(0) {
  clearCandidates();
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
  info_.state = ZB_NWK_STATE_COORDINATOR;
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
  info_.state = (deviceType == ZB_DEVICE_ROUTER) ? ZB_NWK_STATE_ROUTER :
                (deviceType == ZB_DEVICE_END_DEVICE) ? ZB_NWK_STATE_END_DEVICE :
                ZB_NWK_STATE_IDLE;
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

void ZigbeeNetwork::beginJoining(uint8_t deviceType, uint16_t panId,
                                 uint64_t extendedPanId, uint8_t channel,
                                 uint16_t parentAddress, uint8_t updateId) {
  info_ = ZigbeeNetworkInfo();
  info_.joined = false;
  info_.state = ZB_NWK_STATE_JOINING;
  info_.deviceType = deviceType;
  info_.panId = panId;
  info_.extendedPanId = extendedPanId;
  info_.channel = channel;
  info_.nwkAddress = ZB_NWK_ADDR_INVALID;
  info_.parentAddress = parentAddress;
  info_.depth = 0xFF;
  info_.updateId = updateId;
  info_.outgoingFrameCounter = 0;
}

bool ZigbeeNetwork::completeJoin(uint16_t nwkAddress,
                                 uint16_t parentAddress,
                                 uint8_t parentDepth) {
  if (!isJoining()) return false;
  if (!allocator_.isUsable(nwkAddress)) return false;
  info_.joined = true;
  info_.nwkAddress = nwkAddress;
  info_.parentAddress = parentAddress;
  info_.depth = (uint8_t)(parentDepth + 1);
  info_.state = (info_.deviceType == ZB_DEVICE_ROUTER) ? ZB_NWK_STATE_ROUTER :
                ZB_NWK_STATE_END_DEVICE;
  return true;
}

void ZigbeeNetwork::leave() {
  info_ = ZigbeeNetworkInfo();
  info_.joined = false;
  info_.state = ZB_NWK_STATE_IDLE;
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
  if (!neighbors_ || (!isCoordinator() && !isRouter())) return nullptr;
  if (!permitJoin_.isOpen(nowMs)) return nullptr;
  ZigbeeNeighbor* existing = neighbors_->findByIeee(ieeeAddress);
  uint16_t shortAddress = existing ? existing->nwkAddress
                                   : allocator_.allocate(*neighbors_);
  if (shortAddress == ZB_NWK_ADDR_INVALID) return nullptr;
  return neighbors_->upsert(shortAddress, ieeeAddress, deviceType, ZB_REL_CHILD,
                            (uint8_t)(info_.depth + 1), lqi, rxOnWhenIdle,
                            false, nowMs);
}

ZigbeeAssociationDecision ZigbeeNetwork::handleAssociationRequest(
    uint64_t ieeeAddress, const MacAssociationRequest& request,
    uint8_t lqi, uint32_t nowMs) {
  ZigbeeAssociationDecision decision = ZigbeeAssociationDecision();
  decision.status = MAC_ASSOC_PAN_ACCESS_DENIED;
  decision.assignedAddress = ZB_NWK_ADDR_INVALID;
  if (!request.valid || !request.allocateAddress) {
    return decision;
  }
  if (!neighbors_ || (!isCoordinator() && !isRouter()) ||
      !permitJoin_.isOpen(nowMs)) {
    return decision;
  }

  uint8_t childType = request.fullFunctionDevice ? ZB_DEVICE_ROUTER :
                      ZB_DEVICE_END_DEVICE;
  ZigbeeNeighbor* child = acceptChild(ieeeAddress, childType,
                                      request.receiverOnWhenIdle, lqi, nowMs);
  if (!child) {
    decision.status = MAC_ASSOC_PAN_AT_CAPACITY;
    return decision;
  }

  decision.accepted = true;
  decision.assignedAddress = child->nwkAddress;
  decision.status = MAC_ASSOC_SUCCESS;
  decision.deviceType = childType;
  decision.rxOnWhenIdle = request.receiverOnWhenIdle;
  return decision;
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

void ZigbeeNetwork::clearCandidates() {
  for (uint8_t i = 0; i < kMaxParentCandidates; ++i) {
    candidates_[i] = ZigbeeParentCandidate();
  }
}

void ZigbeeNetwork::beginScan(uint8_t deviceType) {
  clearCandidates();
  scanDeviceType_ = deviceType;
  info_ = ZigbeeNetworkInfo();
  info_.joined = false;
  info_.state = ZB_NWK_STATE_SCANNING;
  info_.deviceType = deviceType;
  info_.panId = 0xFFFF;
  info_.nwkAddress = ZB_NWK_ADDR_INVALID;
  info_.parentAddress = ZB_NWK_ADDR_INVALID;
  info_.depth = 0xFF;
}

bool ZigbeeNetwork::noteBeacon(uint8_t channel, const MacBeaconFrame& beacon,
                               const NwkBeaconPayload& payload, int8_t rssi,
                               uint8_t lqi, uint64_t requiredExtendedPanId) {
  if (!beacon.valid || !payload.valid) return false;
  if (requiredExtendedPanId != 0 &&
      payload.extendedPanId != requiredExtendedPanId) {
    return false;
  }

  // Deduplicate on channel + PAN + sender; keep the best-LQI sighting.
  ZigbeeParentCandidate* slot = nullptr;
  for (uint8_t i = 0; i < kMaxParentCandidates; ++i) {
    ZigbeeParentCandidate& c = candidates_[i];
    if (c.used && c.channel == channel && c.panId == beacon.srcPanId &&
        c.shortAddress == beacon.srcShort) {
      if (lqi < c.lqi) return true;  // already have a better sighting
      slot = &c;
      break;
    }
    if (!slot && !c.used) slot = &c;
  }
  if (!slot) {
    // Table full: replace the weakest entry if this one is stronger.
    ZigbeeParentCandidate* weakest = &candidates_[0];
    for (uint8_t i = 1; i < kMaxParentCandidates; ++i) {
      if (candidates_[i].lqi < weakest->lqi) weakest = &candidates_[i];
    }
    if (weakest->lqi >= lqi) return false;
    slot = weakest;
  }

  slot->used = true;
  slot->channel = channel;
  slot->panId = beacon.srcPanId;
  slot->extendedPanId = payload.extendedPanId;
  slot->shortAddress = beacon.srcShort;
  slot->depth = payload.deviceDepth;
  slot->lqi = lqi;
  slot->rssi = rssi;
  slot->permitJoining = beacon.associationPermit;
  slot->routerCapacity = payload.routerCapacity;
  slot->endDeviceCapacity = payload.endDeviceCapacity;
  slot->stackProfile = payload.stackProfile;
  slot->updateId = payload.updateId;
  return true;
}

uint8_t ZigbeeNetwork::candidateCount() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < kMaxParentCandidates; ++i) {
    if (candidates_[i].used) ++n;
  }
  return n;
}

const ZigbeeParentCandidate* ZigbeeNetwork::candidate(uint8_t index) const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < kMaxParentCandidates; ++i) {
    if (!candidates_[i].used) continue;
    if (n == index) return &candidates_[i];
    ++n;
  }
  return nullptr;
}

bool ZigbeeNetwork::candidateUsableFor(const ZigbeeParentCandidate& c,
                                       uint8_t deviceType) const {
  if (!c.used || !c.permitJoining) return false;
  if (c.stackProfile != ZigbeeNwk::kStackProfilePro) return false;
  if (deviceType == ZB_DEVICE_ROUTER) return c.routerCapacity;
  if (deviceType == ZB_DEVICE_END_DEVICE) return c.endDeviceCapacity;
  return false;
}

const ZigbeeParentCandidate* ZigbeeNetwork::selectParent() const {
  const ZigbeeParentCandidate* best = nullptr;
  for (uint8_t i = 0; i < kMaxParentCandidates; ++i) {
    const ZigbeeParentCandidate& c = candidates_[i];
    if (!candidateUsableFor(c, scanDeviceType_)) continue;
    if (!best || c.lqi > best->lqi ||
        (c.lqi == best->lqi && c.depth < best->depth)) {
      best = &c;
    }
  }
  return best;
}

bool ZigbeeNetwork::beginJoiningCandidate(const ZigbeeParentCandidate& parent) {
  if (!parent.used) return false;
  uint8_t deviceType = scanDeviceType_;
  beginJoining(deviceType, parent.panId, parent.extendedPanId, parent.channel,
               parent.shortAddress, parent.updateId);
  return true;
}

uint8_t ZigbeeNetwork::costFromLqi(uint8_t lqi) {
  if (lqi >= 105) return 1;
  if (lqi >= 95) return 2;
  if (lqi >= 85) return 3;
  if (lqi >= 75) return 4;
  if (lqi >= 65) return 5;
  if (lqi >= 55) return 6;
  return 7;
}

bool ZigbeeNetwork::linkStatusDue(uint32_t nowMs) const {
  if (!isCoordinator() && !isRouter()) return false;
  return (uint32_t)(nowMs - lastLinkStatusMs_) >= kLinkStatusPeriodMs;
}

void ZigbeeNetwork::markLinkStatusSent(uint32_t nowMs) {
  lastLinkStatusMs_ = nowMs;
}

uint8_t ZigbeeNetwork::collectLinkStatusEntries(NwkLinkStatusEntry* entries,
                                                uint8_t maxEntries) const {
  if (!neighbors_ || !entries || maxEntries == 0) return 0;
  uint8_t n = 0;
  for (uint8_t i = 0; i < neighbors_->capacity() && n < maxEntries; ++i) {
    const ZigbeeNeighbor* neighbor = neighbors_->slot(i);
    if (!neighbor || !neighbor->used) continue;
    if (neighbor->relationship == ZB_REL_PREVIOUS_CHILD) continue;
    if (neighbor->deviceType != ZB_DEVICE_ROUTER &&
        neighbor->deviceType != ZB_DEVICE_COORDINATOR) {
      continue;
    }
    entries[n].address = neighbor->nwkAddress;
    entries[n].incomingCost =
        neighbor->incomingCost ? neighbor->incomingCost
                               : costFromLqi(neighbor->lqi);
    entries[n].outgoingCost = neighbor->outgoingCost;
    ++n;
  }
  return n;
}

bool ZigbeeNetwork::handleLinkStatus(uint16_t senderShort,
                                     const NwkLinkStatusCommand& command,
                                     uint8_t lqi, uint32_t nowMs) {
  if (!neighbors_ || !command.valid || !info_.joined) return false;

  // The sender is a live router neighbor: refresh (or learn) it.
  ZigbeeNeighbor* existing = neighbors_->findByNwk(senderShort);
  uint8_t relationship = (senderShort == info_.parentAddress)
                             ? ZB_REL_PARENT
                             : ZB_REL_SIBLING;
  uint64_t ieee = 0;
  uint8_t depth = 0xFF;
  if (existing) {
    if (senderShort != info_.parentAddress) {
      relationship = existing->relationship;
    }
    ieee = existing->ieeeAddress;
    depth = existing->depth;
  }
  ZigbeeNeighbor* sender = neighbors_->upsert(
      senderShort, ieee, ZB_DEVICE_ROUTER, relationship, depth, lqi, true,
      existing ? existing->permitJoining : false, nowMs);
  if (!sender) return false;

  sender->incomingCost = costFromLqi(lqi);

  // If the sender lists us, that entry's incoming cost is OUR outgoing cost.
  for (uint8_t i = 0; i < command.entryCount; ++i) {
    NwkLinkStatusEntry entry;
    if (!ZigbeeNwk::getLinkStatusEntry(command, i, entry)) break;
    if (entry.address == info_.nwkAddress) {
      sender->outgoingCost = entry.incomingCost;
      break;
    }
  }
  return true;
}

ZigbeeNetwork::AgingResult ZigbeeNetwork::ageNeighbors(uint32_t nowMs) {
  AgingResult result;
  result.removed = 0;
  result.parentLost = false;
  if (!neighbors_ || !info_.joined) return result;

  uint32_t maxAge = kLinkStatusPeriodMs * (uint32_t)kRouterAgeLimit;
  if (nowMs < maxAge) return result;  // not enough history yet
  uint32_t cutoff = nowMs - maxAge;

  result.removed = neighbors_->removeStaleRouters(cutoff, info_.parentAddress);

  if (!isCoordinator() && info_.parentAddress != ZB_NWK_ADDR_INVALID) {
    const ZigbeeNeighbor* parent = neighbors_->findByNwk(info_.parentAddress);
    if (parent && parent->lastSeenMs < cutoff) {
      result.parentLost = true;
    }
  }
  return result;
}

bool ZigbeeNetwork::rejoinParent() {
  // Needs a remembered network identity and parent from an earlier join.
  if (info_.panId == 0xFFFF ||
      info_.parentAddress == ZB_NWK_ADDR_INVALID ||
      info_.deviceType == ZB_DEVICE_UNKNOWN ||
      info_.deviceType == ZB_DEVICE_COORDINATOR) {
    return false;
  }
  info_.joined = false;
  info_.state = ZB_NWK_STATE_JOINING;
  info_.nwkAddress = ZB_NWK_ADDR_INVALID;
  info_.depth = 0xFF;
  return true;
}

}  // namespace nzb
