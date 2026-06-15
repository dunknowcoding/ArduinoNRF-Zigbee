/*
  ZigbeeSecurity.h - Zigbee NWK-layer frame security (AES-CCM*, level 5).

  Implements ENC-MIC-32 network-key security host-side on the nRF52840: the
  CCM* mode (CBC-MAC + CTR) is assembled in software on top of the chip's
  hardware AES-128 ECB block (NrfEcb, ~7 us per block), and the secured NPDU
  is handed to the CC2530 as an ordinary PSDU payload. The CC2530 firmware
  needs no changes.

  Wire format follows the Zigbee spec: the NWK frame-control security bit is
  set and a 14-byte auxiliary header (security control, 4-byte frame counter,
  8-byte source IEEE, key sequence number) sits between the NWK header and
  the encrypted payload, followed by a 4-byte MIC. The security-level bits of
  the transmitted security control byte are zeroed on air and substituted
  with level 5 for nonce/MIC computation, as real stacks do.

  Replay protection keeps a small source-IEEE -> last-frame-counter table.

  Note: this secures NWK frames with a PRE-SHARED network key. Standard
  Zigbee key transport (Trust Center link keys delivering the network key at
  join time) is future work - see docs/STACK_ROADMAP.md.
*/
#ifndef NIUS_ZIGBEE_SECURITY_H
#define NIUS_ZIGBEE_SECURITY_H

#include <Arduino.h>

namespace nzb {

struct ZigbeeSecurityStats {
  uint32_t secured;      ///< frames encrypted for transmit
  uint32_t opened;       ///< frames verified + decrypted
  uint32_t micFailures;  ///< frames dropped: MIC mismatch / malformed aux
  uint32_t replays;      ///< frames dropped: stale frame counter
};

class ZigbeeSecurity {
 public:
  static const uint8_t kKeyLen = 16;
  static const uint8_t kMicLen = 4;    // ENC-MIC-32
  static const uint8_t kAuxLen = 14;   // ctrl(1) + fc(4) + ieee(8) + keyseq(1)
  static const uint8_t kLevel = 5;     // ENC-MIC-32
  static const uint8_t kMaxReplayPeers = 8;

  ZigbeeSecurity();

  void setNetworkKey(const uint8_t key[kKeyLen], uint8_t keySequence = 0);
  bool hasKey() const { return hasKey_; }
  uint8_t keySequence() const { return keySequence_; }

  /** Hold a second (alternate) network key for a key rotation: the Trust Center
      distributes a new key with a fresh sequence number, then broadcasts a
      Switch-Key. While both are held, incoming frames are decrypted with the
      key whose sequence number matches their aux header, so traffic secured
      under either key is accepted across the switchover. */
  void setAlternateKey(const uint8_t key[kKeyLen], uint8_t keySequence);
  bool hasAlternateKey() const { return hasAlt_; }
  uint8_t alternateKeySequence() const { return altKeySeq_; }

  /** Switch the active (outgoing) key to the one with @p keySequence. If it is
      the alternate key, it becomes active and the old active becomes the
      alternate (so late frames under the old key still decrypt). @return true
      if a matching key was made active. */
  bool switchKey(uint8_t keySequence);

  /** Secure a plain NPDU (NWK header + payload).
      @param npdu      plain frame, payload starting at @p headerLen
      @param srcIeee   our IEEE address (goes into the aux header / nonce)
      @param frameCounter  strictly increasing outgoing counter
      @param out       receives header + aux + ciphertext + MIC
      @return secured length, or 0 on error. */
  uint8_t secureNpdu(const uint8_t* npdu, uint8_t npduLen, uint8_t headerLen,
                     uint64_t srcIeee, uint32_t frameCounter, uint8_t* out,
                     uint8_t outMax);

  /** Verify and decrypt a secured NPDU.
      @param out receives the plain frame (security bit cleared) and payload.
      @return plain length, or 0 when the MIC fails / the counter replays. */
  uint8_t openNpdu(const uint8_t* npdu, uint8_t npduLen, uint8_t headerLen,
                   uint8_t* out, uint8_t outMax,
                   uint64_t* senderIeee = nullptr);

  const ZigbeeSecurityStats& stats() const { return stats_; }
  void resetReplayTable();

 private:
  struct ReplayEntry {
    bool used;
    uint64_t ieee;
    uint32_t lastCounter;
  };

  bool hasKey_;
  uint8_t key_[kKeyLen];
  uint8_t keySequence_;
  bool hasAlt_;
  uint8_t altKey_[kKeyLen];
  uint8_t altKeySeq_;
  ZigbeeSecurityStats stats_;
  ReplayEntry replay_[kMaxReplayPeers];

  bool replayCheckAndUpdate(uint64_t ieee, uint32_t counter);

  // CCM* core (M=4, L=2) on the hardware AES block.
  bool ccmStar(bool encrypt, const uint8_t nonce[13], const uint8_t* aad,
               uint8_t aadLen, const uint8_t* in, uint8_t inLen, uint8_t* out,
               const uint8_t* micIn, uint8_t* micOut);
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_SECURITY_H
