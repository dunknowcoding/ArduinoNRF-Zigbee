/*
  ZigbeeMac.h - small IEEE 802.15.4 MAC helpers used by NiusZigbee.

  This is intentionally the layer below a full Zigbee PRO stack. It builds and
  parses short-address data frames (PAN ID + 16-bit source/destination address),
  which is enough for structured CC2530-to-CC2530 links and gives future NWK /
  APS / ZCL code a real MAC envelope to sit on.
*/
#ifndef NIUS_ZIGBEE_MAC_H
#define NIUS_ZIGBEE_MAC_H

#include <Arduino.h>

namespace nzb {

enum MacFrameType : uint8_t {
  MAC_FRAME_BEACON = 0,
  MAC_FRAME_DATA = 1,
  MAC_FRAME_ACK = 2,
  MAC_FRAME_COMMAND = 3
};

struct MacDataFrame {
  bool valid;
  uint8_t frameType;
  uint8_t sequence;
  bool ackRequest;
  bool panIdCompression;
  uint16_t dstPanId;
  uint16_t dstShort;
  uint16_t srcPanId;
  uint16_t srcShort;
  const uint8_t* payload;
  uint8_t payloadLen;
};

enum MacAddressMode : uint8_t {
  MAC_ADDR_NONE = 0,
  MAC_ADDR_SHORT = 2,
  MAC_ADDR_EXTENDED = 3
};

enum MacCommandId : uint8_t {
  MAC_CMD_ASSOCIATION_REQUEST = 0x01,
  MAC_CMD_ASSOCIATION_RESPONSE = 0x02,
  MAC_CMD_BEACON_REQUEST = 0x07
};

enum MacAssociationStatus : uint8_t {
  MAC_ASSOC_SUCCESS = 0x00,
  MAC_ASSOC_PAN_AT_CAPACITY = 0x01,
  MAC_ASSOC_PAN_ACCESS_DENIED = 0x02
};

struct MacCommandFrame {
  bool valid;
  uint8_t frameType;
  uint8_t sequence;
  bool ackRequest;
  bool panIdCompression;
  uint8_t dstAddrMode;
  uint8_t srcAddrMode;
  uint16_t dstPanId;
  uint16_t dstShort;
  uint64_t dstIeee;
  uint16_t srcPanId;
  uint16_t srcShort;
  uint64_t srcIeee;
  uint8_t commandId;
  const uint8_t* payload;
  uint8_t payloadLen;
};

struct MacAssociationRequest {
  bool valid;
  uint8_t capability;
  bool allocateAddress;
  bool receiverOnWhenIdle;
  bool fullFunctionDevice;
};

struct MacAssociationResponse {
  bool valid;
  uint16_t shortAddress;
  uint8_t status;
};

/** Parsed IEEE 802.15.4 beacon frame (beacon-enabled fields are not used by
    Zigbee, so only the beaconless-PAN subset is exposed). */
struct MacBeaconFrame {
  bool valid;
  uint8_t sequence;
  uint16_t srcPanId;
  uint16_t srcShort;
  bool panCoordinator;
  bool associationPermit;
  const uint8_t* payload;  ///< beacon payload (Zigbee NWK layer payload)
  uint8_t payloadLen;
};

class ZigbeeMac {
 public:
  static const uint8_t kMaxPsdu = 125;
  static const uint16_t kBroadcastPan = 0xFFFF;
  static const uint16_t kBroadcastShort = 0xFFFF;

  static uint8_t buildShortDataFrame(uint8_t* out, uint8_t outMax,
                                     uint16_t panId, uint16_t dstShort,
                                     uint16_t srcShort, uint8_t sequence,
                                     const uint8_t* payload,
                                     uint8_t payloadLen,
                                     bool ackRequest = false);

  static bool parseShortDataFrame(const uint8_t* psdu, uint8_t len,
                                  MacDataFrame& frame);

  static uint8_t buildAssociationRequest(uint8_t* out, uint8_t outMax,
                                         uint16_t panId, uint16_t coordShort,
                                         uint64_t srcIeee, uint8_t sequence,
                                         uint8_t capability,
                                         bool ackRequest = true);

  static uint8_t buildAssociationResponse(uint8_t* out, uint8_t outMax,
                                          uint16_t panId, uint64_t dstIeee,
                                          uint16_t srcShort, uint8_t sequence,
                                          uint16_t assignedShort,
                                          uint8_t status,
                                          bool ackRequest = true);

  static bool parseCommandFrame(const uint8_t* psdu, uint8_t len,
                                MacCommandFrame& frame);
  static bool parseAssociationRequest(const MacCommandFrame& frame,
                                      MacAssociationRequest& request);
  static bool parseAssociationResponse(const MacCommandFrame& frame,
                                       MacAssociationResponse& response);

  /** Build a broadcast Beacon Request MAC command (active scan probe). */
  static uint8_t buildBeaconRequest(uint8_t* out, uint8_t outMax,
                                    uint8_t sequence);

  /** Build a beaconless-PAN 802.15.4 beacon frame carrying @p payload. */
  static uint8_t buildBeacon(uint8_t* out, uint8_t outMax, uint16_t srcPanId,
                             uint16_t srcShort, uint8_t sequence,
                             bool panCoordinator, bool associationPermit,
                             const uint8_t* payload, uint8_t payloadLen);

  /** Parse a received beacon frame (PSDU without FCS). */
  static bool parseBeacon(const uint8_t* psdu, uint8_t len,
                          MacBeaconFrame& frame);

  static bool isBroadcastShort(uint16_t shortAddress) {
    return shortAddress == kBroadcastShort;
  }

 private:
  static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  }
  static void writeLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
  }
  static uint64_t readLe64(const uint8_t* p);
  static void writeLe64(uint8_t* p, uint64_t v);
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_MAC_H
