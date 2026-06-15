/*
  ZigbeeApsSecurity.h - APS-layer AES-CCM* security for Zigbee key transport.

  The network key is delivered to a joiner inside an APS Transport-Key command
  (see ZigbeeApsKey) that is itself encrypted at the APS layer under a key the
  joiner already holds - a "specialized" key derived from the link key. This
  header provides that envelope:

    * AES-MMO hash (Zigbee spec B.6) - the keyed one-way hash built on the
      chip's AES-128 block.
    * HMAC over AES-MMO (Zigbee B.1.4) - the keyed MAC the spec uses for key
      derivation.
    * Specialized key derivation (Zigbee 4.5.3): the key-transport key
      (HMAC(linkKey, 0x00)) protects Transport-Key commands; the key-load key
      (HMAC(linkKey, 0x02)) protects other key commands.
    * APS CCM* encrypt/decrypt of a command payload, using the SAME CCM* core
      (ZigbeeCcmStar.h) as the hardware-verified NWK security, with an APS
      nonce (source IEEE | frame counter | security control) and the APS
      header + auxiliary header as additional authenticated data.

  Like the NWK layer, the security level is zeroed in the transmitted security
  control byte and substituted (level 5, ENC-MIC-32) for the nonce/MIC
  computation, as real stacks do.
*/
#ifndef NIUS_ZIGBEE_APS_SECURITY_H
#define NIUS_ZIGBEE_APS_SECURITY_H

#include <Arduino.h>
#include <NrfCrypto.h>  // ArduinoNRF core: hardware AES-128 ECB block

#include "ZigbeeCcmStar.h"

namespace nzb {

// APS security control: key-identifier sub-field (bits 3-4).
enum ApsSecurityKeyId : uint8_t {
  APS_SEC_KEY_DATA = 0,           // application data / link key
  APS_SEC_KEY_NETWORK = 1,        // network key
  APS_SEC_KEY_KEY_TRANSPORT = 2,  // key-transport key (Transport-Key cmds)
  APS_SEC_KEY_KEY_LOAD = 3,       // key-load key (other key cmds)
};

class ZigbeeApsSecurity {
 public:
  static const uint8_t kKeyLen = 16;
  static const uint8_t kMicLen = 4;   // ENC-MIC-32
  static const uint8_t kLevel = 5;    // ENC-MIC-32
  static const uint8_t kAuxLen = 13;  // ctrl(1) + fc(4) + srcIeee(8), ext nonce
  static const uint8_t kHashLen = 16;

  // ---------------------------------------------------------- AES-MMO hash
  /** Matyas-Meyer-Oseas hash over the AES-128 block (Zigbee B.6). Handles
      messages up to ~80 bytes (enough for key derivation and a Transport-Key);
      returns false if the padded message would exceed the scratch buffer. */
  static bool aesMmoHash(const uint8_t* msg, uint16_t len, uint8_t out[16]) {
    uint8_t buf[96];
    // Pad: append 0x80, zero-fill, 16-bit big-endian bit length in the last 2
    // octets, total rounded up to a 16-byte multiple.
    uint16_t total = (uint16_t)(((len + 3 + 15) / 16) * 16);
    if (total > sizeof(buf)) return false;  // message too long for this helper
    memset(buf, 0, total);
    memcpy(buf, msg, len);
    buf[len] = 0x80;
    uint16_t bitLen = (uint16_t)(len * 8);
    buf[total - 2] = (uint8_t)(bitLen >> 8);
    buf[total - 1] = (uint8_t)(bitLen & 0xFF);

    uint8_t hash[16];
    memset(hash, 0, 16);
    for (uint16_t off = 0; off < total; off += 16) {
      uint8_t enc[16];
      if (!NrfEcb::encrypt(hash, &buf[off], enc)) return false;  // key = hash
      for (uint8_t i = 0; i < 16; ++i) hash[i] = (uint8_t)(enc[i] ^ buf[off + i]);
    }
    memcpy(out, hash, 16);
    return true;
  }

  // -------------------------------------------------------------- HMAC-MMO
  /** HMAC (Zigbee B.1.4) keyed with @p key (16 B = the hash block size) over
      @p data, using AES-MMO as the hash. @p data must be short (<= 64 B). */
  static bool hmacAesMmo(const uint8_t key[16], const uint8_t* data,
                         uint16_t len, uint8_t out[16]) {
    uint8_t kIpad[16], kOpad[16];
    for (uint8_t i = 0; i < 16; ++i) {
      kIpad[i] = (uint8_t)(key[i] ^ 0x36);
      kOpad[i] = (uint8_t)(key[i] ^ 0x5C);
    }
    uint8_t inner[16];
    {
      uint8_t buf[16 + 64];
      if ((uint16_t)(16 + len) > sizeof(buf)) return false;
      memcpy(buf, kIpad, 16);
      memcpy(buf + 16, data, len);
      if (!aesMmoHash(buf, (uint16_t)(16 + len), inner)) return false;
    }
    uint8_t buf2[32];
    memcpy(buf2, kOpad, 16);
    memcpy(buf2 + 16, inner, 16);
    return aesMmoHash(buf2, 32, out);
  }

