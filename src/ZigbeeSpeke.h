/*
  ZigbeeSpeke.h - Dynamic Link Key (DLK) via SPEKE over Curve25519, R23-style.

  Zigbee PRO 2023 (R23) replaces the well-known "ZigBeeAlliance09" link key with a
  password-authenticated key exchange: a low-entropy shared secret (install code or
  passphrase) authenticates an ephemeral elliptic-curve Diffie-Hellman so two
  parties derive the same 128-bit link key only if they hold the same secret. This
  is the SPEKE (Simple Password Exponential Key Exchange) variant over Curve25519.

  Built entirely on NiusZigbee's own crypto - ZigbeeCurve25519 (X25519) for the
  curve and ZigbeeApsSecurity::aesMmoHash (AES-MMO-128) for the password->generator
  mapping, the KDF, and key confirmation. No external crypto library.

  Protocol:
    G   = generator(password)                 (password mapped to a curve u-coord)
    A:  Xa = a * G        (a = random scalar)  -> send Xa
    B:  Xb = b * G        (b = random scalar)  -> send Xb
    A:  K  = a * Xb,  B: K = b * Xa            (== a*b*G on both sides)
    linkKey = KDF(K);  each side confirms with confirmTag() before trusting it.
  A wrong password yields a different generator, so the shares do not combine to a
  common K and confirmation fails - the join is rejected instead of using a guessable
  key.

  NOTE: cryptographically sound SPEKE, but the exact byte-level constants of
  certified R23 (generator mapping, KDF labels, transcript hashing) are spec-defined;
  interop with a certified R23 device requires matching those. Verified for internal
  consistency (mutual agreement / mismatch rejection / confirmation) by CC2530_Speke.
*/
#ifndef ARDUINONRF_ZIGBEE_SPEKE_H
#define ARDUINONRF_ZIGBEE_SPEKE_H

#include <stdint.h>
#include <string.h>
#include "ZigbeeCurve25519.h"
#include "ZigbeeApsSecurity.h"  // aesMmoHash (AES-MMO-128)

namespace nzb {

class ZigbeeSpeke {
 public:
  static const uint8_t kKeySize = 16;     // 128-bit link key
  static const uint8_t kShareSize = 32;   // X25519 public share / scalar

  /** Map the shared secret (install code / passphrase) to a Curve25519 generator
      u-coordinate: 32 bytes = AES-MMO("S0"||pw) || AES-MMO("S1"||pw). */
  static void generator(const uint8_t* password, uint16_t len, uint8_t gen[32]) {
    uint8_t buf[80];
    if (len > sizeof(buf) - 2) len = sizeof(buf) - 2;
    buf[0] = 'S'; buf[1] = 0; memcpy(buf + 2, password, len);
    ZigbeeApsSecurity::aesMmoHash(buf, (uint16_t)(2 + len), gen);
    buf[1] = 1;
    ZigbeeApsSecurity::aesMmoHash(buf, (uint16_t)(2 + len), gen + 16);
  }

  /** Public share for this party: outShare = myScalar * generator(password).
      @p myScalar is 32 random bytes (clamped internally by X25519). */
  static void publicShare(const uint8_t* password, uint16_t len,
                          const uint8_t myScalar[32], uint8_t outShare[32]) {
    uint8_t gen[32];
    generator(password, len, gen);
    ZigbeeCurve25519::scalarMult(myScalar, gen, outShare);
  }

  /** Derive the 128-bit link key from this party's scalar and the peer's share:
      linkKey = AES-MMO("LK" || (myScalar * peerShare)). Both parties get the same
      key iff they used the same password and exchanged shares honestly. */
  static void linkKey(const uint8_t myScalar[32], const uint8_t peerShare[32],
                      uint8_t outKey[16]) {
    uint8_t k[32], buf[34];
    ZigbeeCurve25519::scalarMult(myScalar, peerShare, k);
    buf[0] = 'L'; buf[1] = 'K'; memcpy(buf + 2, k, 32);
    ZigbeeApsSecurity::aesMmoHash(buf, 34, outKey);
  }

  /** Key-confirmation tag = AES-MMO(role || linkKey || H(initiatorShare ||
      responderShare)). The initiator sends tag with @p role = 1, the responder with
      @p role = 2; each verifies the other's tag (recomputed with the same share
      order) before trusting the key. A mismatched password gives a different key
      and the tags will not verify. */
  static void confirmTag(const uint8_t linkKey[16], uint8_t role,
                         const uint8_t initiatorShare[32],
                         const uint8_t responderShare[32], uint8_t tag[16]) {
    uint8_t tr[64], th[16], buf[33];
    memcpy(tr, initiatorShare, 32);
    memcpy(tr + 32, responderShare, 32);
    ZigbeeApsSecurity::aesMmoHash(tr, 64, th);   // transcript hash
    buf[0] = role; memcpy(buf + 1, linkKey, 16); memcpy(buf + 17, th, 16);
    ZigbeeApsSecurity::aesMmoHash(buf, 33, tag);
  }
};

}  // namespace nzb

#endif  // ARDUINONRF_ZIGBEE_SPEKE_H
