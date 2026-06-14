/*
  ZigbeeGroupsCluster.h - ZCL Groups cluster (0x0004) command tooling +
  server behavior.

  The Groups cluster is how a coordinator/app manages a device's group
  membership remotely: Add Group, Remove Group, Remove All Groups, and Get Group
  Membership. This header builds the cluster-specific command payloads (the
  ZCL header itself comes from ZigbeeZcl::buildCommandFrame) and provides a
  server handler that applies an incoming command to a ZigbeeGroupTable and
  produces the matching response payload. Together with the group-addressed APS
  frame (ZigbeeAps::buildGroupDataFrame) this is end-to-end group control.
*/
#ifndef NIUS_ZIGBEE_GROUPS_CLUSTER_H
#define NIUS_ZIGBEE_GROUPS_CLUSTER_H

#include <Arduino.h>

#include "ZigbeeGroupTable.h"

namespace nzb {

enum ZclGroupsCommand : uint8_t {
  GROUPS_CMD_ADD = 0x00,
  GROUPS_CMD_VIEW = 0x01,
  GROUPS_CMD_GET_MEMBERSHIP = 0x02,
  GROUPS_CMD_REMOVE = 0x03,
  GROUPS_CMD_REMOVE_ALL = 0x04,
  GROUPS_CMD_ADD_IF_IDENTIFYING = 0x05,
};

// ZCL status codes used by the Groups responses.
enum ZclGroupsStatus : uint8_t {
  GROUPS_STATUS_SUCCESS = 0x00,
  GROUPS_STATUS_INSUFFICIENT_SPACE = 0x89,
  GROUPS_STATUS_DUPLICATE_EXISTS = 0x8A,
  GROUPS_STATUS_NOT_FOUND = 0x8B,
};

class ZigbeeGroupsCluster {
 public:
  // -------------------------------------------------------- request payloads
  /** Add Group: group id(2) + group name (ZCL string: length + chars; empty
      here). */
  static uint8_t buildAddGroup(uint8_t* out, uint8_t outMax, uint16_t groupId) {
    if (!out || outMax < 3) return 0;
    putLe16(out, groupId);
    out[2] = 0;  // zero-length group name
    return 3;
  }
  /** Remove Group / View Group: group id(2). */
  static uint8_t buildGroupId(uint8_t* out, uint8_t outMax, uint16_t groupId) {
    if (!out || outMax < 2) return 0;
    putLe16(out, groupId);
    return 2;
  }
  /** Get Group Membership: group count(1) + group list(2*n). count 0 = "all". */
  static uint8_t buildGetMembership(uint8_t* out, uint8_t outMax,
                                    const uint16_t* groups, uint8_t count) {
    uint8_t need = (uint8_t)(1 + count * 2);
    if (!out || outMax < need) return 0;
    out[0] = count;
    for (uint8_t i = 0; i < count; ++i) putLe16(&out[1 + i * 2], groups[i]);
    return need;
  }

  // ------------------------------------------------------- response payloads
  /** Add/Remove Group response: status(1) + group id(2). */
  static uint8_t buildStatusResponse(uint8_t* out, uint8_t outMax,
                                     uint8_t status, uint16_t groupId) {
    if (!out || outMax < 3) return 0;
    out[0] = status;
    putLe16(&out[1], groupId);
    return 3;
  }
  /** Get Group Membership response: capacity(1) + count(1) + group list(2*n). */
  static uint8_t buildMembershipResponse(uint8_t* out, uint8_t outMax,
                                         uint8_t capacity, const uint16_t* groups,
                                         uint8_t count) {
    uint8_t need = (uint8_t)(2 + count * 2);
    if (!out || outMax < need) return 0;
    out[0] = capacity;
    out[1] = count;
    for (uint8_t i = 0; i < count; ++i) putLe16(&out[2 + i * 2], groups[i]);
    return need;
  }

  // --------------------------------------------------------- server handler
  /** Apply a received Groups command to @p table and build its response.
      @param commandId the ZCL command id (ZclGroupsCommand).
      @param payload/payloadLen the command payload (after the ZCL header).
      @param respCmdId out: the response command id to send back (unchanged for
             commands that have no response, e.g. Remove All).
      @param resp/respMax the response payload buffer.
      @return response payload length, or 0 if the command has no response /
              is malformed. */
  static uint8_t handle(ZigbeeGroupTable& table, uint8_t commandId,
                        const uint8_t* payload, uint8_t payloadLen,
                        uint8_t& respCmdId, uint8_t* resp, uint8_t respMax) {
    switch (commandId) {
      case GROUPS_CMD_ADD:
      case GROUPS_CMD_ADD_IF_IDENTIFYING: {
        if (payloadLen < 2) return 0;
        uint16_t g = getLe16(payload);
        uint8_t status;
        if (table.isMember(g)) status = GROUPS_STATUS_DUPLICATE_EXISTS;
        else if (table.add(g)) status = GROUPS_STATUS_SUCCESS;
        else status = GROUPS_STATUS_INSUFFICIENT_SPACE;
        respCmdId = GROUPS_CMD_ADD;
        return buildStatusResponse(resp, respMax, status, g);
      }
      case GROUPS_CMD_REMOVE: {
        if (payloadLen < 2) return 0;
        uint16_t g = getLe16(payload);
        uint8_t status = table.remove(g) ? GROUPS_STATUS_SUCCESS
                                         : GROUPS_STATUS_NOT_FOUND;
        respCmdId = GROUPS_CMD_REMOVE;
        return buildStatusResponse(resp, respMax, status, g);
      }
      case GROUPS_CMD_REMOVE_ALL:
        table.clear();
        return 0;  // no response
      case GROUPS_CMD_GET_MEMBERSHIP: {
        // Respond with the groups we belong to (intersection if a list was
        // given; all of ours if the request count is 0).
        if (payloadLen < 1) return 0;
        uint8_t reqCount = payload[0];
        uint16_t mine[16];
        uint8_t n = 0;
        if (reqCount == 0) {
          for (uint8_t i = 0; i < table.count() && n < 16; ++i)
            mine[n++] = table.at(i);
        } else {
          for (uint8_t i = 0; i < reqCount; ++i) {
            uint8_t off = (uint8_t)(1 + i * 2);
            if ((uint8_t)(off + 2) > payloadLen) break;
            uint16_t g = getLe16(&payload[off]);
            if (table.isMember(g) && n < 16) mine[n++] = g;
          }
        }
        uint8_t capacity = (uint8_t)(table.capacity() - table.count());
        respCmdId = GROUPS_CMD_GET_MEMBERSHIP;
        return buildMembershipResponse(resp, respMax, capacity, mine, n);
      }
      default:
        return 0;
    }
  }

 private:
  static void putLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
  }
  static uint16_t getLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_GROUPS_CLUSTER_H
