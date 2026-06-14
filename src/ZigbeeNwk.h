/*
  ZigbeeNwk.h - small Zigbee NWK frame helpers used by NiusZigbee.

  This is frame tooling, not a full Zigbee PRO network stack. It builds and
  parses simple NWK data frames that fit inside the MAC data-frame payload
  produced by ZigbeeMac.
*/
#ifndef NIUS_ZIGBEE_NWK_H
#define NIUS_ZIGBEE_NWK_H

#include <Arduino.h>

namespace nzb {

enum NwkFrameType : uint8_t {
  NWK_FRAME_DATA = 0,
  NWK_FRAME_COMMAND = 1,
  NWK_FRAME_INTERPAN = 3
};

enum NwkDiscoverRoute : uint8_t {
  NWK_DISCOVER_SUPPRESS = 0,
  NWK_DISCOVER_ENABLE = 1
};

// What a node should do with a received source-routed data frame.
enum NwkSourceRouteAction : uint8_t {
  NWK_SR_DELIVER = 0,  // this node is the destination - hand to the app
  NWK_SR_RELAY = 1,    // forward to nextHop with the updated relay index
  NWK_SR_DROP = 2,     // malformed, or this node is not on the path
};

struct NwkDataFrame {
  bool valid;
  uint8_t frameType;
  uint8_t protocolVersion;
  uint8_t discoverRoute;
  bool multicast;
  bool security;
  bool sourceRoute;
  bool dstIeeePresent;
  bool srcIeeePresent;
  bool endDeviceInitiator;
  uint16_t dstShort;
  uint16_t srcShort;
  uint8_t radius;
  uint8_t sequence;
  // Source-route subframe (valid only when sourceRoute is set): the ordered
  // relay list the frame is to follow, and the index of the next relay.
  uint8_t srRelayCount;
  uint8_t srRelayIndex;
  const uint8_t* srRelayList;  // srRelayCount little-endian 16-bit relays
  const uint8_t* payload;
  uint8_t payloadLen;
};

struct NwkCommandFrame {
  bool valid;
  uint8_t frameType;
  uint8_t protocolVersion;
  uint8_t discoverRoute;
  bool multicast;
  bool security;
  bool sourceRoute;
  bool dstIeeePresent;
  bool srcIeeePresent;
  bool endDeviceInitiator;
  uint16_t dstShort;
  uint16_t srcShort;
  uint8_t radius;
  uint8_t sequence;
  uint8_t commandId;
  const uint8_t* payload;
  uint8_t payloadLen;
};

enum NwkCommandId : uint8_t {
  NWK_CMD_ROUTE_REQUEST = 0x01,
  NWK_CMD_ROUTE_REPLY = 0x02,
  NWK_CMD_NETWORK_STATUS = 0x03,
  NWK_CMD_LEAVE = 0x04,
  NWK_CMD_ROUTE_RECORD = 0x05,
  NWK_CMD_REJOIN_REQUEST = 0x06,
  NWK_CMD_REJOIN_RESPONSE = 0x07,
  NWK_CMD_LINK_STATUS = 0x08,
  NWK_CMD_NETWORK_REPORT = 0x09,
  NWK_CMD_NETWORK_UPDATE = 0x0A,
  NWK_CMD_ED_TIMEOUT_REQUEST = 0x0B,
  NWK_CMD_ED_TIMEOUT_RESPONSE = 0x0C
};

enum NwkNetworkStatusCode : uint8_t {
  NWK_STATUS_NO_ROUTE_AVAILABLE = 0x00,
  NWK_STATUS_TREE_LINK_FAILURE = 0x01,
  NWK_STATUS_NON_TREE_LINK_FAILURE = 0x02,
  NWK_STATUS_LOW_BATTERY_LEVEL = 0x03,
  NWK_STATUS_NO_ROUTING_CAPACITY = 0x04,
  NWK_STATUS_NO_INDIRECT_CAPACITY = 0x05,
  NWK_STATUS_INDIRECT_TRANSACTION_EXPIRY = 0x06,
  NWK_STATUS_TARGET_DEVICE_UNAVAILABLE = 0x07,
  NWK_STATUS_TARGET_ADDRESS_UNALLOCATED = 0x08,
  NWK_STATUS_PARENT_LINK_FAILURE = 0x09,
  NWK_STATUS_VALIDATE_ROUTE = 0x0A,
  NWK_STATUS_SOURCE_ROUTE_FAILURE = 0x0B,
  NWK_STATUS_MANY_TO_ONE_ROUTE_FAILURE = 0x0C,
  NWK_STATUS_ADDRESS_CONFLICT = 0x0D,
  NWK_STATUS_VERIFY_ADDRESSES = 0x0E,
  NWK_STATUS_PAN_IDENTIFIER_UPDATE = 0x0F,
  NWK_STATUS_NETWORK_ADDRESS_UPDATE = 0x10,
  NWK_STATUS_BAD_FRAME_COUNTER = 0x11,
  NWK_STATUS_BAD_KEY_SEQUENCE_NUMBER = 0x12
};

struct NwkRouteRequestCommand {
  bool valid;
  bool manyToOne;
  bool destinationIeeePresent;
  bool multicast;
  uint8_t routeRequestId;
  uint16_t destination;
  uint8_t pathCost;
  uint64_t destinationIeee;
};

struct NwkRouteReplyCommand {
  bool valid;
  bool originatorIeeePresent;
  bool responderIeeePresent;
  bool multicast;
  uint8_t routeRequestId;
  uint16_t originator;
  uint16_t responder;
  uint8_t pathCost;
  uint64_t originatorIeee;
  uint64_t responderIeee;
};

struct NwkNetworkStatusCommand {
  bool valid;
  uint8_t status;
  uint16_t destination;
};

struct NwkRouteRecordCommand {
  bool valid;
  uint8_t relayCount;
  const uint8_t* relayList;
};

struct NwkLeaveCommand {
  bool valid;
  bool rejoin;
  bool request;
  bool removeChildren;
};

struct NwkRejoinRequestCommand {
  bool valid;
  uint8_t capability;
};

struct NwkRejoinResponseCommand {
  bool valid;
  uint16_t nwkAddress;
  uint8_t status;
};

/** One neighbor entry inside a Link Status command. */
struct NwkLinkStatusEntry {
  uint16_t address;
  uint8_t incomingCost;  ///< 1 (best) .. 7 (worst), 0 = unknown
  uint8_t outgoingCost;
};

struct NwkLinkStatusCommand {
  bool valid;
  bool firstFrame;
  bool lastFrame;
  uint8_t entryCount;
  const uint8_t* entries;  ///< raw 3-byte records, use getLinkStatusEntry()
};

/** Zigbee NWK-layer beacon payload (the 15 bytes carried by 802.15.4
    beacons on a Zigbee PRO network). */
struct NwkBeaconPayload {
  bool valid;
  uint8_t protocolId;       ///< 0x00 = Zigbee
  uint8_t stackProfile;     ///< 0x02 = Zigbee PRO
  uint8_t protocolVersion;  ///< 0x02
  bool routerCapacity;      ///< parent can accept router children
  uint8_t deviceDepth;      ///< sender's tree depth (0 = coordinator)
  bool endDeviceCapacity;   ///< parent can accept end-device children
  uint64_t extendedPanId;
  uint32_t txOffset;        ///< 0xFFFFFF on beaconless PANs
  uint8_t updateId;         ///< nwkUpdateId
};

class ZigbeeNwk {
 public:
  static const uint8_t kProtocolVersion = 2;
  static const uint8_t kBaseHeaderLen = 8;
  static const uint8_t kMaxFrame = 116;
  static const uint8_t kMaxPayload = kMaxFrame - kBaseHeaderLen;
  static const uint8_t kDefaultRadius = 30;
  static const uint16_t kBroadcastRxOnWhenIdle = 0xFFFD;
  static const uint16_t kBroadcastAllRouters = 0xFFFC;
  static const uint8_t kBeaconPayloadLen = 15;
  static const uint8_t kProtocolIdZigbee = 0x00;
  static const uint8_t kStackProfilePro = 0x02;
  static const uint32_t kBeaconlessTxOffset = 0xFFFFFFUL;

