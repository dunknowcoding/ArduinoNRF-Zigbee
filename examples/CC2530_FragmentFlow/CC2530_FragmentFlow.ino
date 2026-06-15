/*
  CC2530_FragmentFlow - APS fragmentation end-to-end self-test.

  A long APS payload (more than one frame's worth) is split by ZigbeeApsFragmenter
  into a sequence of fragment APDUs; each is sent as a normal frame and the
  receiver feeds them to ZigbeeApsReassembler, which rebuilds the original ASDU.
  This sketch exercises the full send->reassemble flow (in order and out of
  order) without radio. Runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

static const uint8_t kBlock = 50;
static const uint16_t kLen = 200;          // 4 blocks of 50
uint8_t source[kLen];
uint8_t frags[8][64];                       // captured fragment APDUs
uint8_t fragLen[8];

void buildSource() {
  for (uint16_t i = 0; i < kLen; ++i) source[i] = (uint8_t)(i * 7 + 1);
}

uint8_t fragmentAll(uint8_t counter) {
  ZigbeeApsFragmenter frag;
  frag.begin(source, kLen, kBlock, /*dstEp=*/1, /*cluster=*/0x1042,
             /*profile=*/0x0104, /*srcEp=*/1, counter);
  uint8_t n = 0;
  while (!frag.done() && n < 8) {
    fragLen[n] = frag.next(frags[n], sizeof(frags[n]));
    if (fragLen[n] == 0) break;
    ++n;
  }
  return n;  // number of fragments produced
}

bool reassemble(const uint8_t* order, uint8_t count, uint8_t counter) {
  static uint8_t buf[256];
  ZigbeeApsReassembler re;
  re.begin(buf, sizeof(buf), kBlock);
  bool complete = false;
  for (uint8_t k = 0; k < count; ++k) {
    ApsFragmentInfo info;
    if (!ZigbeeApsFragment::parseFragment(frags[order[k]], fragLen[order[k]], info))
      return false;
    complete = re.addBlock(info);
  }
  if (!complete || re.length() != kLen) return false;
  for (uint16_t i = 0; i < kLen; ++i) if (re.payload()[i] != source[i]) return false;
  (void)counter;
  return true;
}

void testFlow() {
  Serial.println("Fragment -> reassemble (200 B, block 50):");
  uint8_t n = fragmentAll(0x11);
  check(n == 4, "200 B / 50 = 4 fragments");

  // First fragment carries the total count; later ones are indexed.
  ApsFragmentInfo f0, f1;
  ZigbeeApsFragment::parseFragment(frags[0], fragLen[0], f0);
  ZigbeeApsFragment::parseFragment(frags[1], fragLen[1], f1);
  check(f0.firstBlock && f0.blockNumber == 4, "first fragment carries total = 4");
  check(!f1.firstBlock && f1.blockNumber == 1, "second fragment is block 1");
  check(fragLen[3] == ZigbeeApsFragment::kHeaderLen + (kLen - 3 * kBlock),
        "final fragment carries the remainder");

  const uint8_t inOrder[4] = {0, 1, 2, 3};
  check(reassemble(inOrder, 4, 0x11), "in-order reassembly matches the original");

  const uint8_t shuffled[4] = {3, 0, 2, 1};
  check(reassemble(shuffled, 4, 0x11), "out-of-order reassembly matches");

  // A duplicate block must not corrupt the result.
  const uint8_t withDup[5] = {0, 1, 1, 2, 3};
  check(reassemble(withDup, 5, 0x11), "duplicate block tolerated");
}

void testShort() {
  Serial.println("Short payload (single block):");
  ZigbeeApsFragmenter frag;
  uint8_t one[10] = {1,2,3,4,5,6,7,8,9,10};
  frag.begin(one, 10, kBlock, 1, 0x1042, 0x0104, 1, 0x22);
  check(frag.totalBlocks() == 1, "10 B < block -> 1 fragment");
  uint8_t out[64];
  uint8_t m = frag.next(out, sizeof(out));
  ApsFragmentInfo info;
  ZigbeeApsFragment::parseFragment(out, m, info);
  check(info.firstBlock && info.blockNumber == 1 && info.payloadLen == 10,
        "single fragment is first, total 1, full payload");
  check(frag.done(), "fragmenter done after one block");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee APS fragment-flow self-test ===");

  buildSource();
  testFlow();
  testShort();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
