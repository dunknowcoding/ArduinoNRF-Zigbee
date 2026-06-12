/*
  ZigbeeRouting.h - AODV-style NWK route discovery decisions.

  Transport-agnostic: this class decides what to do with Route Request /
  Route Reply commands (record reverse routes, answer as destination, forward,
  install routes) while the sketch keeps doing the actual radio sends. Works
  with the existing ZigbeeNwk payload helpers and ZigbeeRouteTable storage.
*/
#ifndef NIUS_ZIGBEE_ROUTING_H
#define NIUS_ZIGBEE_ROUTING_H

#include <Arduino.h>
#include "ZigbeeNwk.h"
#include "ZigbeeTables.h"

namespace nzb {

/** Pending discovery bookkeeping (one per outstanding request id). */
struct ZigbeeRouteDiscoveryEntry {
  bool used;
  uint8_t requestId;
  uint16_t originator;
  uint16_t previousHop;  ///< best-cost RREQ sender = next hop back
  uint8_t forwardCost;   ///< accumulated cost originator -> us
  uint32_t expiresMs;
};

/** What to do with a received Route Request. */
struct ZigbeeRreqDecision {
  bool duplicate;           ///< worse/equal duplicate - drop silently
  bool replyAsDestination;  ///< we are the target: unicast a Route Reply
  bool rebroadcast;         ///< intermediate router: forward the request
  uint16_t replyTo;         ///< previous hop to unicast the reply to
  uint8_t pathCost;         ///< updated accumulated cost (for the forward)
};

/** What to do with a received Route Reply. */
struct ZigbeeRrepDecision {
  bool routeInstalled;  ///< we are the originator: route is now ACTIVE
  bool forward;         ///< intermediate hop: pass the reply on
  uint16_t forwardTo;   ///< next hop toward the originator
  uint8_t pathCost;     ///< updated accumulated cost (for the forward)
};

class ZigbeeRouting {
 public:
  static const uint8_t kMaxDiscoveries = 4;
  static const uint32_t kDiscoveryLifetimeMs = 10000;
  static const uint16_t kNoNextHop = 0xFFFF;

  ZigbeeRouting();

  void attachRouteTable(ZigbeeRouteTable& routes);

  /** Start a discovery toward @p destination: marks the route
      DISCOVERY_UNDERWAY and returns the request id to send in the RREQ. */
  uint8_t originateDiscovery(uint16_t destination,
                             uint32_t nowMs = millis());

  /** Process a received Route Request.
      @param selfShort    our NWK address
      @param isRouter     whether we may rebroadcast (router/coordinator)
      @param previousHop  MAC source of the broadcast (the hop it came over)
      @param originator   NWK source (the device looking for a route)
      @param linkCost     cost of the hop it arrived over (costFromLqi). */
  ZigbeeRreqDecision handleRouteRequest(uint16_t selfShort, bool isRouter,
                                        uint16_t previousHop,
                                        uint16_t originator,
                                        const NwkRouteRequestCommand& rreq,
                                        uint8_t linkCost,
                                        uint32_t nowMs = millis());

  /** Process a received Route Reply (unicast back along the reverse path). */
  ZigbeeRrepDecision handleRouteReply(uint16_t selfShort,
                                      uint16_t previousHop,
                                      const NwkRouteReplyCommand& rrep,
                                      uint8_t linkCost,
                                      uint32_t nowMs = millis());

  /** Next hop for @p destination, or kNoNextHop when no ACTIVE route. */
  uint16_t nextHopFor(uint16_t destination) const;
  bool routeIsActive(uint16_t destination) const;

  /** Drop expired discovery entries; mark their routes FAILED. */
  void expire(uint32_t nowMs = millis());

 private:
  ZigbeeRouteTable* routes_;
  ZigbeeRouteDiscoveryEntry discoveries_[kMaxDiscoveries];
  uint8_t requestId_;

  ZigbeeRouteDiscoveryEntry* findDiscovery(uint8_t requestId,
                                           uint16_t originator);
  ZigbeeRouteDiscoveryEntry* allocDiscovery(uint32_t nowMs);
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_ROUTING_H
