/*
  ZigbeeGreenPower.h - Zigbee Green Power: GPDF frames, GP frame security, the
  commissioning command, and a sink table.

  Green Power devices (GPDs) are ultra-low-power, often battery-less switches and
  sensors that are NOT full Zigbee nodes. They emit Green Power Data Frames
  (GPDFs) - a stub NWK frame (protocol version 3) carrying a 4-byte GPD source
  id, a frame counter, a command, and (optionally) an AES-CCM* MIC. A GP sink
  (in the coordinator/app) keeps a sink table of commissioned GPDs and decrypts
  their frames.

  This header builds/parses the GPDF, secures/opens it with AES-CCM* (the same
  hardware-verified core as the NWK/APS layers, with the GP nonce =
  srcId||srcId||frameCounter||securityControl), builds/parses the GP
  commissioning command (0xE0), and provides a small sink table with per-GPD
  frame-counter replay protection.

  Note: the GPDF security-control/level bit layout follows the Green Power spec's
  common case (full 4-byte frame counter + 4-byte MIC, encrypted command);
  confirm the security-control nonce byte against a reference vector before
  interoperating with certified GP devices.
*/
#ifndef NIUS_ZIGBEE_GREEN_POWER_H
#define NIUS_ZIGBEE_GREEN_POWER_H

#include <Arduino.h>

#include "ZigbeeCcmStar.h"

namespace nzb {

// GPD command ids (a subset: commissioning + the generic switch/level set).
enum GpdCommandId : uint8_t {
  GPD_CMD_IDENTIFY = 0x00,
  GPD_CMD_OFF = 0x20,
  GPD_CMD_ON = 0x21,
  GPD_CMD_TOGGLE = 0x22,
  GPD_CMD_LEVEL_UP = 0x30,
  GPD_CMD_LEVEL_DOWN = 0x31,
  GPD_CMD_COMMISSIONING = 0xE0,
  GPD_CMD_DECOMMISSIONING = 0xE1,
};

// GP security levels (ext NWK frame control bits 3-4).
enum GpSecurityLevel : uint8_t {
  GP_SEC_NONE = 0,
  GP_SEC_FC_MIC = 2,      // 4-byte frame counter + 4-byte MIC, cleartext command
  GP_SEC_ENC_FC_MIC = 3,  // encrypted command + 4-byte FC + 4-byte MIC
};

struct GpdfFrame {
  bool valid;
  uint8_t applicationId;  // 0 = GPD source id
  uint8_t securityLevel;
  bool rxAfterTx;
  uint32_t srcId;
  uint32_t frameCounter;
  uint8_t commandId;
  uint8_t payload[48];
  uint8_t payloadLen;
};

// A parsed GP commissioning command (0xE0) - what a GPD announces about itself.
struct GpCommissioningCommand {
  uint8_t deviceId;
  uint8_t options;
  bool keyPresent;
  uint8_t key[16];
  bool outgoingCounterPresent;
  uint32_t outgoingCounter;
};

class ZigbeeGreenPower {
 public:
  static const uint8_t kMicLen = 4;
  static const uint8_t kGpProtocolVersion = 3;
  static const uint8_t kNwkFcExtension = 0x80;  // bit 7

  // ----------------------------------------------------- unsecured GPDF
  /** Build an unsecured GPDF (security level 0) - used for commissioning. The
      stub NWK header is frame control(1) + ext FC(1) + srcId(4), then the
      command id and payload. */
  static uint8_t buildUnsecured(uint8_t* out, uint8_t outMax, uint32_t srcId,
                                uint8_t commandId, const uint8_t* payload,
                                uint8_t payloadLen) {
    uint8_t need = (uint8_t)(2 + 4 + 1 + payloadLen);
    if (!out || outMax < need) return 0;
    out[0] = (uint8_t)(kGpProtocolVersion << 2) | kNwkFcExtension;
    out[1] = extFc(/*appId=*/0, GP_SEC_NONE, /*rxAfterTx=*/false);
    putLe32(&out[2], srcId);
    out[6] = commandId;
    for (uint8_t i = 0; i < payloadLen; ++i) out[7 + i] = payload[i];
    return need;
  }

