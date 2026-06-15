/*
  ZigbeeInterPan.h - inter-PAN transmission framing (the stub NWK/APS that
  carries touchlink and other commands between devices with no shared network).

  Some commissioning traffic must travel before a device has joined any network
  - touchlink scans, for instance. Those ride "inter-PAN": a MAC data frame
  whose payload is a one-octet stub NWK header (frame type inter-PAN, protocol
  version 2) followed by a stripped-down APS header (frame control + optional
  group address + cluster id + profile id) and the command. There is no NWK
  routing, no APS counter, and no endpoints.

  This header builds/parses that inter-PAN APDU (the bytes that go in the MAC
  payload). The caller still sends it with ZigbeeMac framing - typically a
  broadcast (dst PAN 0xFFFF) for a scan, or a unicast to a discovered device.
*/
#ifndef NIUS_ZIGBEE_INTERPAN_H
#define NIUS_ZIGBEE_INTERPAN_H

#include <Arduino.h>

namespace nzb {

// Inter-PAN APS delivery modes (frame control bits 2-3).
enum InterPanDelivery : uint8_t {
  INTERPAN_UNICAST = 0,
  INTERPAN_BROADCAST = 2,
  INTERPAN_GROUP = 3,
};

struct InterPanFrame {
  bool valid;
  uint8_t deliveryMode;
  uint16_t groupAddress;  // valid for group delivery
  uint16_t clusterId;
  uint16_t profileId;
  const uint8_t* payload;
  uint8_t payloadLen;
};

class ZigbeeInterPan {
 public:
  // Stub NWK header octet: frame type = inter-PAN (0b11), protocol version 2.
  static const uint8_t kStubNwkHeader = 0x0B;
  // Inter-PAN APS frame type (frame control bits 0-1).
  static const uint8_t kApsFrameType = 0x03;

  /** Build an inter-PAN APDU: stub NWK(1) + APS FC(1) [+ group(2)] + cluster(2)
      + profile(2) + payload. @return total length, or 0 on error. */
  static uint8_t build(uint8_t* out, uint8_t outMax, uint8_t deliveryMode,
                       uint16_t groupAddress, uint16_t clusterId,
                       uint16_t profileId, const uint8_t* payload,
                       uint8_t payloadLen) {
    bool group = (deliveryMode == INTERPAN_GROUP);
    uint8_t hdr = (uint8_t)(2 + (group ? 2 : 0) + 4);  // stubNwk+apsFc [+grp] +cl+pr
    uint8_t need = (uint8_t)(hdr + payloadLen);
    if (!out || outMax < need) return 0;
    if (payloadLen > 0 && !payload) return 0;

    out[0] = kStubNwkHeader;
    out[1] = (uint8_t)(kApsFrameType | ((deliveryMode & 0x03) << 2));
    uint8_t idx = 2;
    if (group) { putLe16(&out[idx], groupAddress); idx += 2; }
    putLe16(&out[idx], clusterId); idx += 2;
    putLe16(&out[idx], profileId); idx += 2;
    for (uint8_t i = 0; i < payloadLen; ++i) out[idx + i] = payload[i];
    return need;
  }

  static bool parse(const uint8_t* in, uint8_t len, InterPanFrame& f) {
    f = InterPanFrame();
    if (!in || len < 6) return false;
    if (in[0] != kStubNwkHeader) return false;
    uint8_t fc = in[1];
    if ((uint8_t)(fc & 0x03) != kApsFrameType) return false;
    f.deliveryMode = (uint8_t)((fc >> 2) & 0x03);
    uint8_t idx = 2;
    if (f.deliveryMode == INTERPAN_GROUP) {
      if ((uint8_t)(idx + 2) > len) return false;
      f.groupAddress = getLe16(&in[idx]);
      idx += 2;
    }
    if ((uint8_t)(idx + 4) > len) return false;
    f.clusterId = getLe16(&in[idx]); idx += 2;
    f.profileId = getLe16(&in[idx]); idx += 2;
    f.payload = &in[idx];
    f.payloadLen = (uint8_t)(len - idx);
    f.valid = true;
    return true;
  }

 private:
  static void putLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8);
  }
  static uint16_t getLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_INTERPAN_H