  static uint8_t buildDataFrame(uint8_t* out, uint8_t outMax,
                                uint16_t dstShort, uint16_t srcShort,
                                uint8_t radius, uint8_t sequence,
                                const uint8_t* payload,
                                uint8_t payloadLen,
                                uint8_t discoverRoute = NWK_DISCOVER_SUPPRESS);

  static bool parseDataFrame(const uint8_t* npdu, uint8_t len,
                             NwkDataFrame& frame);

  static uint8_t buildCommandFrame(uint8_t* out, uint8_t outMax,
                                   uint16_t dstShort, uint16_t srcShort,
                                   uint8_t radius, uint8_t sequence,
                                   uint8_t commandId,
                                   const uint8_t* payload,
                                   uint8_t payloadLen,
                                   uint8_t discoverRoute = NWK_DISCOVER_SUPPRESS);

  static bool parseCommandFrame(const uint8_t* npdu, uint8_t len,
                                NwkCommandFrame& frame);

  static uint8_t buildRouteRequestPayload(uint8_t* out, uint8_t outMax,
                                          uint8_t routeRequestId,
                                          uint16_t destination,
                                          uint8_t pathCost = 0,
                                          bool manyToOne = false,
                                          const uint64_t* destinationIeee = nullptr,
                                          bool multicast = false);
  static bool parseRouteRequestPayload(const uint8_t* payload,
                                       uint8_t payloadLen,
                                       NwkRouteRequestCommand& command);

