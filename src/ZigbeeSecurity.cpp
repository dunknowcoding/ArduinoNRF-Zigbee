#include "ZigbeeSecurity.h"

#include <string.h>
#include <NrfCrypto.h>  // ArduinoNRF core: hardware AES-128 ECB block

#include "ZigbeeCcmStar.h"  // shared CCM* core (also used by ZigbeeApsSecurity)

namespace nzb {

namespace {

inline void writeLe32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

inline uint32_t readLe32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

inline void writeLe64(uint8_t* p, uint64_t v) {
  for (uint8_t i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

inline uint64_t readLe64(const uint8_t* p) {
  uint64_t v = 0;
  for (uint8_t i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (8 * i);
  return v;
}

// NWK frame control byte 1: bit 1 = security.
inline bool nwkSecurityBit(const uint8_t* npdu) {
  return (npdu[1] & 0x02) != 0;
}
inline void setNwkSecurityBit(uint8_t* npdu, bool on) {
  if (on) {
    npdu[1] |= 0x02;
  } else {
    npdu[1] &= (uint8_t)~0x02;
  }
}

}  // namespace

ZigbeeSecurity::ZigbeeSecurity()
    : hasKey_(false), keySequence_(0), hasAlt_(false), altKeySeq_(0), stats_() {
  memset(key_, 0, sizeof(key_));
  memset(altKey_, 0, sizeof(altKey_));
  resetReplayTable();
}

void ZigbeeSecurity::setNetworkKey(const uint8_t key[kKeyLen],
                                   uint8_t keySequence) {
  memcpy(key_, key, kKeyLen);
  keySequence_ = keySequence;
  hasKey_ = true;
}

void ZigbeeSecurity::setAlternateKey(const uint8_t key[kKeyLen],
                                     uint8_t keySequence) {
  memcpy(altKey_, key, kKeyLen);
  altKeySeq_ = keySequence;
  hasAlt_ = true;
}

bool ZigbeeSecurity::switchKey(uint8_t keySequence) {
  if (hasKey_ && keySequence == keySequence_) return true;  // already active
  if (hasAlt_ && keySequence == altKeySeq_) {
    // Swap: the alternate becomes active; the old active is kept as alternate
    // so in-flight frames under it still decrypt during the transition.
    uint8_t tmpKey[kKeyLen];
    uint8_t tmpSeq = keySequence_;
    memcpy(tmpKey, key_, kKeyLen);
    memcpy(key_, altKey_, kKeyLen);
    keySequence_ = altKeySeq_;
    memcpy(altKey_, tmpKey, kKeyLen);
    altKeySeq_ = tmpSeq;
    hasAlt_ = hasKey_;  // the previous active key is now the alternate
    hasKey_ = true;
    return true;
  }
  return false;
}

void ZigbeeSecurity::resetReplayTable() {
  for (uint8_t i = 0; i < kMaxReplayPeers; ++i) replay_[i] = ReplayEntry();
}

bool ZigbeeSecurity::replayCheckAndUpdate(uint64_t ieee, uint32_t counter) {
  ReplayEntry* slot = nullptr;
  for (uint8_t i = 0; i < kMaxReplayPeers; ++i) {
    if (replay_[i].used && replay_[i].ieee == ieee) {
      if (counter <= replay_[i].lastCounter) return false;
      replay_[i].lastCounter = counter;
      return true;
    }
    if (!slot && !replay_[i].used) slot = &replay_[i];
  }
  if (!slot) {
    // Table full: recycle the entry with the lowest counter (oldest device).
    slot = &replay_[0];
    for (uint8_t i = 1; i < kMaxReplayPeers; ++i) {
      if (replay_[i].lastCounter < slot->lastCounter) slot = &replay_[i];
    }
  }
  slot->used = true;
  slot->ieee = ieee;
  slot->lastCounter = counter;
  return true;
}

// ---------------------------------------------------------------- CCM* core

bool ZigbeeSecurity::ccmStar(bool encrypt, const uint8_t nonce[13],
                             const uint8_t* aad, uint8_t aadLen,
                             const uint8_t* in, uint8_t inLen, uint8_t* out,
                             const uint8_t* micIn, uint8_t* micOut) {
  // CCM* with M = kMicLen (4) and L = 2 per IEEE 802.15.4 / Zigbee. Delegates
  // to the shared core (ZigbeeCcmStar.h) so the NWK and APS layers run one
  // hardware-verified implementation.
  return ccmStarCrypt(encrypt, key_, kMicLen, nonce, aad, aadLen, in, inLen,
                      out, micIn, micOut);
}

// ------------------------------------------------------------- public API

uint8_t ZigbeeSecurity::secureNpdu(const uint8_t* npdu, uint8_t npduLen,
                                   uint8_t headerLen, uint64_t srcIeee,
                                   uint32_t frameCounter, uint8_t* out,
                                   uint8_t outMax) {
  if (!hasKey_ || !npdu || !out || headerLen > npduLen) return 0;
  uint8_t payloadLen = (uint8_t)(npduLen - headerLen);
  uint8_t total = (uint8_t)(npduLen + kAuxLen + kMicLen);
  if (outMax < total) return 0;

  // Header with the security bit set, then the auxiliary header. The
  // security-control byte carries level 0 on air; level 5 is used for
  // the cryptographic computation.
  memcpy(out, npdu, headerLen);
  setNwkSecurityBit(out, true);

  uint8_t* aux = &out[headerLen];
  const uint8_t controlOnAir = 0x28;            // keyId=network(01), extNonce=1
  const uint8_t controlCrypto = controlOnAir | kLevel;
  aux[0] = controlOnAir;
  writeLe32(&aux[1], frameCounter);
  writeLe64(&aux[5], srcIeee);
  aux[13] = keySequence_;

  // Nonce: source IEEE (8) | frame counter (4) | security control (1).
  uint8_t nonce[13];
  writeLe64(&nonce[0], srcIeee);
  writeLe32(&nonce[8], frameCounter);
  nonce[12] = controlCrypto;

  // AAD: NWK header + aux header, both as processed (level substituted).
  uint8_t aadBuf[40];
  uint8_t aadLen = (uint8_t)(headerLen + kAuxLen);
  if (aadLen > sizeof(aadBuf)) return 0;
  memcpy(aadBuf, out, headerLen);
  memcpy(&aadBuf[headerLen], aux, kAuxLen);
  aadBuf[headerLen] = controlCrypto;

  uint8_t* cipher = &out[headerLen + kAuxLen];
  uint8_t mic[kMicLen];
  if (!ccmStar(true, nonce, aadBuf, aadLen, &npdu[headerLen], payloadLen,
               cipher, nullptr, mic)) {
    return 0;
  }
  memcpy(&out[headerLen + kAuxLen + payloadLen], mic, kMicLen);

  ++stats_.secured;
  return total;
}

uint8_t ZigbeeSecurity::openNpdu(const uint8_t* npdu, uint8_t npduLen,
                                 uint8_t headerLen, uint8_t* out,
                                 uint8_t outMax, uint64_t* senderIeee) {
  if (!hasKey_ || !npdu || !out) return 0;
  if (!nwkSecurityBit(npdu) ||
      npduLen < headerLen + kAuxLen + kMicLen) {
    ++stats_.micFailures;
    return 0;
  }

  const uint8_t* aux = &npdu[headerLen];
  uint32_t frameCounter = readLe32(&aux[1]);
  uint64_t srcIeee = readLe64(&aux[5]);
  uint8_t controlCrypto = (uint8_t)((aux[0] & ~0x07) | kLevel);

  // Select the decryption key by the aux header's key sequence number. With a
  // single key this is always key_ (behaviour unchanged); during a key rotation
  // an alternate key lets frames secured under either key be accepted.
  const uint8_t* useKey = key_;
  if (hasAlt_) {
    uint8_t keySeq = aux[13];
    if (keySeq == keySequence_) {
      useKey = key_;
    } else if (keySeq == altKeySeq_) {
      useKey = altKey_;
    } else {
      ++stats_.micFailures;
      return 0;
    }
  }

  uint8_t cipherLen =
      (uint8_t)(npduLen - headerLen - kAuxLen - kMicLen);
  uint8_t plainTotal = (uint8_t)(headerLen + cipherLen);
  if (outMax < plainTotal) return 0;

  uint8_t nonce[13];
  writeLe64(&nonce[0], srcIeee);
  writeLe32(&nonce[8], frameCounter);
  nonce[12] = controlCrypto;

  // Decrypt payload (CTR), writing plaintext after the copied header.
  memcpy(out, npdu, headerLen);
  setNwkSecurityBit(out, false);
  uint8_t* plain = &out[headerLen];
  {
    // CTR pass via ccmStar's encrypt path (XOR is symmetric): run keystream
    // over the ciphertext into plain, skipping authentication by passing an
    // empty AAD/MIC; authentication happens in the second pass below.
    uint8_t a[16];
    a[0] = 0x01;
    memcpy(&a[1], nonce, 13);
    uint16_t counter = 1;
    uint8_t pos = 0;
    while (pos < cipherLen) {
      a[14] = (uint8_t)(counter >> 8);
      a[15] = (uint8_t)(counter & 0xFF);
      ++counter;
      uint8_t ks[16];
      if (!NrfEcb::encrypt(useKey, a, ks)) return 0;
      for (uint8_t i = 0; i < 16 && pos < cipherLen; ++i, ++pos) {
        plain[pos] = npdu[headerLen + kAuxLen + pos] ^ ks[i];
      }
    }
  }

  // Authenticate over header + aux (level substituted) + plaintext.
  uint8_t aadBuf[40];
  uint8_t aadLen = (uint8_t)(headerLen + kAuxLen);
  if (aadLen > sizeof(aadBuf)) return 0;
  memcpy(aadBuf, npdu, headerLen);
  memcpy(&aadBuf[headerLen], aux, kAuxLen);
  setNwkSecurityBit(aadBuf, true);  // AAD carries the on-air header
  aadBuf[headerLen] = controlCrypto;

  const uint8_t* mic = &npdu[npduLen - kMicLen];
  if (!ccmStarCrypt(false, useKey, kMicLen, nonce, aadBuf, aadLen, plain,
                    cipherLen, nullptr, mic, nullptr)) {
    ++stats_.micFailures;
    return 0;
  }

  if (!replayCheckAndUpdate(srcIeee, frameCounter)) {
    ++stats_.replays;
    return 0;
  }

  if (senderIeee) *senderIeee = srcIeee;
  ++stats_.opened;
  return plainTotal;
}

}  // namespace nzb
