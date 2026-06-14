/*
  ZigbeeCcmStar.h - shared AES-CCM* core (IEEE 802.15.4 / Zigbee), L=2.

  This is the exact CCM* construction (CBC-MAC + CTR over the chip's hardware
  AES-128 ECB block) that ZigbeeSecurity uses for NWK-layer security and that
  ZigbeeApsSecurity uses for APS-layer key transport. Pulling it into one
  free function keeps a single, hardware-verified implementation behind both
  layers instead of two copies that could drift.

  - encrypt=true:  authenticate AAD + plaintext `in`, write ciphertext to
                   `out` and the M-byte tag to `micOut`.
  - encrypt=false: authenticate AAD + plaintext `in` (the caller has already
                   CTR-decrypted the ciphertext into `in`) and verify it
                   matches `micIn`; returns false on MIC mismatch.

  M (the MIC length) is a parameter; Zigbee uses M=4 (ENC-MIC-32). L is fixed
  at 2, so the 13-byte nonce + 2-byte length counter fill the 16-byte block.
*/
#ifndef NIUS_ZIGBEE_CCM_STAR_H
#define NIUS_ZIGBEE_CCM_STAR_H

#include <Arduino.h>
#include <NrfCrypto.h>  // ArduinoNRF core: hardware AES-128 ECB block

namespace nzb {

/** AES-CCM* (L=2) over the hardware AES block. See file header for the
    encrypt/decrypt contract. @return false on AES failure or MIC mismatch. */
inline bool ccmStarCrypt(bool encrypt, const uint8_t key[16], uint8_t micLen,
                         const uint8_t nonce[13], const uint8_t* aad,
                         uint8_t aadLen, const uint8_t* in, uint8_t inLen,
                         uint8_t* out, const uint8_t* micIn, uint8_t* micOut) {
  uint8_t block[16];
  uint8_t x[16];

  // ---- authentication (CBC-MAC over B0 | AAD | message) ----
  block[0] = 0x40 |                       // Adata
             (((micLen - 2) / 2) << 3) |  // M' = (M-2)/2
             0x01;                        // L' = L-1
  memcpy(&block[1], nonce, 13);
  block[14] = 0;
  block[15] = inLen;  // l(m), 16-bit big-endian (inLen fits the low byte)
  if (!NrfEcb::encrypt(key, block, x)) return false;

  // AAD: prefixed with its 16-bit big-endian length, zero-padded to blocks.
  uint8_t pos = 0;
  uint8_t chunk[16];
  chunk[0] = 0;
  chunk[1] = aadLen;
  uint8_t fill = 2;
  while (pos < aadLen) {
    while (fill < 16 && pos < aadLen) chunk[fill++] = aad[pos++];
    while (fill < 16) chunk[fill++] = 0;
    for (uint8_t i = 0; i < 16; ++i) block[i] = x[i] ^ chunk[i];
    if (!NrfEcb::encrypt(key, block, x)) return false;
    fill = 0;
  }

  // Message blocks (always over the PLAINTEXT - on decrypt the caller passes
  // the already-recovered plaintext via `in`).
  pos = 0;
  while (pos < inLen) {
    fill = 0;
    while (fill < 16 && pos < inLen) chunk[fill++] = in[pos++];
    while (fill < 16) chunk[fill++] = 0;
    for (uint8_t i = 0; i < 16; ++i) block[i] = x[i] ^ chunk[i];
    if (!NrfEcb::encrypt(key, block, x)) return false;
  }
  // x now holds the CBC-MAC tag T (untruncated).

  // ---- A0 keystream for the MIC ----
  uint8_t a[16];
  a[0] = 0x01;  // flags: L' = 1
  memcpy(&a[1], nonce, 13);
  a[14] = 0;
  a[15] = 0;  // counter 0
  uint8_t s0[16];
  if (!NrfEcb::encrypt(key, a, s0)) return false;

  if (micOut) {
    for (uint8_t i = 0; i < micLen; ++i) micOut[i] = x[i] ^ s0[i];
  }
  if (micIn) {
    uint8_t diff = 0;
    for (uint8_t i = 0; i < micLen; ++i) diff |= (uint8_t)(micIn[i] ^ x[i] ^ s0[i]);
    if (diff != 0) return false;
  }

  // ---- CTR keystream for the payload (A1, A2, ...) ----
  if (encrypt && out) {
    uint16_t counter = 1;
    pos = 0;
    while (pos < inLen) {
      a[14] = (uint8_t)(counter >> 8);
      a[15] = (uint8_t)(counter & 0xFF);
      ++counter;
      uint8_t ks[16];
      if (!NrfEcb::encrypt(key, a, ks)) return false;
      for (uint8_t i = 0; i < 16 && pos < inLen; ++i, ++pos) out[pos] = in[pos] ^ ks[i];
    }
  }
  return true;
}

}  // namespace nzb

#endif  // NIUS_ZIGBEE_CCM_STAR_H
