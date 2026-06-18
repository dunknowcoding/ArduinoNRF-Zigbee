/*
  ZigbeeTcLinkKeyStore.h - the Trust Center's per-device link-key store.

  When a device joins, the Trust Center sends it the network key inside an APS
  Transport-Key command encrypted under a link key the joiner already holds
  (see ZigbeeApsSecurity). By default that is the global "ZigBeeAlliance09" key,
  but Zigbee 3.0 lets each device carry a unique INSTALL CODE from which the TC
  derives a per-device link key (see ZigbeeInstallCode). Per-device keys are the
  secure way to admit devices: a stolen global key cannot impersonate the join.

  This small table maps a joiner IEEE address to its link key. `keyFor(ieee)`
  returns the per-device key if one was provisioned, otherwise the global key, so
  the secure-join code can stay agnostic: it just asks the store which key to wrap
  the Transport-Key with. Provision a key directly, or from an install code (which
  validates the CRC and derives the key with AES-MMO).
*/
#ifndef NIUS_ZIGBEE_TC_LINK_KEY_STORE_H
#define NIUS_ZIGBEE_TC_LINK_KEY_STORE_H

#include <Arduino.h>

#include "ZigbeeApsKey.h"        // defaultTcLinkKey()
#include "ZigbeeInstallCode.h"   // install-code -> link key

namespace nzb {

struct TcLinkKeyEntry {
  bool used;
  uint64_t ieee;
  uint8_t key[16];
};

class ZigbeeTcLinkKeyStore {
 public:
  ZigbeeTcLinkKeyStore() : entries_(nullptr), capacity_(0) {
    memcpy(global_, ZigbeeApsKey::defaultTcLinkKey(), 16);
  }

  void begin(TcLinkKeyEntry* storage, uint8_t capacity) {
    entries_ = storage;
    capacity_ = capacity;
    for (uint8_t i = 0; i < capacity_; ++i) entries_[i] = TcLinkKeyEntry();
  }

  /** Override the global (default) TC link key used for devices with no
      per-device key. Defaults to "ZigBeeAlliance09". */
  void setGlobalKey(const uint8_t key[16]) { memcpy(global_, key, 16); }
  const uint8_t* globalKey() const { return global_; }

  /** Provision a per-device link key for @p ieee directly. Idempotent. */
  bool provision(uint64_t ieee, const uint8_t key[16]) {
    TcLinkKeyEntry* e = find(ieee);
    if (!e) e = firstFree();
    if (!e) return false;
    e->used = true;
    e->ieee = ieee;
    memcpy(e->key, key, 16);
    return true;
  }

  /** Provision a per-device link key for @p ieee from an install code: validates
      the CRC and derives the link key via AES-MMO. @return false on a bad CRC,
      a full table, or a derivation failure. */
  bool provisionInstallCode(uint64_t ieee, const uint8_t* codeWithCrc,
                            uint8_t len) {
    uint8_t key[16];
    if (!ZigbeeInstallCode::deriveLinkKey(codeWithCrc, len, key)) return false;
    return provision(ieee, key);
  }

  bool remove(uint64_t ieee) {
    TcLinkKeyEntry* e = find(ieee);
    if (!e) return false;
    *e = TcLinkKeyEntry();
    return true;
  }

  bool hasPerDevice(uint64_t ieee) const { return constFind(ieee) != nullptr; }

  /** The link key to use for @p ieee: the per-device key if provisioned, else
      the global key. Never null. */
  const uint8_t* keyFor(uint64_t ieee) const {
    const TcLinkKeyEntry* e = constFind(ieee);
    return e ? e->key : global_;
  }

 private:
  TcLinkKeyEntry* find(uint64_t ieee) {
    for (uint8_t i = 0; i < capacity_; ++i)
      if (entries_[i].used && entries_[i].ieee == ieee) return &entries_[i];
    return nullptr;
  }
  const TcLinkKeyEntry* constFind(uint64_t ieee) const {
    for (uint8_t i = 0; i < capacity_; ++i)
      if (entries_[i].used && entries_[i].ieee == ieee) return &entries_[i];
    return nullptr;
  }
  TcLinkKeyEntry* firstFree() {
    for (uint8_t i = 0; i < capacity_; ++i)
      if (!entries_[i].used) return &entries_[i];
    return nullptr;
  }

  TcLinkKeyEntry* entries_;
  uint8_t capacity_;
  uint8_t global_[16];
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_TC_LINK_KEY_STORE_H
