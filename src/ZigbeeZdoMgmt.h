/*
  ZigbeeZdoMgmt.h - ZDO network-management commands a gateway uses to run a
  Zigbee network.

  Builds/parses the ZDP commands beyond discovery and Mgmt_Lqi/Rtg (which live
  in ZigbeeZdo):
    * Mgmt_Permit_Joining (0x0036/0x8036) - open/close joining network-wide.
    * Mgmt_Leave (0x0034/0x8034) - tell a device to leave the network.
    * Node_Desc (0x0002/0x8002) - query a device's node descriptor (logical
      type, manufacturer code, buffer/transfer sizes, server mask).

  Every ZDP payload starts with the 1-byte transaction sequence number, then the
  command fields (this matches ZigbeeZdo's other builders).
*/
#ifndef NIUS_ZIGBEE_ZDO_MGMT_H
#define NIUS_ZIGBEE_ZDO_MGMT_H

#include <Arduino.h>

namespace nzb {

// Node descriptor logical device type (low 3 bits of byte 0).
enum ZdoLogicalType : uint8_t {
  ZDO_TYPE_COORDINATOR = 0,
  ZDO_TYPE_ROUTER = 1,
  ZDO_TYPE_END_DEVICE = 2,
};

struct ZdoNodeDescriptor {
  uint8_t logicalType;             // ZdoLogicalType
  uint8_t macCapabilityFlags;
  uint16_t manufacturerCode;
  uint8_t maxBufferSize;
  uint16_t maxIncomingTransferSize;
  uint16_t serverMask;
  uint16_t maxOutgoingTransferSize;
  uint8_t descriptorCapability;

  ZdoNodeDescriptor()
      : logicalType(ZDO_TYPE_ROUTER), macCapabilityFlags(0),
        manufacturerCode(0), maxBufferSize(0), maxIncomingTransferSize(0),
        serverMask(0), maxOutgoingTransferSize(0), descriptorCapability(0) {}
};

class ZigbeeZdoMgmt {
 public:
  // ---------------------------------------------- Mgmt_Permit_Joining (0x0036)
  /** seq(1) + permit duration seconds(1) + TC significance(1). duration 0 closes
      joining, 0xFF opens it indefinitely. */
  static uint8_t buildPermitJoiningRequest(uint8_t* out, uint8_t outMax,
                                           uint8_t sequence, uint8_t duration,
                                           bool tcSignificance) {
    if (!out || outMax < 3) return 0;
    out[0] = sequence;
    out[1] = duration;
    out[2] = tcSignificance ? 1 : 0;
    return 3;
  }
  /** Response: seq(1) + status(1). */
  static uint8_t buildStatusResponse(uint8_t* out, uint8_t outMax,
                                     uint8_t sequence, uint8_t status) {
    if (!out || outMax < 2) return 0;
    out[0] = sequence;
    out[1] = status;
    return 2;
  }
  static bool parseStatusResponse(const uint8_t* payload, uint8_t len,
                                  uint8_t& sequence, uint8_t& status) {
    if (!payload || len < 2) return false;
    sequence = payload[0];
    status = payload[1];
    return true;
  }

  // ----------------------------------------------------- Mgmt_Leave (0x0034)
  /** seq(1) + device IEEE(8) + flags(1). flags bit 6 = remove children, bit 7 =
      rejoin. The device address is the target to remove (0 = self). */
  static uint8_t buildLeaveRequest(uint8_t* out, uint8_t outMax,
                                   uint8_t sequence, uint64_t deviceIeee,
                                   bool removeChildren, bool rejoin) {
    if (!out || outMax < 10) return 0;
    out[0] = sequence;
    putLe64(&out[1], deviceIeee);
    out[9] = (uint8_t)((removeChildren ? 0x40 : 0) | (rejoin ? 0x80 : 0));
    return 10;
  }
  static bool parseLeaveRequest(const uint8_t* payload, uint8_t len,
                                uint8_t& sequence, uint64_t& deviceIeee,
                                bool& removeChildren, bool& rejoin) {
    if (!payload || len < 10) return false;
    sequence = payload[0];
    deviceIeee = getLe64(&payload[1]);
    removeChildren = (payload[9] & 0x40) != 0;
    rejoin = (payload[9] & 0x80) != 0;
    return true;
  }

  // ----------------------------------------------------- Node_Desc (0x0002)
  /** seq(1) + NWK address of interest(2). */
  static uint8_t buildNodeDescRequest(uint8_t* out, uint8_t outMax,
                                      uint8_t sequence, uint16_t nwkAddr) {
    if (!out || outMax < 3) return 0;
    out[0] = sequence;
    putLe16(&out[1], nwkAddr);
    return 3;
  }
  static bool parseNodeDescRequest(const uint8_t* payload, uint8_t len,
                                   uint8_t& sequence, uint16_t& nwkAddr) {
    if (!payload || len < 3) return false;
    sequence = payload[0];
    nwkAddr = getLe16(&payload[1]);
    return true;
  }

  /** Node_Desc_rsp: seq(1) + status(1) + NWK addr(2) + node descriptor(13). */
  static uint8_t buildNodeDescResponse(uint8_t* out, uint8_t outMax,
                                       uint8_t sequence, uint8_t status,
                                       uint16_t nwkAddr,
                                       const ZdoNodeDescriptor& d) {
    if (!out || outMax < 17) return 0;
    out[0] = sequence;
    out[1] = status;
    putLe16(&out[2], nwkAddr);
    // 13-byte node descriptor.
    out[4] = (uint8_t)(d.logicalType & 0x07);
    out[5] = 0;  // APS flags + frequency band (unset here)
    out[6] = d.macCapabilityFlags;
    putLe16(&out[7], d.manufacturerCode);
    out[9] = d.maxBufferSize;
    putLe16(&out[10], d.maxIncomingTransferSize);
    putLe16(&out[12], d.serverMask);
    putLe16(&out[14], d.maxOutgoingTransferSize);
    out[16] = d.descriptorCapability;
    return 17;
  }
  static bool parseNodeDescResponse(const uint8_t* payload, uint8_t len,
                                    uint8_t& sequence, uint8_t& status,
                                    uint16_t& nwkAddr, ZdoNodeDescriptor& d) {
    d = ZdoNodeDescriptor();
    if (!payload || len < 17) return false;
    sequence = payload[0];
    status = payload[1];
    nwkAddr = getLe16(&payload[2]);
    d.logicalType = (uint8_t)(payload[4] & 0x07);
    d.macCapabilityFlags = payload[6];
    d.manufacturerCode = getLe16(&payload[7]);
    d.maxBufferSize = payload[9];
    d.maxIncomingTransferSize = getLe16(&payload[10]);
    d.serverMask = getLe16(&payload[12]);
    d.maxOutgoingTransferSize = getLe16(&payload[14]);
    d.descriptorCapability = payload[16];
    return true;
  }

 private:
  static void putLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8);
  }
  static uint16_t getLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  }
  static void putLe64(uint8_t* p, uint64_t v) {
    for (uint8_t i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (8 * i));
  }
  static uint64_t getLe64(const uint8_t* p) {
    uint64_t v = 0;
    for (uint8_t i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (8 * i);
    return v;
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_ZDO_MGMT_H
