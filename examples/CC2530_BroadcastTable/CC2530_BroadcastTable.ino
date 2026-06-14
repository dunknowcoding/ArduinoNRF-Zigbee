/*
  CC2530_BroadcastTable - Broadcast Transaction Table self-test.

  The BTT (ZigbeeBroadcastTable) gives NWK broadcasts deduplication and
  passive acknowledgement: a router rebroadcasts a (source, sequence)
  transaction exactly once, then watches for neighbors rebroadcasting it as
  passive acks and retries only if too few are heard. This sketch self-tests
  that logic - no radio traffic needed; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testDedup() {
  Serial.println("Dedup (new vs duplicate):");
  BroadcastEntry storage[4];
  ZigbeeBroadcastTable btt(storage, 4);

  check(btt.recordIncoming(0x1234, 7, 1000), "first sight is NEW");
  check(!btt.recordIncoming(0x1234, 7, 1010), "same src+seq is DUPLICATE");
  check(!btt.recordIncoming(0x1234, 7, 1020), "second duplicate too");
  check(btt.recordIncoming(0x1234, 8, 1030), "different seq is NEW");
  check(btt.recordIncoming(0x5678, 7, 1040), "different src is NEW");
  check(btt.activeCount() == 3, "three distinct transactions tracked");

  // The duplicates are counted as passive acks on the original entry.
  BroadcastEntry* e = btt.find(0x1234, 7);
  check(e && e->passiveAcks == 2, "duplicates counted as passive acks");
}

void testPassiveAckSuppressesRetry() {
  Serial.println("Passive ack suppresses retry:");
  BroadcastEntry storage[4];
  ZigbeeBroadcastTable btt(storage, 4);

  // We originate a broadcast at t=0 and rebroadcast it (retries=1 after mark).
  BroadcastEntry* e = btt.recordOutgoing(0x0000, 42, 0);
  btt.markRebroadcast(e, 0);
  check(e && e->retries == 1, "outgoing recorded + rebroadcast once");

  // Need 2 passive acks, retry window 100 ms, max 3 retries.
  check(btt.due(50, 2, 100, 3) == nullptr, "not due before retry window");
  check(btt.due(150, 2, 100, 3) == e, "due after window with 0 acks");

  // Two neighbors rebroadcast -> enough passive acks -> no longer due.
  btt.markPassiveAck(0x0000, 42);
  check(btt.due(300, 2, 100, 3) == e, "still due with only 1 ack");
  btt.markPassiveAck(0x0000, 42);
  check(btt.due(300, 2, 100, 3) == nullptr, "not due once neededAcks reached");
}

void testRetryCap() {
  Serial.println("Retry cap:");
  BroadcastEntry storage[2];
  ZigbeeBroadcastTable btt(storage, 2);

  BroadcastEntry* e = btt.recordOutgoing(0x0001, 9, 0);
  btt.markRebroadcast(e, 0);  // retries = 1
  uint32_t now = 0;
  uint8_t rebroadcasts = 0;
  for (int i = 0; i < 10; ++i) {
    now += 200;  // past each 100 ms window
    BroadcastEntry* due = btt.due(now, 5 /*never enough acks*/, 100, 3);
    if (!due) break;
    btt.markRebroadcast(due, now);
    ++rebroadcasts;
  }
  // Started at retries=1, cap=3 -> at most 2 more rebroadcasts.
  check(rebroadcasts == 2, "stops at maxRetries (2 further rebroadcasts)");
  check(e->retries == 3, "retry counter reaches the cap");
}

void testExpiryAndRecycle() {
  Serial.println("Expiry + full-table recycle:");
  BroadcastEntry storage[2];
  ZigbeeBroadcastTable btt(storage, 2);

  btt.recordIncoming(0x00A1, 1, 1000);
  btt.recordIncoming(0x00A2, 2, 2000);
  check(btt.activeCount() == 2, "table full (2/2)");

  // Expire entries older than 9 s as of t=10500: only the first (age 9500)
  // goes; the second (age 8500) stays.
  uint8_t freed = btt.expire(10500, 9000);
  check(freed == 1 && btt.activeCount() == 1, "expired the stale transaction");
  check(btt.find(0x00A1, 1) == nullptr, "stale entry gone");
  check(btt.find(0x00A2, 2) != nullptr, "fresh entry kept");

  // Fill it back up, then a third NEW transaction recycles the oldest slot.
  btt.recordIncoming(0x00A3, 3, 11500);  // 2/2 again (A2,A3)
  check(btt.activeCount() == 2, "refilled");
  btt.recordIncoming(0x00A4, 4, 12000);  // recycles oldest (A2, firstSeen=2000)
  check(btt.activeCount() == 2, "still 2 after recycle");
  check(btt.find(0x00A2, 2) == nullptr, "oldest recycled");
  check(btt.find(0x00A4, 4) != nullptr, "newest present");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee Broadcast Transaction Table self-test ===");

  testDedup();
  testPassiveAckSuppressesRetry();
  testRetryCap();
  testExpiryAndRecycle();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