  /** Key-transport key = HMAC(linkKey, 0x00); protects Transport-Key cmds. */
  static bool deriveKeyTransportKey(const uint8_t linkKey[16], uint8_t out[16]) {
    uint8_t in = 0x00;
    return hmacAesMmo(linkKey, &in, 1, out);
  }
  /** Key-load key = HMAC(linkKey, 0x02); protects other key commands. */
  static bool deriveKeyLoadKey(const uint8_t linkKey[16], uint8_t out[16]) {
    uint8_t in = 0x02;
    return hmacAesMmo(linkKey, &in, 1, out);
  }

  // ------------------------------------------------- APS command CCM* envelope
  /** Encrypt+authenticate an APS command payload under @p key. The output is
      @p apsHeader (copied verbatim, the AAD prefix) followed by the APS
      auxiliary security header, the ciphertext, and the 4-byte MIC.
      @param keyId   APS security key identifier (e.g. APS_SEC_KEY_KEY_TRANSPORT).
      @param srcIeee our IEEE address (into the aux header + nonce).
      @param frameCounter  strictly increasing APS security counter.
      @return total written length, or 0 on error. */
  static uint8_t secureCommand(const uint8_t* apsHeader, uint8_t apsHeaderLen,
                               const uint8_t key[16], uint8_t keyId,
                               uint64_t srcIeee, uint32_t frameCounter,
                               const uint8_t* payload, uint8_t payloadLen,
                               uint8_t* out, uint8_t outMax) {
    uint16_t total =
        (uint16_t)(apsHeaderLen + kAuxLen + payloadLen + kMicLen);
    if (!out || outMax < total) return 0;

    memcpy(out, apsHeader, apsHeaderLen);
    uint8_t* aux = out + apsHeaderLen;
    aux[0] = onAirControl(keyId);  // level zeroed on air
    writeLe32(&aux[1], frameCounter);
    writeLe64(&aux[5], srcIeee);

    uint8_t cryptoControl = cryptoControlByte(keyId);
    uint8_t nonce[13];
    writeLe64(&nonce[0], srcIeee);
    writeLe32(&nonce[8], frameCounter);
    nonce[12] = cryptoControl;

    uint8_t aad[80];
    uint8_t aadLen = (uint8_t)(apsHeaderLen + kAuxLen);
    if (aadLen > sizeof(aad)) return 0;
    memcpy(aad, apsHeader, apsHeaderLen);
    memcpy(aad + apsHeaderLen, aux, kAuxLen);
    aad[apsHeaderLen] = cryptoControl;  // level substituted in the AAD copy

    uint8_t* cipher = out + apsHeaderLen + kAuxLen;
    uint8_t mic[kMicLen];
    if (!ccmStarCrypt(true, key, kMicLen, nonce, aad, aadLen, payload,
                      payloadLen, cipher, nullptr, mic)) {
      return 0;
    }
    memcpy(cipher + payloadLen, mic, kMicLen);
    return (uint8_t)total;
  }

  /** Verify+decrypt an APS command secured by secureCommand(). @p secured is
      apsHeader | aux | ciphertext | MIC; @p apsHeaderLen tells the split.
      Writes the recovered plaintext to @p payloadOut. @return plaintext length,
      or 0 on MIC failure / malformed input. Fills @p frameCounterOut /
      @p srcIeeeOut from the aux header when non-null. */
  static uint8_t openCommand(const uint8_t* secured, uint8_t securedLen,
                             uint8_t apsHeaderLen, const uint8_t key[16],
                             uint8_t* payloadOut, uint8_t payloadOutMax,
                             uint32_t* frameCounterOut = nullptr,
                             uint64_t* srcIeeeOut = nullptr) {
    if (!secured || securedLen < apsHeaderLen + kAuxLen + kMicLen) return 0;
    const uint8_t* aux = secured + apsHeaderLen;
    uint8_t keyId = (uint8_t)((aux[0] >> 3) & 0x03);
    uint32_t frameCounter = readLe32(&aux[1]);
    uint64_t srcIeee = readLe64(&aux[5]);
    uint8_t cryptoControl = (uint8_t)((aux[0] & ~0x07) | kLevel);

    uint8_t cipherLen =
        (uint8_t)(securedLen - apsHeaderLen - kAuxLen - kMicLen);
    if (payloadOutMax < cipherLen) return 0;

    uint8_t nonce[13];
    writeLe64(&nonce[0], srcIeee);
    writeLe32(&nonce[8], frameCounter);
    nonce[12] = cryptoControl;

    // CTR-decrypt the ciphertext into payloadOut (XOR keystream).
    const uint8_t* cipher = secured + apsHeaderLen + kAuxLen;
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
      if (!NrfEcb::encrypt(key, a, ks)) return 0;
      for (uint8_t i = 0; i < 16 && pos < cipherLen; ++i, ++pos)
        payloadOut[pos] = (uint8_t)(cipher[pos] ^ ks[i]);
    }