  static uint8_t buildRouteReplyPayload(uint8_t* out, uint8_t outMax,
                                        uint8_t routeRequestId,
                                        uint16_t originator,
                                        uint16_t responder,
                                        uint8_t pathCost,
                                        const uint64_t* originatorIeee = nullptr,
                                        const uint64_t* responderIeee = nullptr,
                                        bool multicast = false);
  static bool parseRouteReplyPayload(const uint8_t* payload,
                                     uint8_t payloadLen,
                                     NwkRouteReplyCommand& command);

  static uint8_t buildNetworkStatusPayload(uint8_t* out, uint8_t outMax,
                                           uint8_t status,
                                           uint16_t destination);
  static bool parseNetworkStatusPayload(const uint8_t* payload,
                                        uint8_t payloadLen,
                                        NwkNetworkStatusCommand& command);

  static uint8_t buildRouteRecordPayload(uint8_t* out, uint8_t outMax,
                                         const uint16_t* relays,
                                         uint8_t relayCount);
  static bool parseRouteRecordPayload(const uint8_t* payload,
                                      uint8_t payloadLen,
                                      NwkRouteRecordCommand& command);
  static bool getRouteRecordRelay(const NwkRouteRecordCommand& command,
                                  uint8_t index, uint16_t& relay);

  /** Build a source-routed NWK data frame: the base header (with the
      source-route FCF bit set) + the source-route subframe (relay count, relay
      index, ordered relay list source->destination) + the payload. @p relayIndex
      is the 0-based index of the relay expected to process the frame next; the
      originator sets it to 0 and sends to relays[0] (see sourceRouteAction for
      the forwarding rule). @return length, or 0 on error. */
  static uint8_t buildDataFrameSourceRouted(
      uint8_t* out, uint8_t outMax, uint16_t dstShort, uint16_t srcShort,
      uint8_t radius, uint8_t sequence, const uint16_t* relays,
      uint8_t relayCount, uint8_t relayIndex, const uint8_t* payload,
      uint8_t payloadLen, uint8_t discoverRoute = NWK_DISCOVER_SUPPRESS);

  /** Read the @p index-th relay of a parsed source-routed data frame. */
  static bool getDataFrameRelay(const NwkDataFrame& frame, uint8_t index,
                                uint16_t& relay);

  /** Decide how a parsed source-routed data frame received at @p selfShort
      should be handled. Convention: the relay list is ordered originator ->
      destination and `srRelayIndex` is the 0-based index of the relay expected
      to process the frame next (the originator sets it to 0, sending to
      relays[0]). On NWK_SR_RELAY, @p nextHop is the address to forward to (the
      next relay, or the destination if this was the last relay) and
      @p outRelayIndex is the relay index to write into the forwarded frame. */
  static uint8_t sourceRouteAction(const NwkDataFrame& frame, uint16_t selfShort,
                                   uint16_t& nextHop, uint8_t& outRelayIndex);

  static uint8_t buildLeavePayload(uint8_t* out, uint8_t outMax,
                                   bool rejoin, bool request,
                                   bool removeChildren);
  static bool parseLeavePayload(const uint8_t* payload, uint8_t payloadLen,
                                NwkLeaveCommand& command);

  static uint8_t buildRejoinRequestPayload(uint8_t* out, uint8_t outMax,
                                           uint8_t capability);
  static bool parseRejoinRequestPayload(const uint8_t* payload,
                                        uint8_t payloadLen,
                                        NwkRejoinRequestCommand& command);

  static uint8_t buildRejoinResponsePayload(uint8_t* out, uint8_t outMax,
                                            uint16_t nwkAddress,
                                            uint8_t status);
  static bool parseRejoinResponsePayload(const uint8_t* payload,
                                         uint8_t payloadLen,
                                         NwkRejoinResponseCommand& command);

  static uint8_t buildLinkStatusPayload(uint8_t* out, uint8_t outMax,
                                        const NwkLinkStatusEntry* entries,
                                        uint8_t entryCount,
                                        bool firstFrame = true,
                                        bool lastFrame = true);
  static bool parseLinkStatusPayload(const uint8_t* payload,
                                     uint8_t payloadLen,
                                     NwkLinkStatusCommand& command);
  static bool getLinkStatusEntry(const NwkLinkStatusCommand& command,
                                 uint8_t index, NwkLinkStatusEntry& entry);

  /** Build the 15-byte Zigbee beacon payload carried by 802.15.4 beacons. */
  static uint8_t buildBeaconPayload(uint8_t* out, uint8_t outMax,
                                    uint64_t extendedPanId,
                                    uint8_t deviceDepth,
                                    bool routerCapacity,
                                    bool endDeviceCapacity,
                                    uint8_t updateId,
                                    uint8_t stackProfile = kStackProfilePro);
  static bool parseBeaconPayload(const uint8_t* payload, uint8_t payloadLen,
                                 NwkBeaconPayload& beacon);

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

#endif  // NIUS_ZIGBEE_NWK_H
