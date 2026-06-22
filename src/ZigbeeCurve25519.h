/*
  ZigbeeCurve25519.h - X25519 (Curve25519 ECDH, RFC 7748), implemented in-tree.

  NiusZigbee provides its own cryptography; this does NOT depend on any external
  crypto library. X25519 is the elliptic-curve Diffie-Hellman primitive required by
  the Zigbee PRO 2023 (R23) Dynamic Link Key negotiation (SPEKE over Curve25519);
  see ZigbeeSpeke.h, which builds the password-authenticated key exchange on top.

  Algorithm: the field arithmetic mod p = 2^255-19 and the Montgomery ladder follow
  the well-known public-domain TweetNaCl construction (Bernstein et al.). The
  implementation is verified against the RFC 7748 section 5.2 / section 6.1 test
  vectors by the CC2530_Curve25519 self-test.

  Runs on the nRF52840 host; no radio involved. ~order-of-milliseconds per scalar
  multiplication, which is fine for the occasional commissioning key agreement.
*/
#ifndef ARDUINONRF_ZIGBEE_CURVE25519_H
#define ARDUINONRF_ZIGBEE_CURVE25519_H

#include <stdint.h>
#include <string.h>

namespace nzb {

class ZigbeeCurve25519 {
 public:
  static const uint8_t kKeySize = 32;

  /** out = scalar * basePoint(9): the X25519 public key for a private scalar.
      The scalar is clamped per RFC 7748 internally. */
  static void basePoint(const uint8_t scalar[32], uint8_t out[32]) {
    uint8_t nine[32];
    memset(nine, 0, 32);
    nine[0] = 9;
    scalarMult(scalar, nine, out);
  }

  /** out = scalar * point (X25519). With a peer's public key as `point`, this is
      the ECDH shared secret. Returns out as a 32-byte little-endian u-coordinate. */
  static void scalarMult(const uint8_t scalar[32], const uint8_t point[32],
                         uint8_t out[32]) {
    uint8_t z[32];
    gfe x, a, b, c, d, e, f;
    int64_t r, i;
    for (i = 0; i < 31; i++) z[i] = scalar[i];
    z[31] = (uint8_t)((scalar[31] & 127) | 64);
    z[0] = (uint8_t)(z[0] & 248);            // clamp
    unpack(x, point);
    for (i = 0; i < 16; i++) {
      b[i] = x[i];
      d[i] = a[i] = c[i] = 0;
    }
    a[0] = d[0] = 1;
    for (i = 254; i >= 0; --i) {
      r = (z[i >> 3] >> (i & 7)) & 1;
      sel(a, b, (int)r);
      sel(c, d, (int)r);
      add(e, a, c);
      sub(a, a, c);
      add(c, b, d);
      sub(b, b, d);
      fsqr(d, e);
      fsqr(f, a);
      mul(a, c, a);
      mul(c, b, e);
      add(e, a, c);
      sub(a, a, c);
      fsqr(b, a);
      sub(c, d, f);
      mul(a, c, k121665());
      add(a, a, d);
      mul(c, c, a);
      mul(a, d, f);
      mul(d, b, x);
      fsqr(b, e);
      sel(a, b, (int)r);
      sel(c, d, (int)r);
    }
    inv(c, c);
    mul(a, a, c);
    pack(out, a);
  }

 private:
  typedef int64_t gfe[16];  // field element: 16 little-endian 16-bit limbs

  static const gfe& k121665() {
    static const gfe v = {0xDB41, 1};
    return v;
  }

  static void carry(gfe o) {
    int i;
    int64_t c;
    for (i = 0; i < 16; i++) {
      o[i] += (1LL << 16);
      c = o[i] >> 16;
      o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
      o[i] -= c << 16;
    }
  }

  static void sel(gfe p, gfe q, int b) {
    int64_t t, i, c = ~((int64_t)b - 1);
    for (i = 0; i < 16; i++) {
      t = c & (p[i] ^ q[i]);
      p[i] ^= t;
      q[i] ^= t;
    }
  }

  static void pack(uint8_t* o, const gfe n) {
    int i, j, b;
    gfe m, t;
    for (i = 0; i < 16; i++) t[i] = n[i];
    carry(t);
    carry(t);
    carry(t);
    for (j = 0; j < 2; j++) {
      m[0] = t[0] - 0xffed;
      for (i = 1; i < 15; i++) {
        m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
        m[i - 1] &= 0xffff;
      }
      m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
      b = (int)((m[15] >> 16) & 1);
      m[14] &= 0xffff;
      sel(t, m, 1 - b);
    }
    for (i = 0; i < 16; i++) {
      o[2 * i] = (uint8_t)(t[i] & 0xff);
      o[2 * i + 1] = (uint8_t)(t[i] >> 8);
    }
  }

  static void unpack(gfe o, const uint8_t* n) {
    int i;
    for (i = 0; i < 16; i++) o[i] = n[2 * i] + ((int64_t)n[2 * i + 1] << 8);
    o[15] &= 0x7fff;
  }

  static void add(gfe o, const gfe a, const gfe b) {
    for (int i = 0; i < 16; i++) o[i] = a[i] + b[i];
  }
  static void sub(gfe o, const gfe a, const gfe b) {
    for (int i = 0; i < 16; i++) o[i] = a[i] - b[i];
  }
  static void mul(gfe o, const gfe a, const gfe b) {
    int64_t i, j, t[31];
    for (i = 0; i < 31; i++) t[i] = 0;
    for (i = 0; i < 16; i++)
      for (j = 0; j < 16; j++) t[i + j] += a[i] * b[j];
    for (i = 0; i < 15; i++) t[i] += 38 * t[i + 16];
    for (i = 0; i < 16; i++) o[i] = t[i];
    carry(o);
    carry(o);
  }
  static void fsqr(gfe o, const gfe a) { mul(o, a, a); }

  static void inv(gfe o, const gfe in) {
    gfe c;
    int a;
    for (a = 0; a < 16; a++) c[a] = in[a];
    for (a = 253; a >= 0; a--) {
      fsqr(c, c);
      if (a != 2 && a != 4) mul(c, c, in);
    }
    for (a = 0; a < 16; a++) o[a] = c[a];
  }
};

}  // namespace nzb

#endif  // ARDUINONRF_ZIGBEE_CURVE25519_H
