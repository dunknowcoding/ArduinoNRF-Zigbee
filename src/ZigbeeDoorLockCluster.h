/*
  ZigbeeDoorLockCluster.h - ZCL Door Lock cluster (0x0101).

  A door lock exposes LockState (locked / unlocked / not-fully-locked) and obeys
  Lock Door / Unlock Door / Toggle commands. The lock answers each command with
  a command-specific response carrying a status byte. This header applies the
  commands to a local LockState, builds the command + response payloads, and
  builds Read Attributes Response records for the lock attributes - the
  actuator counterpart to the read-only measurement clusters.
*/
#ifndef NIUS_ZIGBEE_DOOR_LOCK_CLUSTER_H
#define NIUS_ZIGBEE_DOOR_LOCK_CLUSTER_H

#include <Arduino.h>
#include "ZigbeeZcl.h"

namespace nzb {

enum ZclDoorLockCommandId : uint8_t {
  DOOR_LOCK_CMD_LOCK = 0x00,
  DOOR_LOCK_CMD_UNLOCK = 0x01,
  DOOR_LOCK_CMD_TOGGLE = 0x02,
};

enum ZclDoorLockState : uint8_t {
  DOOR_LOCK_STATE_NOT_FULLY_LOCKED = 0x00,
  DOOR_LOCK_STATE_LOCKED = 0x01,
  DOOR_LOCK_STATE_UNLOCKED = 0x02,
};

enum ZclDoorState : uint8_t {
  DOOR_STATE_OPEN = 0x00,
  DOOR_STATE_CLOSED = 0x01,
  DOOR_STATE_ERROR_JAMMED = 0x02,
};

class ZigbeeDoorLockCluster {
 public:
  static const uint16_t kClusterId = 0x0101;
  static const uint16_t kAttrLockState = 0x0000;        // enum8
  static const uint16_t kAttrLockType = 0x0001;         // enum8
  static const uint16_t kAttrActuatorEnabled = 0x0002;  // bool
  static const uint16_t kAttrDoorState = 0x0003;        // enum8

  ZigbeeDoorLockCluster()
      : lockState_(DOOR_LOCK_STATE_LOCKED),
        doorState_(DOOR_STATE_CLOSED),
        actuatorEnabled_(true) {}

  uint8_t lockState() const { return lockState_; }
  bool isLocked() const { return lockState_ == DOOR_LOCK_STATE_LOCKED; }
  void setActuatorEnabled(bool on) { actuatorEnabled_ = on; }
  void setDoorState(uint8_t s) { doorState_ = s; }

  /** Apply a Lock/Unlock/Toggle command. @return true if the command is one we
      handle (then @p outStatus is SUCCESS, or FAILURE if the actuator is
      disabled); false for an unknown command id. */
  bool applyCommand(uint8_t commandId, uint8_t& outStatus) {
    if (!actuatorEnabled_ &&
        (commandId == DOOR_LOCK_CMD_LOCK || commandId == DOOR_LOCK_CMD_UNLOCK ||
         commandId == DOOR_LOCK_CMD_TOGGLE)) {
      outStatus = ZCL_STATUS_FAILURE;
      return true;
    }
    switch (commandId) {
      case DOOR_LOCK_CMD_LOCK:
        lockState_ = DOOR_LOCK_STATE_LOCKED;
        outStatus = ZCL_STATUS_SUCCESS;
        return true;
      case DOOR_LOCK_CMD_UNLOCK:
        lockState_ = DOOR_LOCK_STATE_UNLOCKED;
        outStatus = ZCL_STATUS_SUCCESS;
        return true;
      case DOOR_LOCK_CMD_TOGGLE:
        lockState_ = isLocked() ? DOOR_LOCK_STATE_UNLOCKED
                                : DOOR_LOCK_STATE_LOCKED;
        outStatus = ZCL_STATUS_SUCCESS;
        return true;
      default:
        return false;
    }
  }

  /** Door Lock command response payload: a single status byte. The response
      command id mirrors the request command id (Lock Door Response = 0x00, ...). */
  static uint8_t buildCommandResponse(uint8_t* out, uint8_t outMax,
                                      uint8_t status) {
    if (!out || outMax < 1) return 0;
    out[0] = status;
    return 1;
  }

  uint8_t appendReadAttributeRecord(uint16_t attrId, uint8_t* out,
                                    uint8_t outMax) const {
    switch (attrId) {
      case kAttrLockState:
        return ZigbeeZcl::buildTyped8AttributeRecord(out, outMax, attrId,
                                                     ZCL_TYPE_ENUM8, lockState_);
      case kAttrDoorState:
        return ZigbeeZcl::buildTyped8AttributeRecord(out, outMax, attrId,
                                                     ZCL_TYPE_ENUM8, doorState_);
      case kAttrActuatorEnabled:
        return ZigbeeZcl::buildBoolAttributeRecord(out, outMax, attrId,
                                                   actuatorEnabled_);
      default:
        return ZigbeeZcl::buildAttributeStatusRecord(
            out, outMax, attrId, ZCL_STATUS_UNSUPPORTED_ATTRIBUTE);
    }
  }

  uint8_t buildReadAttributesResponsePayload(const uint8_t* reqPayload,
                                             uint8_t reqLen, uint8_t* out,
                                             uint8_t outMax) const {
    if (!out || !reqPayload) return 0;
    uint8_t len = 0;
    for (uint8_t i = 0;; ++i) {
      uint16_t attrId = 0;
      if (!ZigbeeZcl::getReadAttributeId(reqPayload, reqLen, i, attrId)) break;
      uint8_t added = appendReadAttributeRecord(attrId, &out[len],
                                                (uint8_t)(outMax - len));
      if (added == 0) return 0;
      len += added;
    }
    return len;
  }

  /** Report Attributes payload for LockState (enum8). */
  uint8_t buildReport(uint8_t* out, uint8_t outMax) const {
    if (!out || outMax < 4) return 0;
    out[0] = (uint8_t)(kAttrLockState & 0xFF);
    out[1] = (uint8_t)(kAttrLockState >> 8);
    out[2] = ZCL_TYPE_ENUM8;
    out[3] = lockState_;
    return 4;
  }

 private:
  uint8_t lockState_;
  uint8_t doorState_;
  bool actuatorEnabled_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_DOOR_LOCK_CLUSTER_H