  // ------------------------------------------------------- secured GPDF
  /** Build a secured GPDF (level 3: encrypted command + 4-byte FC + 4-byte MIC).
      Layout: FC(1) extFC(1) srcId(4) frameCounter(4) | enc[commandId|payload] |
      MIC(4). */
  static uint8_t secure(uint8_t* out, uint8_t outMax, const uint8_t key[16],
                        uint32_t srcId, uint32_t frameCounter, uint8_t commandId,
                        const uint8_t* payload, uint8_t payloadLen) {
    uint8_t encLen = (uint8_t)(1 + payloadLen);  // commandId + payload
    uint8_t need = (uint8_t)(2 + 4 + 4 + encLen + kMicLen);
    if (!out || outMax < need || encLen > 48) return 0;

    out[0] = (uint8_t)(kGpProtocolVersion << 2) | kNwkFcExtension;
    out[1] = extFc(0, GP_SEC_ENC_FC_MIC, false);
    putLe32(&out[2], srcId);
    putLe32(&out[6], frameCounter);

    uint8_t nonce[13];
    gpNonce(srcId, frameCounter, nonce);
    uint8_t aad[10];  // FC + extFC + srcId + frameCounter (the header)
    memcpy(aad, out, 10);

    uint8_t plain[48];
    plain[0] = commandId;
    for (uint8_t i = 0; i < payloadLen; ++i) plain[1 + i] = payload[i];

    uint8_t* cipher = &out[10];
    uint8_t mic[kMicLen];
    if (!ccmStarCrypt(true, key, kMicLen, nonce, aad, 10, plain, encLen, cipher,
                      nullptr, mic)) {
      return 0;
    }
    memcpy(&out[10 + encLen], mic, kMicLen);
    return need;
  }

  /** Parse a GPDF header (both unsecured and secured). For a secured frame the
      command/payload are still encrypted - call open() to recover them. */
  static bool parse(const uint8_t* in, uint8_t len, GpdfFrame& f) {
    f = GpdfFrame();
    if (!in || len < 6) return false;
    uint8_t fc = in[0];
    if (((fc >> 2) & 0x0F) != kGpProtocolVersion) return false;
    uint8_t idx = 1;
    uint8_t appId = 0, secLevel = 0;
    bool rxAfterTx = false;
    if (fc & kNwkFcExtension) {
      uint8_t e = in[1];
      appId = (uint8_t)(e & 0x07);
      secLevel = (uint8_t)((e >> 3) & 0x03);
      rxAfterTx = (e & 0x40) != 0;
      idx = 2;
    }
    if (appId != 0) return false;  // only GPD source id supported here
    if ((uint8_t)(idx + 4) > len) return false;
    f.srcId = getLe32(&in[idx]);
    idx += 4;
    if (secLevel >= GP_SEC_FC_MIC) {
      if ((uint8_t)(idx + 4) > len) return false;
      f.frameCounter = getLe32(&in[idx]);
      idx += 4;
    }
    f.valid = true;
    f.applicationId = appId;
    f.securityLevel = secLevel;
    f.rxAfterTx = rxAfterTx;
    if (secLevel == GP_SEC_NONE) {
      if (idx >= len) return false;
      f.commandId = in[idx++];
      f.payloadLen = (uint8_t)(len - idx);
      if (f.payloadLen > sizeof(f.payload)) return false;
      memcpy(f.payload, &in[idx], f.payloadLen);
    }
    return true;
  }

  /** Verify + decrypt a secured (level 3) GPDF into @p f (commandId + payload).
      @return false on MIC failure / malformed input. */
  static bool open(const uint8_t* in, uint8_t len, const uint8_t key[16],
                   GpdfFrame& f) {
    if (!parse(in, len, f)) return false;
    if (f.securityLevel != GP_SEC_ENC_FC_MIC) return false;
    // header(10) | cipher | mic(4)
    if (len < 10 + kMicLen) return false;
    uint8_t encLen = (uint8_t)(len - 10 - kMicLen);
    if (encLen < 1 || encLen > 48) return false;

    uint8_t nonce[13];
    gpNonce(f.srcId, f.frameCounter, nonce);
    uint8_t aad[10];
    memcpy(aad, in, 10);

    // CTR-decrypt the cipher, then authenticate.
    uint8_t plain[48];
    const uint8_t* cipher = &in[10];
    ctrCrypt(key, nonce, cipher, encLen, plain);
    const uint8_t* mic = &in[10 + encLen];
    if (!ccmStarCrypt(false, key, kMicLen, nonce, aad, 10, plain, encLen,
                      nullptr, mic, nullptr)) {
      return false;
    }
    f.commandId = plain[0];
    f.payloadLen = (uint8_t)(encLen - 1);
    memcpy(f.payload, &plain[1], f.payloadLen);
    return true;
  }

  // ---------------------------------------- GP commissioning command (0xE0)
  /** Payload: device id(1) + options(1) [+ ext options(1)] [+ GPD key(16)]
      [+ outgoing counter(4)]. Built here with the common fields. */
  static uint8_t buildCommissioning(uint8_t* out, uint8_t outMax,
                                    const GpCommissioningCommand& c) {
    uint8_t need = 2;
    if (c.keyPresent) need += 16;
    if (c.outgoingCounterPresent) need += 4;
    if (!out || outMax < need) return 0;
    out[0] = c.deviceId;
    out[1] = c.options;
    uint8_t idx = 2;
    if (c.keyPresent) { memcpy(&out[idx], c.key, 16); idx += 16; }
    if (c.outgoingCounterPresent) { putLe32(&out[idx], c.outgoingCounter); idx += 4; }
    return idx;
  }
  static bool parseCommissioning(const uint8_t* p, uint8_t len,
                                 GpCommissioningCommand& c) {
    c = GpCommissioningCommand();
    if (!p || len < 2) return false;
    c.deviceId = p[0];
    c.options = p[1];
    uint8_t idx = 2;
    // options bit 5 = GPD key present (per the common encoding used here).
    c.keyPresent = (c.options & 0x20) != 0;
    if (c.keyPresent) {
      if ((uint8_t)(idx + 16) > len) return false;
      memcpy(c.key, &p[idx], 16);
      idx += 16;
    }
    c.outgoingCounterPresent = (c.options & 0x40) != 0;
    if (c.outgoingCounterPresent) {
      if ((uint8_t)(idx + 4) > len) return false;
      c.outgoingCounter = getLe32(&p[idx]);
      idx += 4;
    }
    return true;
  }

