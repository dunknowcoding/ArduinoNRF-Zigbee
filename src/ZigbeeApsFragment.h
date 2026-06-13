/*
  ZigbeeApsFragment.h - Zigbee APS fragmentation (long payloads over the air).

  A single APS data frame can only carry what fits one 802.15.4 PSDU (~80 B of
  payload after NWK + APS + security headers). Zigbee fragments a larger ASDU
  across several APS frames using an APS extended header: the first frame's
  extended frame control marks "first block" and its block-number field holds
  the TOTAL block count; each following frame marks "not first" and carries its
  block index. The receiver reassembles by block number.

  This is the fragment frame tooling plus a reassembler; it is kept separate
  from ZigbeeAps (which builds plain, unfragmented data frames) so the common
  path stays simple. The sketch splits an ASDU into blocks, sends each with
  buildFragment(), and feeds received blocks to ZigbeeApsReassembler.

  Wire layout of a fragment APDU:
    FCF(1, ext-header bit set) dstEp(1) cluster(2) profile(2) srcEp(1)
    counter(1) extFCF(1) blockNumber(1) block-payload(...)
  where extFCF fragmentation bits = 01 (first) or 10 (subsequent).
*/
#ifndef NIUS_ZIGBEE_APS_FRAGMENT_H
#define NIUS_ZIGBEE_APS_FRAGMENT_H

#include <Arduino.h>

namespace nzb {

struct ApsFragmentInfo {
  bool valid;
  uint8_t frameType;
  uint8_t deliveryMode;
  bool ackRequest;
  uint8_t dstEndpoint;
  uint16_t clusterId;
  uint16_t profileId;
  uint8_t srcEndpoint;
  uint8_t counter;
  bool firstBlock;        // extFCF fragmentation == 01
  uint8_t blockNumber;    // first block: total block count; else block index
  const uint8_t* payload; // this block's data
  uint8_t payloadLen;
};

class ZigbeeApsFragment {
 public:
  static const uint8_t kBaseHeaderLen = 8;
  static const uint8_t kExtHeaderLen = 2;     // extFCF + blockNumber
  static const uint8_t kHeaderLen = kBaseHeaderLen + kExtHeaderLen;  // 10
  static const uint8_t kExtFcfFirst = 0x01;
  static const uint8_t kExtFcfSubsequent = 0x02;
  static const uint8_t kFcfExtHeaderBit = 0x08;  // APS FCF extended-header bit

  /** Build one fragment APDU.
      @param firstBlock     true for block 0 (blockNumber must then be the
                            TOTAL block count); false for later blocks.
      @param blockNumber    total-count (first) or block index (subsequent). */
  static uint8_t buildFragment(uint8_t* out, uint8_t outMax,
                               uint8_t dstEndpoint, uint16_t clusterId,
                               uint16_t profileId, uint8_t srcEndpoint,
                               uint8_t counter, bool firstBlock,
                               uint8_t blockNumber, const uint8_t* block,
                               uint8_t blockLen, bool ackRequest = false) {
    if (!out || (blockLen > 0 && !block)) return 0;
    if (outMax < (uint16_t)kHeaderLen + blockLen) return 0;

    uint8_t fcf = 0;                      // APS_FRAME_DATA
    fcf |= (uint8_t)(0 << 2);             // unicast
    fcf |= kFcfExtHeaderBit;              // extended header present
    if (ackRequest) fcf |= (1u << 6);

    out[0] = fcf;
    out[1] = dstEndpoint;
    out[2] = (uint8_t)clusterId;
    out[3] = (uint8_t)(clusterId >> 8);
    out[4] = (uint8_t)profileId;
    out[5] = (uint8_t)(profileId >> 8);
    out[6] = srcEndpoint;
    out[7] = counter;
    out[8] = firstBlock ? kExtFcfFirst : kExtFcfSubsequent;
    out[9] = blockNumber;
    for (uint8_t i = 0; i < blockLen; ++i) out[kHeaderLen + i] = block[i];
    return (uint8_t)(kHeaderLen + blockLen);
  }

