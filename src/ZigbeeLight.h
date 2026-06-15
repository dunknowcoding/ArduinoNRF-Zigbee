/*
  ZigbeeLight.h - a high-level Zigbee light device tying the clusters together.

  Building an actual Zigbee device means wiring up a handful of clusters and
  their state. ZigbeeLight does that for a (color, dimmable) light: it owns the
  On/Off, Level, and Color state, an Identify countdown, and group + scene
  tables, and dispatches a received cluster-specific ZCL command to the right
  cluster behavior, updating its state. A sketch then just feeds incoming ZCL
  frames to handleCommand() and reads isOn()/level()/color() to drive the LED -
  the clusters and their interactions (e.g. Scenes capturing On/Off + Level) are
  handled here.

  This is the device-layer convenience on top of the per-cluster helpers; a
  switch, sensor, etc. would compose the same building blocks differently.
*/
#ifndef NIUS_ZIGBEE_LIGHT_H
#define NIUS_ZIGBEE_LIGHT_H

#include <Arduino.h>

#include "ZigbeeZcl.h"
#include "ZigbeeLevelControlCluster.h"
#include "ZigbeeColorControlCluster.h"
#include "ZigbeeIdentifyCluster.h"
#include "ZigbeeGroupTable.h"
#include "ZigbeeGroupsCluster.h"
#include "ZigbeeScenesCluster.h"

namespace nzb {

class ZigbeeLight {
 public:
  ZigbeeLight() : on_(false), level_(254), identifyTime_(0) {
    groups_.begin(groupStore_, kGroups);
    scenes_.begin(sceneStore_, kScenes);
  }

  // ---- state, for the sketch to drive hardware ----
  bool isOn() const { return on_; }
  uint8_t level() const { return level_; }            // 0..254
  const ColorState& color() const { return color_; }
  bool isIdentifying() const { return identifyTime_ > 0; }
  uint16_t identifyTime() const { return identifyTime_; }
  ZigbeeGroupTable& groups() { return groups_; }
  ZigbeeSceneTable& scenes() { return scenes_; }

  void setOn(bool on) { on_ = on; }
  void setLevel(uint8_t level) { level_ = level > 254 ? 254 : level; }

  /** Count the Identify timer down (call ~once per second). */
  void tickIdentify() { ZigbeeIdentifyCluster::tick(identifyTime_); }

  /** Dispatch a received cluster-specific ZCL command to the right cluster and
      update state. @return true if the command was handled (state may have
      changed); false for an unknown cluster/command. */
  bool handleCommand(uint16_t clusterId, uint8_t commandId,
                     const uint8_t* payload, uint8_t payloadLen) {
    switch (clusterId) {
      case ZigbeeZcl::kClusterOnOff:
        return ZigbeeZcl::applyOnOffCommand(commandId, on_);
      case ZigbeeZcl::kClusterLevelControl:
        return ZigbeeLevelControlCluster::applyCommand(commandId, payload,
                                                       payloadLen, level_);
      case ZigbeeZcl::kClusterColorControl:
        return ZigbeeColorControlCluster::applyCommand(commandId, payload,
                                                       payloadLen, color_);
      case ZigbeeZcl::kClusterIdentify:
        if (commandId == IDENTIFY_CMD_IDENTIFY) {
          ZigbeeIdentifyCluster::applyIdentify(payload, payloadLen, identifyTime_);
          return true;
        }
        return false;
      case ZigbeeZcl::kClusterGroups: {
        uint8_t resp[24], rc = 0;
        ZigbeeGroupsCluster::handle(groups_, commandId, payload, payloadLen, rc,
                                    resp, sizeof(resp));
        return true;
      }
      case ZigbeeZcl::kClusterScenes: {
        // Store captures the current On/Off + Level; Recall applies a stored
        // scene back into them.
        uint8_t status = 0;
        return ZigbeeScenesCluster::handle(scenes_, commandId, payload,
                                           payloadLen, on_, level_, status);
      }
      default:
        return false;
    }
  }

 private:
  static const uint8_t kGroups = 4;
  static const uint8_t kScenes = 4;

  bool on_;
  uint8_t level_;
  ColorState color_;
  uint16_t identifyTime_;
  ZigbeeGroupTable groups_;
  uint16_t groupStore_[kGroups];
  ZigbeeSceneTable scenes_;
  SceneEntry sceneStore_[kScenes];
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_LIGHT_H
