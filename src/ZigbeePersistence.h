/*
  ZigbeePersistence.h - serialize/restore Zigbee network state across reboots.

  Real Zigbee devices keep their network identity and the outgoing NWK security
  frame counter in non-volatile storage so a power cycle does not force a
  re-join and (critically) does not rewind the frame counter - a rewound
  counter lets old secured frames replay and is rejected by peers.

  This class only turns the state into a fixed little-endian blob (magic +
  version + fields + CRC-16) and back; the sketch decides WHERE to store it.
  On ArduinoNRF the natural backing store is the core's EEPROM library, which
  already manages a wear-levelled flash region - so a sketch does
  EEPROM.put(0, blob) / EEPROM.get(0, blob) without this library touching
  flash addresses itself.

  The frame counter should be saved with a margin (e.g. store counter + 1024
  and resume from there) so a crash between saves cannot reuse a counter; the
  sketch picks the policy.
*/
#ifndef NIUS_ZIGBEE_PERSISTENCE_H
#define NIUS_ZIGBEE_PERSISTENCE_H

#include <Arduino.h>

namespace nzb {

struct ZigbeePersistentState {
  uint16_t panId;
  uint64_t extendedPanId;
  uint8_t channel;
  uint16_t nwkAddress;
  uint16_t parentAddress;
  uint8_t depth;
  uint8_t deviceType;
  uint64_t ieeeAddress;
  uint32_t outgoingFrameCounter;
  uint8_t keySequence;
};

class ZigbeePersistence {
 public:
  static const uint32_t kMagic = 0x4E5A4231UL;  // "NZB1"
  static const uint8_t kVersion = 1;
  // magic4 ver1 pan2 extpan8 ch1 nwk2 parent2 depth1 type1 ieee8 fc4 keyseq1 crc2
  static const uint8_t kBlobSize = 37;

  static uint8_t serialize(const ZigbeePersistentState& s, uint8_t* out,
                           uint8_t outMax) {
    if (!out || outMax < kBlobSize) return 0;
    uint8_t* p = out;
    putLe32(p, kMagic); p += 4;
    *p++ = kVersion;
    putLe16(p, s.panId); p += 2;
    putLe64(p, s.extendedPanId); p += 8;
    *p++ = s.channel;
    putLe16(p, s.nwkAddress); p += 2;
    putLe16(p, s.parentAddress); p += 2;
    *p++ = s.depth;
    *p++ = s.deviceType;
    putLe64(p, s.ieeeAddress); p += 8;
    putLe32(p, s.outgoingFrameCounter); p += 4;
    *p++ = s.keySequence;
    uint16_t crc = crc16(out, (uint8_t)(p - out));
    putLe16(p, crc); p += 2;
    return (uint8_t)(p - out);  // == kBlobSize
  }

  static bool deserialize(const uint8_t* in, uint8_t len,
                          ZigbeePersistentState& s) {
    s = ZigbeePersistentState();
    if (!in || len < kBlobSize) return false;
    if (getLe32(in) != kMagic) return false;
    if (in[4] != kVersion) return false;
    uint16_t storedCrc = getLe16(&in[kBlobSize - 2]);
    if (crc16(in, kBlobSize - 2) != storedCrc) return false;

    const uint8_t* p = &in[5];
    s.panId = getLe16(p); p += 2;
    s.extendedPanId = getLe64(p); p += 8;
    s.channel = *p++;
    s.nwkAddress = getLe16(p); p += 2;
    s.parentAddress = getLe16(p); p += 2;
    s.depth = *p++;
    s.deviceType = *p++;
    s.ieeeAddress = getLe64(p); p += 8;
    s.outgoingFrameCounter = getLe32(p); p += 4;
    s.keySequence = *p++;
    return true;
  }

 private:
  static void putLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
  }
  static void putLe32(uint8_t* p, uint32_t v) {
    for (uint8_t i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
  }
  static void putLe64(uint8_t* p, uint64_t v) {
    for (uint8_t i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (8 * i));
  }
  static uint16_t getLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  }
  static uint32_t getLe32(const uint8_t* p) {
    uint32_t v = 0;
    for (uint8_t i = 0; i < 4; ++i) v |= (uint32_t)p[i] << (8 * i);
    return v;
  }
  static uint64_t getLe64(const uint8_t* p) {
    uint64_t v = 0;
    for (uint8_t i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (8 * i);
    return v;
  }
  // CRC-16/CCITT-FALSE.
  static uint16_t crc16(const uint8_t* p, uint8_t n) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < n; ++i) {
      crc ^= (uint16_t)p[i] << 8;
      for (uint8_t b = 0; b < 8; ++b) {
        crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
                             : (uint16_t)(crc << 1);
      }
    }
    return crc;
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_PERSISTENCE_H