  static bool parseFragment(const uint8_t* apdu, uint8_t len,
                            ApsFragmentInfo& info) {
    info = ApsFragmentInfo();
    if (!apdu || len < kHeaderLen) return false;
    uint8_t fcf = apdu[0];
    if ((fcf & kFcfExtHeaderBit) == 0) return false;   // not fragmented
    if ((uint8_t)(fcf & 0x03) != 0) return false;      // must be DATA
    info.valid = true;
    info.frameType = 0;
    info.deliveryMode = (uint8_t)((fcf >> 2) & 0x03);
    info.ackRequest = (fcf & (1u << 6)) != 0;
    info.dstEndpoint = apdu[1];
    info.clusterId = (uint16_t)apdu[2] | ((uint16_t)apdu[3] << 8);
    info.profileId = (uint16_t)apdu[4] | ((uint16_t)apdu[5] << 8);
    info.srcEndpoint = apdu[6];
    info.counter = apdu[7];
    info.firstBlock = (apdu[8] & 0x03) == kExtFcfFirst;
    info.blockNumber = apdu[9];
    info.payload = &apdu[kHeaderLen];
    info.payloadLen = (uint8_t)(len - kHeaderLen);
    return true;
  }
};

// Reassembles fragments of one ASDU into a caller-provided buffer. Tracks the
// expected total block count (learned from the first block) and which blocks
// have arrived, so out-of-order and duplicate blocks are handled.
class ZigbeeApsReassembler {
 public:
  ZigbeeApsReassembler()
      : buffer_(nullptr), bufMax_(0), length_(0), totalBlocks_(0),
        received_(0), counter_(0), haveCounter_(false), blockSize_(0) {}

  // blockSize is the size of every non-final fragment (the sender's block
  // size). It is passed in rather than inferred so out-of-order delivery -
  // where the short final block could arrive first - computes block offsets
  // correctly. In Zigbee this is the agreed max transfer block size.
  void begin(uint8_t* buffer, uint16_t bufMax, uint8_t blockSize) {
    buffer_ = buffer;
    bufMax_ = bufMax;
    reset();
    blockSize_ = blockSize;
  }

  void reset() {  // per-ASDU state only; blockSize_ is config, set by begin()
    length_ = 0;
    totalBlocks_ = 0;
    received_ = 0;
    haveCounter_ = false;
    for (uint8_t i = 0; i < kMaxBlocks; ++i) got_[i] = false;
  }

  /** Feed one parsed fragment. A new APS counter starts a fresh ASDU.
      @return true when all blocks have arrived (payload() is then complete). */
  bool addBlock(const ApsFragmentInfo& f) {
    if (!buffer_ || !f.valid) return false;

    if (!haveCounter_ || f.counter != counter_) {
      reset();
      counter_ = f.counter;
      haveCounter_ = true;
    }

    if (f.firstBlock) {
      totalBlocks_ = f.blockNumber;        // first block carries the count
      storeBlock(0, f.payload, f.payloadLen);
    } else {
      storeBlock(f.blockNumber, f.payload, f.payloadLen);
    }
    return complete();
  }

  bool complete() const {
    return totalBlocks_ > 0 && received_ >= totalBlocks_;
  }
  const uint8_t* payload() const { return buffer_; }
  uint16_t length() const { return length_; }

 private:
  static const uint8_t kMaxBlocks = 16;

  void storeBlock(uint8_t index, const uint8_t* data, uint8_t len) {
    if (index >= kMaxBlocks) return;
    uint16_t offset = (uint16_t)index * blockSize_;
    if (offset + len > bufMax_) return;
    for (uint8_t i = 0; i < len; ++i) buffer_[offset + i] = data[i];
    if (offset + len > length_) length_ = offset + len;
    if (!got_[index]) { got_[index] = true; ++received_; }
  }

  uint8_t* buffer_;
  uint16_t bufMax_;
  uint16_t length_;
  uint8_t totalBlocks_;
  uint8_t received_;
  uint8_t counter_;
  bool haveCounter_;
  uint8_t blockSize_;
  bool got_[kMaxBlocks];
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_APS_FRAGMENT_H
