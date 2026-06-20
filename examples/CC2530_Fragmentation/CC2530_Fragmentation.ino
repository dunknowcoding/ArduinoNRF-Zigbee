/*
  CC2530_Fragmentation - Zigbee APS fragmentation of a long ASDU.

  A single APS frame only carries what fits one 802.15.4 PSDU. A larger
  payload (a long attribute report, a Mgmt_Lqi response with many neighbors,
  a firmware chunk) is split into blocks carried by several APS frames with an
  APS extended header, and reassembled by the receiver. This sketch is
  self-contained: it fragments a long buffer, feeds the blocks back through
  ZigbeeApsReassembler (in order and out of order), and checks the result.
*/

#include <CC2530Radio.h>

static const uint8_t DST_EP = 1, SRC_EP = 1;
static const uint16_t CLUSTER = 0x0702, PROFILE = 0x0104;
static const uint8_t APS_COUNTER = 0x55;
static const uint8_t BLOCK_SIZE = 40;

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

// Build a recognizable long ASDU (180 bytes).
static const uint16_t ASDU_LEN = 180;
uint8_t asdu[ASDU_LEN];
uint8_t reasmBuf[256];

void fillAsdu() {
  for (uint16_t i = 0; i < ASDU_LEN; ++i) asdu[i] = (uint8_t)(i * 7 + 3);
}

uint8_t blockCount() {
  return (uint8_t)((ASDU_LEN + BLOCK_SIZE - 1) / BLOCK_SIZE);
}

// Build all fragment APDUs into frames[][], returning the count.
uint8_t buildAll(uint8_t frames[][BLOCK_SIZE + 16], uint8_t* frameLens) {
  uint8_t total = blockCount();
  for (uint8_t i = 0; i < total; ++i) {
    uint16_t off = (uint16_t)i * BLOCK_SIZE;
    uint8_t len = (uint8_t)((off + BLOCK_SIZE <= ASDU_LEN) ? BLOCK_SIZE
                                                           : (ASDU_LEN - off));
    bool first = (i == 0);
    uint8_t blockNum = first ? total : i;  // first block carries total count
    frameLens[i] = ZigbeeApsFragment::buildFragment(
        frames[i], BLOCK_SIZE + 16, DST_EP, CLUSTER, PROFILE, SRC_EP,
        APS_COUNTER, first, blockNum, &asdu[off], len);
  }
  return total;
}

bool reassembleAndCheck(uint8_t frames[][BLOCK_SIZE + 16], uint8_t* frameLens,
                        uint8_t total, const uint8_t* order) {
  ZigbeeApsReassembler reasm;
  reasm.begin(reasmBuf, sizeof(reasmBuf), BLOCK_SIZE);
  bool done = false;
  for (uint8_t k = 0; k < total; ++k) {
    uint8_t idx = order[k];
    ApsFragmentInfo info;
    if (!ZigbeeApsFragment::parseFragment(frames[idx], frameLens[idx], info)) {
      return false;
    }
    done = reasm.addBlock(info);
  }
  if (!done || reasm.length() != ASDU_LEN) return false;
  for (uint16_t i = 0; i < ASDU_LEN; ++i) {
    if (reasm.payload()[i] != asdu[i]) return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee APS fragmentation self-test ===");

  fillAsdu();
  uint8_t frames[8][BLOCK_SIZE + 16];
  uint8_t frameLens[8];
  uint8_t total = buildAll(frames, frameLens);

  Serial.print("ASDU "); Serial.print(ASDU_LEN);
  Serial.print(" B -> "); Serial.print(total);
  Serial.print(" fragments of <= "); Serial.print(BLOCK_SIZE);
  Serial.println(" B");
  check(total == 5, "180 B / 40 -> 5 fragments");

  // First fragment carries the total count; check the header decode.
  ApsFragmentInfo f0;
  check(ZigbeeApsFragment::parseFragment(frames[0], frameLens[0], f0) &&
            f0.firstBlock && f0.blockNumber == total &&
            f0.clusterId == CLUSTER && f0.counter == APS_COUNTER,
        "first fragment header (first=1, count, cluster, counter)");

  ApsFragmentInfo f1;
  check(ZigbeeApsFragment::parseFragment(frames[1], frameLens[1], f1) &&
            !f1.firstBlock && f1.blockNumber == 1,
        "second fragment header (first=0, index=1)");

  // Regression guard: a built fragment MUST also be accepted by the real APS data
  // parser an on-air receiver runs (ZigbeeAps::parseDataFrame) as an
  // extended-header UNICAST frame. The ext-header bit once collided with the
  // delivery-mode field, so every fragment was silently rejected over the air.
  ApsDataFrame ad;
  check(ZigbeeAps::parseDataFrame(frames[0], frameLens[0], ad) &&
            ad.extendedHeader && ad.firstBlock && ad.blockNumber == total &&
            ad.deliveryMode == APS_DELIVERY_UNICAST && ad.clusterId == CLUSTER,
        "fragment accepted by ZigbeeAps::parseDataFrame (ext header, unicast)");

  // In-order reassembly.
  uint8_t inOrder[8];
  for (uint8_t i = 0; i < total; ++i) inOrder[i] = i;
  check(reassembleAndCheck(frames, frameLens, total, inOrder),
        "reassemble in order -> original ASDU");

  // Out-of-order reassembly (reverse).
  uint8_t reverse[8];
  for (uint8_t i = 0; i < total; ++i) reverse[i] = (uint8_t)(total - 1 - i);
  check(reassembleAndCheck(frames, frameLens, total, reverse),
        "reassemble reversed -> original ASDU");

  // Duplicate a block in the middle - must still reassemble correctly.
  uint8_t dup[9];
  uint8_t dn = 0;
  for (uint8_t i = 0; i < total; ++i) {
    dup[dn++] = i;
    if (i == 2) dup[dn++] = 2;  // send block 2 twice
  }
  check(reassembleAndCheck(frames, frameLens, dn, dup),
        "reassemble with a duplicate block -> original ASDU");

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
