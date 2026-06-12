/*
  ZigbeeTables.h - small fixed-storage tables for Zigbee PRO state.

  These tables do not perform routing or joining by themselves. They provide the
  local storage primitives that join, neighbor aging, and route discovery can
  build on without dynamic allocation.
*/
#ifndef NIUS_ZIGBEE_TABLES_H
#define NIUS_ZIGBEE_TABLES_H

#include <Arduino.h>

namespace nzb {

enum ZigbeeDeviceType : uint8_t {
  ZB_DEVICE_COORDINATOR = 0,
  ZB_DEVICE_ROUTER = 1,
  ZB_DEVICE_END_DEVICE = 2,
  ZB_DEVICE_UNKNOWN = 0xFF
};

enum ZigbeeNeighborRelationship : uint8_t {
  ZB_REL_PARENT = 0,
  ZB_REL_CHILD = 1,
  ZB_REL_SIBLING = 2,
  ZB_REL_NONE = 3,
  ZB_REL_PREVIOUS_CHILD = 4
};

struct ZigbeeNeighbor {
  bool used;
  uint16_t nwkAddress;
  uint64_t ieeeAddress;
  uint8_t deviceType;
  uint8_t relationship;
  uint8_t depth;
  uint8_t lqi;
  uint8_t incomingCost;  ///< link cost we measure (1 best .. 7, 0 unknown)
  uint8_t outgoingCost;  ///< cost the neighbor reports for hearing us
  bool rxOnWhenIdle;
  bool permitJoining;
  uint32_t lastSeenMs;
};

class ZigbeeNeighborTable {
 public:
  ZigbeeNeighborTable();
  ZigbeeNeighborTable(ZigbeeNeighbor* entries, uint8_t capacity);

  void begin(ZigbeeNeighbor* entries, uint8_t capacity);
  void clear();
  uint8_t capacity() const { return capacity_; }
  uint8_t count() const;

  /** Raw slot access (0..capacity-1); may return unused entries. */
  const ZigbeeNeighbor* slot(uint8_t index) const {
    return (entries_ && index < capacity_) ? &entries_[index] : nullptr;
  }

  ZigbeeNeighbor* findByNwk(uint16_t nwkAddress);
  const ZigbeeNeighbor* findByNwk(uint16_t nwkAddress) const;
  ZigbeeNeighbor* findByIeee(uint64_t ieeeAddress);
  const ZigbeeNeighbor* findByIeee(uint64_t ieeeAddress) const;

  ZigbeeNeighbor* upsert(uint16_t nwkAddress, uint64_t ieeeAddress,
                         uint8_t deviceType, uint8_t relationship,
                         uint8_t depth, uint8_t lqi,
                         bool rxOnWhenIdle, bool permitJoining,
                         uint32_t nowMs = millis());
  bool removeByNwk(uint16_t nwkAddress);
  uint8_t removeOlderThan(uint32_t cutoffMs);

  /** Remove router/coordinator neighbors (except @p spareAddress, normally
      the parent) whose lastSeenMs is older than @p cutoffMs. Used by the
      Link Status aging protocol. @return number of entries removed. */
  uint8_t removeStaleRouters(uint32_t cutoffMs,
                             uint16_t spareAddress = 0xFFFF);

  const ZigbeeNeighbor* bestParentCandidate() const;

 private:
  ZigbeeNeighbor* entries_;
  uint8_t capacity_;

  ZigbeeNeighbor* firstFree();
  ZigbeeNeighbor* leastRecentlySeen();
};

enum ZigbeeRouteStatus : uint8_t {
  ZB_ROUTE_ACTIVE = 0,
  ZB_ROUTE_DISCOVERY_UNDERWAY = 1,
  ZB_ROUTE_DISCOVERY_FAILED = 2,
  ZB_ROUTE_INACTIVE = 3,
  ZB_ROUTE_VALIDATION_UNDERWAY = 4
};

struct ZigbeeRoute {
  bool used;
  uint16_t dstAddress;
  uint16_t nextHop;
  uint8_t status;
  bool manyToOne;
  bool routeRecordRequired;
  uint32_t updatedMs;
};

class ZigbeeRouteTable {
 public:
  ZigbeeRouteTable();
  ZigbeeRouteTable(ZigbeeRoute* entries, uint8_t capacity);

  void begin(ZigbeeRoute* entries, uint8_t capacity);
  void clear();
  uint8_t capacity() const { return capacity_; }
  uint8_t count() const;

  ZigbeeRoute* find(uint16_t dstAddress);
  const ZigbeeRoute* find(uint16_t dstAddress) const;
  ZigbeeRoute* upsert(uint16_t dstAddress, uint16_t nextHop,
                      uint8_t status = ZB_ROUTE_ACTIVE,
                      bool manyToOne = false,
                      bool routeRecordRequired = false,
                      uint32_t nowMs = millis());
  bool remove(uint16_t dstAddress);
  uint8_t removeInactive();

 private:
  ZigbeeRoute* entries_;
  uint8_t capacity_;

  ZigbeeRoute* firstFree();
  ZigbeeRoute* leastRecentlyUpdated();
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_TABLES_H
