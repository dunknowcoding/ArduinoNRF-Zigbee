/*
  ZigbeeInstallCode.h - Zigbee install-code -> Trust Center link key.

  Instead of the well-known global TC link key "ZigBeeAlliance09", a Zigbee 3.0
  device can be commissioned with a per-device INSTALL CODE - a random secret
  printed on the device (often as a QR code) together with a 16-bit CRC. The
  Trust Center, told the same install code out of band, derives the device's
  unique link key from it and uses that to APS-encrypt the Transport-Key (see
  ZigbeeApsSecurity). The link key is the AES-MMO-128 hash of the full install
  code (its data bytes followed by the CRC).

  This header validates an install code's CRC and derives the link key. Install
  codes are 6/8/12/16 data bytes + a 2-byte CRC, so 8/10/14/18 bytes total.
  The CRC is CRC-16/X-25 (reflected, init 0xFFFF, xorout 0xFFFF) and is stored
  LITTLE-endian (least significant byte first) at the end of the code, matching
  real Zigbee install codes - e.g. the install code
  83FED3407A939723A5C639B26916D505C3B5 has CRC value 0xB5C3 stored as the trailing
  bytes C3 B5, and derives the link key 66B6900981E1EE3CA4206B6B861C02BB.
*/
#ifndef NIUS_ZIGBEE_INSTALL_CODE_H
#define NIUS_ZIGBEE_INSTALL_CODE_H

#include <Arduino.h>

#include "ZigbeeApsSecurity.h"  // AES-MMO hash

namespace nzb {

class ZigbeeInstallCode {
 public:
  /** CRC-16/X-25: reflected, polynomial 0x1021, init 0xFFFF, final xor 0xFFFF.
      The standard check value for the ASCII string "123456789" is 0x906E. */
  static uint16_t crc16(const uint8_t* data, uint8_t len) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; ++i) {
      crc ^= data[i];
      for (uint8_t b = 0; b < 8; ++b)
        crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0x8408) : (uint16_t)(crc >> 1);
    }
    return (uint16_t)(crc ^ 0xFFFF);
  }

  /** True if @p len is a valid install-code length (8/10/14/18) and the trailing
      2-byte little-endian CRC matches a CRC computed over the data bytes. */
  static bool validate(const uint8_t* codeWithCrc, uint8_t len) {
    if (!codeWithCrc) return false;
    if (len != 8 && len != 10 && len != 14 && len != 18) return false;
    uint16_t computed = crc16(codeWithCrc, (uint8_t)(len - 2));
    uint16_t stored = (uint16_t)(codeWithCrc[len - 2] | (codeWithCrc[len - 1] << 8));
    return computed == stored;
  }

  /** Append the correct CRC to @p data (the code body) to form a full install
      code in @p out. @p dataLen must be 6/8/12/16. @return total length (dataLen
      + 2), or 0 on error. */
  static uint8_t build(const uint8_t* data, uint8_t dataLen, uint8_t* out,
                       uint8_t outMax) {
    if (!data || !out) return 0;
    if (dataLen != 6 && dataLen != 8 && dataLen != 12 && dataLen != 16) return 0;
    if (outMax < (uint8_t)(dataLen + 2)) return 0;
    memcpy(out, data, dataLen);
    uint16_t crc = crc16(data, dataLen);
    out[dataLen] = (uint8_t)(crc & 0xFF);  // little-endian (Zigbee install-code order)
    out[dataLen + 1] = (uint8_t)(crc >> 8);
    return (uint8_t)(dataLen + 2);
  }

  /** Derive the per-device TC link key from a (CRC-valid) install code: the
      AES-MMO-128 hash of the full code, data bytes + CRC. @return false if the
      CRC is invalid or the hash fails. */
  static bool deriveLinkKey(const uint8_t* codeWithCrc, uint8_t len,
                            uint8_t out[16]) {
    if (!validate(codeWithCrc, len)) return false;
    return ZigbeeApsSecurity::aesMmoHash(codeWithCrc, len, out);
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_INSTALL_CODE_H