 private:
  static uint8_t extFc(uint8_t appId, uint8_t secLevel, bool rxAfterTx) {
    return (uint8_t)((appId & 0x07) | ((secLevel & 0x03) << 3) |
                     (rxAfterTx ? 0x40 : 0));
  }
  // GP nonce: srcId || srcId || frameCounter || securityControl(0x05).
  static void gpNonce(uint32_t srcId, uint32_t frameCounter, uint8_t nonce[13]) {
    putLe32(&nonce[0], srcId);
    putLe32(&nonce[4], srcId);
    putLe32(&nonce[8], frameCounter);
    nonce[12] = 0x05;  // security control: GPD, encrypted (confirm vs ref)
  }
  // CTR keystream (A1, A2, ...) over `in` into `out` (used to decrypt; the
  // matching encrypt happens inside ccmStarCrypt).
  static void ctrCrypt(const uint8_t key[16], const uint8_t nonce[13],
                       const uint8_t* in, uint8_t len, uint8_t* out) {
    uint8_t a[16];
    a[0] = 0x01;
    memcpy(&a[1], nonce, 13);
    uint16_t counter = 1;
    uint8_t pos = 0;
    while (pos < len) {
      a[14] = (uint8_t)(counter >> 8);
      a[15] = (uint8_t)(counter & 0xFF);
      ++counter;
      uint8_t ks[16];
      NrfEcb::encrypt(key, a, ks);
      for (uint8_t i = 0; i < 16 && pos < len; ++i, ++pos) out[pos] = in[pos] ^ ks[i];
    }
  }
  static void putLe32(uint8_t* p, uint32_t v) {
    for (uint8_t i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
  }
  static uint32_t getLe32(const uint8_t* p) {
    uint32_t v = 0;
    for (uint8_t i = 0; i < 4; ++i) v |= (uint32_t)p[i] << (8 * i);
    return v;
  }
};

// ------------------------------------------------------------- GP sink table
struct GpSinkEntry {
  bool used;
  uint32_t srcId;
  uint8_t deviceId;
  uint8_t key[16];
  uint32_t lastFrameCounter;
};

class ZigbeeGpSinkTable {
 public:
  ZigbeeGpSinkTable() : entries_(nullptr), capacity_(0) {}
  void begin(GpSinkEntry* storage, uint8_t capacity) {
    entries_ = storage;
    capacity_ = capacity;
    for (uint8_t i = 0; i < capacity_; ++i) entries_[i] = GpSinkEntry();
  }

  /** Commission (or update) a GPD: store its key + device id. Idempotent. */
  GpSinkEntry* commission(uint32_t srcId, uint8_t deviceId, const uint8_t key[16]) {
    GpSinkEntry* e = find(srcId);
    if (!e) e = firstFree();
    if (!e) return nullptr;
    e->used = true;
    e->srcId = srcId;
    e->deviceId = deviceId;
    memcpy(e->key, key, 16);
    e->lastFrameCounter = 0;
    return e;
  }

  GpSinkEntry* find(uint32_t srcId) {
    for (uint8_t i = 0; i < capacity_; ++i)
      if (entries_[i].used && entries_[i].srcId == srcId) return &entries_[i];
    return nullptr;
  }

  /** Accept a frame counter for @p srcId only if it advances (anti-replay). */
  bool checkAndUpdateCounter(uint32_t srcId, uint32_t frameCounter) {
    GpSinkEntry* e = find(srcId);
    if (!e) return false;
    if (frameCounter <= e->lastFrameCounter && e->lastFrameCounter != 0) return false;
    e->lastFrameCounter = frameCounter;
    return true;
  }

  bool remove(uint32_t srcId) {
    GpSinkEntry* e = find(srcId);
    if (!e) return false;
    *e = GpSinkEntry();
    return true;
  }

 private:
  GpSinkEntry* firstFree() {
    for (uint8_t i = 0; i < capacity_; ++i)
      if (!entries_[i].used) return &entries_[i];
    return nullptr;
  }
  GpSinkEntry* entries_;
  uint8_t capacity_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_GREEN_POWER_H
