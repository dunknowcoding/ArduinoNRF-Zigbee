/*
  ZigbeeGroupTable.h - device group membership (Zigbee group addressing).

  Zigbee group addressing lets one APS frame reach a whole set of endpoints at
  once - the classic "turn this group of lights on" command. A group-addressed
  APS data frame (ZigbeeAps::buildGroupDataFrame, delivery mode = group) carries
  a 16-bit group address instead of a destination endpoint and is sent as a NWK
  broadcast to all rx-on devices; each receiver delivers it to its application
  only if it is a MEMBER of that group.

  This is that membership store. A device adds/removes itself from groups (a
  coordinator drives this via the ZCL Groups cluster), and the APS receive path
  asks isMember() before acting on a group-addressed frame. Header-only.
*/
#ifndef NIUS_ZIGBEE_GROUP_TABLE_H
#define NIUS_ZIGBEE_GROUP_TABLE_H

#include <Arduino.h>

namespace nzb {

class ZigbeeGroupTable {
 public:
  ZigbeeGroupTable() : groups_(nullptr), capacity_(0), count_(0) {}
  ZigbeeGroupTable(uint16_t* storage, uint8_t capacity) {
    begin(storage, capacity);
  }
  void begin(uint16_t* storage, uint8_t capacity) {
    groups_ = storage;
    capacity_ = capacity;
    count_ = 0;
  }

  uint8_t capacity() const { return capacity_; }
  uint8_t count() const { return count_; }

  /** Join @p groupId. Idempotent (re-adding a current member succeeds without
      growing). @return false only if the table is full and the group is new. */
  bool add(uint16_t groupId) {
    if (!groups_) return false;
    if (isMember(groupId)) return true;
    if (count_ >= capacity_) return false;
    groups_[count_++] = groupId;
    return true;
  }

  /** Leave @p groupId. @return true if it was a member. */
  bool remove(uint16_t groupId) {
    if (!groups_) return false;
    for (uint8_t i = 0; i < count_; ++i) {
      if (groups_[i] == groupId) {
        groups_[i] = groups_[count_ - 1];  // swap-remove (order not significant)
        --count_;
        return true;
      }
    }
    return false;
  }

  bool isMember(uint16_t groupId) const {
    for (uint8_t i = 0; i < count_; ++i)
      if (groups_[i] == groupId) return true;
    return false;
  }

  /** The @p index-th group id, for enumerating membership (e.g. a ZCL Get Group
      Membership response). Returns 0xFFFF if out of range. */
  uint16_t at(uint8_t index) const {
    return (index < count_) ? groups_[index] : 0xFFFF;
  }

  void clear() { count_ = 0; }

 private:
  uint16_t* groups_;
  uint8_t capacity_;
  uint8_t count_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_GROUP_TABLE_H
