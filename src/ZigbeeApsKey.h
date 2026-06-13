/*
  ZigbeeApsKey.h - Zigbee APS key-transport command tooling (Trust Center).

  A joining device does not get the network key out of band; the Trust Center
  (usually the coordinator) sends it inside an APS Transport-Key command,
  encrypted at the APS layer under a link key the joiner already holds - the
  well-known default TC link key "ZigBeeAlliance09", or a per-device key
  derived from an install code. The joiner decrypts it and then uses the
  network key for all NWK-layer security.

  This header builds/parses the APS command-layer key frames (Transport Key,
  Request Key, Switch Key, Update/Remove Device). The APS-layer AES-CCM*
  encryption that protects a Transport-Key on air reuses the same CCM*
  primitive as ZigbeeSecurity (a different nonce/AAD); wiring that into the
  join flow is the integration step noted in the roadmap. The frames here are
  what carries the key once that envelope is in place.
*/
#ifndef NIUS_ZIGBEE_APS_KEY_H
#define NIUS_ZIGBEE_APS_KEY_H

#include <Arduino.h>

namespace nzb {

enum ApsKeyCommandId : uint8_t {
  APS_CMD_TRANSPORT_KEY = 0x05,
  APS_CMD_UPDATE_DEVICE = 0x06,
  APS_CMD_REMOVE_DEVICE = 0x07,
  APS_CMD_REQUEST_KEY = 0x08,
  APS_CMD_SWITCH_KEY = 0x09,
  APS_CMD_VERIFY_KEY = 0x0F,
  APS_CMD_CONFIRM_KEY = 0x10,
};

enum ApsKeyType : uint8_t {
  APS_KEY_TC_MASTER = 0x00,
  APS_KEY_STANDARD_NETWORK = 0x01,
  APS_KEY_APP_MASTER = 0x02,
  APS_KEY_APP_LINK = 0x03,
  APS_KEY_TC_LINK = 0x04,
  APS_KEY_HIGH_SECURITY_NETWORK = 0x05,
};

struct ApsTransportKey {
  uint8_t keyType;
  uint8_t key[16];
  uint8_t keySeqNumber;   // valid for a network key
  uint64_t destAddress;
  uint64_t srcAddress;
};

struct ApsRequestKey {
  uint8_t keyType;
  uint64_t partnerAddress;  // present for application link key requests
};

class ZigbeeApsKey {
 public:
  // The default global Trust Center link key, "ZigBeeAlliance09".
  static const uint8_t* defaultTcLinkKey() {
    static const uint8_t k[16] = {
        0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C,
        0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39};
    return k;
  }

  /** Build a Transport-Key command carrying a network key (the common case:
      TC -> joiner). Layout: cmdId(1) keyType(1) key(16) keySeq(1) dst(8)
      src(8) = 35 bytes. */
  static uint8_t buildTransportNetworkKey(uint8_t* out, uint8_t outMax,
                                          const ApsTransportKey& t) {
    if (!out || outMax < 35) return 0;
    uint8_t* p = out;
    *p++ = APS_CMD_TRANSPORT_KEY;
    *p++ = t.keyType;
    for (uint8_t i = 0; i < 16; ++i) *p++ = t.key[i];
    *p++ = t.keySeqNumber;
    putLe64(p, t.destAddress); p += 8;
    putLe64(p, t.srcAddress); p += 8;
    return (uint8_t)(p - out);
  }

  static bool parseTransportNetworkKey(const uint8_t* payload, uint8_t len,
                                       ApsTransportKey& t) {
    t = ApsTransportKey();
    if (!payload || len < 35) return false;
    if (payload[0] != APS_CMD_TRANSPORT_KEY) return false;
    const uint8_t* p = &payload[1];
    t.keyType = *p++;
    if (t.keyType != APS_KEY_STANDARD_NETWORK &&
        t.keyType != APS_KEY_HIGH_SECURITY_NETWORK) {
      return false;
    }
    for (uint8_t i = 0; i < 16; ++i) t.key[i] = *p++;
    t.keySeqNumber = *p++;
    t.destAddress = getLe64(p); p += 8;
    t.srcAddress = getLe64(p); p += 8;
    return true;
  }

  /** Build a Request-Key command (joiner -> TC). Layout: cmdId(1) keyType(1)
      [partner(8) for app link key]. */
  static uint8_t buildRequestKey(uint8_t* out, uint8_t outMax,
                                 const ApsRequestKey& r) {
    bool withPartner = (r.keyType == APS_KEY_APP_LINK);
    uint8_t needed = (uint8_t)(2 + (withPartner ? 8 : 0));
    if (!out || outMax < needed) return 0;
    out[0] = APS_CMD_REQUEST_KEY;
    out[1] = r.keyType;
    if (withPartner) putLe64(&out[2], r.partnerAddress);
    return needed;
  }

  static bool parseRequestKey(const uint8_t* payload, uint8_t len,
                              ApsRequestKey& r) {
    r = ApsRequestKey();
    if (!payload || len < 2) return false;
    if (payload[0] != APS_CMD_REQUEST_KEY) return false;
    r.keyType = payload[1];
    if (r.keyType == APS_KEY_APP_LINK) {
      if (len < 10) return false;
      r.partnerAddress = getLe64(&payload[2]);
    }
    return true;
  }

  /** Build a Switch-Key command (TC -> network): cmdId(1) keySeq(1). */
  static uint8_t buildSwitchKey(uint8_t* out, uint8_t outMax,
                                uint8_t keySeqNumber) {
    if (!out || outMax < 2) return 0;
    out[0] = APS_CMD_SWITCH_KEY;
    out[1] = keySeqNumber;
    return 2;
  }

  static bool parseSwitchKey(const uint8_t* payload, uint8_t len,
                             uint8_t& keySeqNumber) {
    if (!payload || len < 2 || payload[0] != APS_CMD_SWITCH_KEY) return false;
    keySeqNumber = payload[1];
    return true;
  }

 private:
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

#endif  // NIUS_ZIGBEE_APS_KEY_H
