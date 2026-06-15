/*
  ZigbeeScenesCluster.h - ZCL Scenes cluster (0x0005): scene store + command
  tooling.

  A scene captures a snapshot of a device's state (here On/Off + Level) under a
  (group, scene) id pair, so a controller can later Recall it - "movie mode" =
  lights at 20%. The Scenes cluster manages those: Store the current state,
  Recall it, View/Remove/Remove-All, and Get Scene Membership for a group.

  This header is the device-side store (ZigbeeSceneTable) plus the command
  payload builders and a server handler that applies an incoming command to the
  table, capturing/returning the device's current On/Off + Level. Scenes are
  group-scoped, so it pairs with ZigbeeGroupTable / ZigbeeGroupsCluster.
*/
#ifndef NIUS_ZIGBEE_SCENES_CLUSTER_H
#define NIUS_ZIGBEE_SCENES_CLUSTER_H

#include <Arduino.h>

namespace nzb {

enum ZclScenesCommand : uint8_t {
  SCENES_CMD_ADD = 0x00,
  SCENES_CMD_VIEW = 0x01,
  SCENES_CMD_REMOVE = 0x02,
  SCENES_CMD_REMOVE_ALL = 0x03,
  SCENES_CMD_STORE = 0x04,
  SCENES_CMD_RECALL = 0x05,
  SCENES_CMD_GET_MEMBERSHIP = 0x06,
};

enum ZclScenesStatus : uint8_t {
  SCENES_STATUS_SUCCESS = 0x00,
  SCENES_STATUS_INSUFFICIENT_SPACE = 0x89,
  SCENES_STATUS_NOT_FOUND = 0x8B,
};

struct SceneEntry {
  bool used;
  uint16_t groupId;
  uint8_t sceneId;
  bool onOff;   // captured On/Off cluster state
  uint8_t level;  // captured Level Control CurrentLevel
  SceneEntry() : used(false), groupId(0), sceneId(0), onOff(false), level(0) {}
};

// Device-side scene store: a snapshot (On/Off + Level) per (group, scene).
class ZigbeeSceneTable {
 public:
  ZigbeeSceneTable() : entries_(nullptr), capacity_(0) {}
  ZigbeeSceneTable(SceneEntry* storage, uint8_t capacity) { begin(storage, capacity); }
  void begin(SceneEntry* storage, uint8_t capacity) {
    entries_ = storage;
    capacity_ = capacity;
    for (uint8_t i = 0; i < capacity_; ++i) entries_[i] = SceneEntry();
  }

  uint8_t capacity() const { return capacity_; }

  /** Store (capture) On/Off + Level under (group, scene). Replaces an existing
      scene with the same ids. @return false if the table is full and it is new. */
  bool store(uint16_t group, uint8_t scene, bool onOff, uint8_t level) {
    SceneEntry* e = find(group, scene);
    if (!e) e = firstFree();
    if (!e) return false;
    e->used = true;
    e->groupId = group;
    e->sceneId = scene;
    e->onOff = onOff;
    e->level = level;
    return true;
  }

  /** Recall (read back) a stored scene's snapshot. @return false if not found. */
  bool recall(uint16_t group, uint8_t scene, bool& onOff, uint8_t& level) {
    SceneEntry* e = find(group, scene);
    if (!e) return false;
    onOff = e->onOff;
    level = e->level;
    return true;
  }

  bool has(uint16_t group, uint8_t scene) { return find(group, scene) != nullptr; }

  bool remove(uint16_t group, uint8_t scene) {
    SceneEntry* e = find(group, scene);
    if (!e) return false;
    *e = SceneEntry();
    return true;
  }

  uint8_t removeAllForGroup(uint16_t group) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < capacity_; ++i)
      if (entries_[i].used && entries_[i].groupId == group) {
        entries_[i] = SceneEntry();
        ++n;
      }
    return n;
  }

  /** Fill @p out with the scene ids stored for @p group. @return how many. */
  uint8_t scenesForGroup(uint16_t group, uint8_t* out, uint8_t outMax) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < capacity_ && n < outMax; ++i)
      if (entries_[i].used && entries_[i].groupId == group) out[n++] = entries_[i].sceneId;
    return n;
  }

  uint8_t count() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < capacity_; ++i) if (entries_[i].used) ++n;
    return n;
  }

 private:
  SceneEntry* find(uint16_t group, uint8_t scene) {
    if (!entries_) return nullptr;
    for (uint8_t i = 0; i < capacity_; ++i)
      if (entries_[i].used && entries_[i].groupId == group &&
          entries_[i].sceneId == scene)
        return &entries_[i];
    return nullptr;
  }
  SceneEntry* firstFree() {
    for (uint8_t i = 0; i < capacity_; ++i) if (!entries_[i].used) return &entries_[i];
    return nullptr;
  }
  SceneEntry* entries_;
  uint8_t capacity_;
};

class ZigbeeScenesCluster {
 public:
  /** Build a Store / Recall / Remove / View payload: group id(2) + scene id(1). */
  static uint8_t buildGroupScene(uint8_t* out, uint8_t outMax, uint16_t group,
                                 uint8_t scene) {
    if (!out || outMax < 3) return 0;
    out[0] = (uint8_t)(group & 0xFF);
    out[1] = (uint8_t)(group >> 8);
    out[2] = scene;
    return 3;
  }
  /** Build a Remove All Scenes / Get Scene Membership payload: group id(2). */
  static uint8_t buildGroup(uint8_t* out, uint8_t outMax, uint16_t group) {
    if (!out || outMax < 2) return 0;
    out[0] = (uint8_t)(group & 0xFF);
    out[1] = (uint8_t)(group >> 8);
    return 2;
  }

  /** Apply a received Scenes command to @p table. Store captures the device's
      current @p onOff / @p level; Recall writes them back from the stored scene
      (the caller then applies them to the device). @p status gets the ZCL
      status. @return true if the command was understood. */
  static bool handle(ZigbeeSceneTable& table, uint8_t commandId,
                     const uint8_t* payload, uint8_t payloadLen, bool& onOff,
                     uint8_t& level, uint8_t& status) {
    status = SCENES_STATUS_SUCCESS;
    switch (commandId) {
      case SCENES_CMD_STORE: {
        if (payloadLen < 3) return false;
        uint16_t g = (uint16_t)(payload[0] | (payload[1] << 8));
        if (!table.store(g, payload[2], onOff, level))
          status = SCENES_STATUS_INSUFFICIENT_SPACE;
        return true;
      }
      case SCENES_CMD_RECALL: {
        if (payloadLen < 3) return false;
        uint16_t g = (uint16_t)(payload[0] | (payload[1] << 8));
        if (!table.recall(g, payload[2], onOff, level))
          status = SCENES_STATUS_NOT_FOUND;
        return true;
      }
      case SCENES_CMD_REMOVE: {
        if (payloadLen < 3) return false;
        uint16_t g = (uint16_t)(payload[0] | (payload[1] << 8));
        if (!table.remove(g, payload[2])) status = SCENES_STATUS_NOT_FOUND;
        return true;
      }
      case SCENES_CMD_REMOVE_ALL: {
        if (payloadLen < 2) return false;
        uint16_t g = (uint16_t)(payload[0] | (payload[1] << 8));
        table.removeAllForGroup(g);
        return true;
      }
      default:
        return false;
    }
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_SCENES_CLUSTER_H
