#include "ZigbeeRouting.h"

namespace nzb {

ZigbeeRouting::ZigbeeRouting() : routes_(nullptr), requestId_(0) {
  for (uint8_t i = 0; i < kMaxDiscoveries; ++i) {
    discoveries_[i] = ZigbeeRouteDiscoveryEntry();
  }
}

void ZigbeeRouting::attachRouteTable(ZigbeeRouteTable& routes) {
  routes_ = &routes;
}

ZigbeeRouteDiscoveryEntry* ZigbeeRouting::findDiscovery(uint8_t requestId,
                                                        uint16_t originator) {
  for (uint8_t i = 0; i < kMaxDiscoveries; ++i) {
    ZigbeeRouteDiscoveryEntry& d = discoveries_[i];
    if (d.used && d.requestId == requestId && d.originator == originator) {
      return &d;
    }
  }
  return nullptr;
}

ZigbeeRouteDiscoveryEntry* ZigbeeRouting::allocDiscovery(uint32_t nowMs) {
  ZigbeeRouteDiscoveryEntry* oldest = nullptr;
  for (uint8_t i = 0; i < kMaxDiscoveries; ++i) {
    ZigbeeRouteDiscoveryEntry& d = discoveries_[i];
    if (!d.used) return &d;
    if (!oldest || d.expiresMs < oldest->expiresMs) oldest = &d;
  }
  (void)nowMs;
  return oldest;  // table full: recycle the entry closest to expiry
}

uint8_t ZigbeeRouting::originateDiscovery(uint16_t destination,
                                          uint32_t nowMs) {
  ++requestId_;
  if (routes_) {
    routes_->upsert(destination, kNoNextHop, ZB_ROUTE_DISCOVERY_UNDERWAY,
                    false, false, nowMs);
  }
  return requestId_;
}

ZigbeeRreqDecision ZigbeeRouting::handleRouteRequest(
    uint16_t selfShort, bool isRouter, uint16_t previousHop,
    uint16_t originator, const NwkRouteRequestCommand& rreq, uint8_t linkCost,
    uint32_t nowMs) {
  ZigbeeRreqDecision decision = ZigbeeRreqDecision();
  decision.replyTo = previousHop;

  if (!rreq.valid || originator == selfShort) {
    decision.duplicate = true;  // our own broadcast echoed back
    return decision;
  }

  uint8_t cost = (uint8_t)(rreq.pathCost + linkCost);

  ZigbeeRouteDiscoveryEntry* entry =
      findDiscovery(rreq.routeRequestId, originator);
  if (entry && entry->forwardCost <= cost) {
    decision.duplicate = true;  // already seen via an equal-or-better path
    return decision;
  }
  if (!entry) {
    entry = allocDiscovery(nowMs);
    if (!entry) {
      decision.duplicate = true;
      return decision;
    }
  }
  entry->used = true;
  entry->requestId = rreq.routeRequestId;
  entry->originator = originator;
  entry->previousHop = previousHop;
  entry->forwardCost = cost;
  entry->expiresMs = nowMs + kDiscoveryLifetimeMs;

  // Reverse route: we now know how to reach the originator.
  if (routes_) {
    routes_->upsert(originator, previousHop, ZB_ROUTE_ACTIVE, false, false,
                    nowMs);
  }

  if (rreq.destination == selfShort) {
    decision.replyAsDestination = true;
    decision.pathCost = 0;  // reply cost accumulates hop by hop on the way back
  } else if (isRouter) {
    decision.rebroadcast = true;
    decision.pathCost = cost;
  }
  return decision;
}

ZigbeeRrepDecision ZigbeeRouting::handleRouteReply(
    uint16_t selfShort, uint16_t previousHop,
    const NwkRouteReplyCommand& rrep, uint8_t linkCost, uint32_t nowMs) {
  ZigbeeRrepDecision decision = ZigbeeRrepDecision();
  if (!rrep.valid) return decision;

  uint8_t cost = (uint8_t)(rrep.pathCost + linkCost);
  decision.pathCost = cost;

  if (rrep.originator == selfShort) {
    // Our discovery completed: the responder is reachable via whoever
    // delivered this reply.
    if (routes_) {
      routes_->upsert(rrep.responder, previousHop, ZB_ROUTE_ACTIVE, false,
                      false, nowMs);
    }
    decision.routeInstalled = true;
    return decision;
  }

  // Intermediate hop: relay toward the originator along the reverse path
  // recorded when the matching Route Request passed through.
  ZigbeeRouteDiscoveryEntry* entry =
      findDiscovery(rrep.routeRequestId, rrep.originator);
  if (entry) {
    decision.forward = true;
    decision.forwardTo = entry->previousHop;
    // Forward route: the responder is reachable via the reply's sender.
    if (routes_) {
      routes_->upsert(rrep.responder, previousHop, ZB_ROUTE_ACTIVE, false,
                      false, nowMs);
    }
  }
  return decision;
}

uint16_t ZigbeeRouting::nextHopFor(uint16_t destination) const {
  if (!routes_) return kNoNextHop;
  const ZigbeeRoute* route = routes_->find(destination);
  if (!route || route->status != ZB_ROUTE_ACTIVE) return kNoNextHop;
  return route->nextHop;
}

bool ZigbeeRouting::routeIsActive(uint16_t destination) const {
  return nextHopFor(destination) != kNoNextHop;
}

void ZigbeeRouting::expire(uint32_t nowMs) {
  for (uint8_t i = 0; i < kMaxDiscoveries; ++i) {
    ZigbeeRouteDiscoveryEntry& d = discoveries_[i];
    if (!d.used) continue;
    if ((int32_t)(nowMs - d.expiresMs) >= 0) {
      d = ZigbeeRouteDiscoveryEntry();
    }
  }
  if (routes_) {
    // Discoveries that never completed leave DISCOVERY_UNDERWAY routes
    // behind; let callers re-originate after expiry.
    // (Routes are small; a stale UNDERWAY entry is replaced on re-discovery.)
  }
}

}  // namespace nzb