    // Authenticate over apsHeader + aux (level substituted) + plaintext.
    uint8_t aad[80];
    uint8_t aadLen = (uint8_t)(apsHeaderLen + kAuxLen);
    if (aadLen > sizeof(aad)) return 0;
    memcpy(aad, secured, apsHeaderLen + kAuxLen);
    aad[apsHeaderLen] = cryptoControl;

    const uint8_t* mic = secured + securedLen - kMicLen;
    if (!ccmStarCrypt(false, key, kMicLen, nonce, aad, aadLen, payloadOut,
                      cipherLen, nullptr, mic, nullptr)) {
      return 0;
    }
    (void)keyId;
    if (frameCounterOut) *frameCounterOut = frameCounter;
    if (srcIeeeOut) *srcIeeeOut = srcIeee;
    return cipherLen;
  }

  // ------------------------------------------- APS application-data encryption
  /** End-to-end encrypt an APS DATA frame's application payload under a shared
      link key (APS-layer security, distinct from the NWK-layer encryption every
      frame already gets). @p apsFrame is a built APS data frame
      (ZigbeeAps::buildDataFrame); @p headerLen is its header length (8 for a
      unicast data frame). The output is the APS header (with the security bit
      set) + the APS auxiliary header + the encrypted payload + MIC. @return the
      secured length, or 0 on error. */
  static uint8_t secureDataFrame(const uint8_t* apsFrame, uint8_t apsFrameLen,
                                 uint8_t headerLen, const uint8_t key[16],
                                 uint64_t srcIeee, uint32_t frameCounter,
                                 uint8_t* out, uint8_t outMax) {
    if (!apsFrame || apsFrameLen < headerLen) return 0;
    uint8_t hdr[16];
    if (headerLen > sizeof(hdr)) return 0;
    memcpy(hdr, apsFrame, headerLen);
    hdr[0] |= 0x20;  // APS frame control: security sub-field
    return secureCommand(hdr, headerLen, key, APS_SEC_KEY_DATA, srcIeee,
                         frameCounter, apsFrame + headerLen,
                         (uint8_t)(apsFrameLen - headerLen), out, outMax);
  }

  /** Verify+decrypt a frame from secureDataFrame() back into a plain APS data
      frame: the APS header (security bit cleared) + recovered payload. @return
      the reconstructed APS data-frame length, or 0 on MIC failure. */
  static uint8_t openDataFrame(const uint8_t* secured, uint8_t securedLen,
                               uint8_t headerLen, const uint8_t key[16],
                               uint8_t* out, uint8_t outMax,
                               uint32_t* frameCounterOut = nullptr) {
    if (!secured || securedLen < headerLen || headerLen > outMax) return 0;
    uint8_t payload[96];
    uint8_t n = openCommand(secured, securedLen, headerLen, key, payload,
                            sizeof(payload), frameCounterOut, nullptr);
    if (n == 0) return 0;
    if ((uint16_t)(headerLen + n) > outMax) return 0;
    memcpy(out, secured, headerLen);
    out[0] &= (uint8_t)~0x20;  // clear the security bit in the recovered frame
    memcpy(out + headerLen, payload, n);
    return (uint8_t)(headerLen + n);
  }

 private:
  // Security control byte: level(0-2) | keyId(3-4) | extNonce(5). Extended
  // nonce is always set here (the source IEEE is carried in the aux header).
  static uint8_t onAirControl(uint8_t keyId) {
    return (uint8_t)(0 | ((keyId & 0x03) << 3) | 0x20);
  }
  static uint8_t cryptoControlByte(uint8_t keyId) {
    return (uint8_t)(kLevel | ((keyId & 0x03) << 3) | 0x20);
  }
  static void writeLe32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
  }
  static uint32_t readLe32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
  }
  static void writeLe64(uint8_t* p, uint64_t v) {
    for (uint8_t i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (8 * i));
  }
  static uint64_t readLe64(const uint8_t* p) {
    uint64_t v = 0;
    for (uint8_t i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (8 * i);
    return v;
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_APS_SECURITY_H
