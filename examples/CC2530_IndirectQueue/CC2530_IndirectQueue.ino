/*
  CC2530_IndirectQueue - indirect transmission (sleepy end device) self-test.

  A sleepy child keeps its radio off and polls its parent with a MAC Data
  Request when it wakes; the parent buffers frames meant for the child and
  delivers them on the poll. This sketch self-tests the two building blocks:
  ZigbeeMac::buildDataRequest (the child's poll) and ZigbeeIndirectQueue (the
  parent's pending-frame store). No radio traffic; runs on board1 via J-Link.

  Setting the frame-pending bit in the ack on air needs CC2530 firmware
  support; the host-side queue + poll logic verified here is the rest of it.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testDataRequestFrame() {
  Serial.println("MAC Data Request frame:");
  uint8_t f[16];
  uint8_t n = ZigbeeMac::buildDataRequest(f, sizeof(f), 0x1A62, 0x0000, 0x0031, 7);
  check(n == 10, "data request is 10 bytes");
  check(f[9] == MAC_CMD_DATA_REQUEST, "command id = Data Request (0x04)");
  check(f[2] == 7, "sequence number placed");

  // Parse it back through the generic MAC command parser.
  MacCommandFrame cmd;
  bool ok = ZigbeeMac::parseCommandFrame(f, n, cmd);
  check(ok, "parses as a MAC command frame");
  check(ok && cmd.commandId == MAC_CMD_DATA_REQUEST, "parsed command id matches");
  check(ok && cmd.ackRequest, "ack requested (parent ack carries pending bit)");
  check(ok && cmd.srcShort == 0x0031, "src = child short address");
  check(ok && cmd.dstShort == 0x0000, "dst = parent short address");
}

void testQueueEnqueueAndPoll() {
  Serial.println("Indirect queue enqueue + poll:");
  IndirectEntry storage[3];
  ZigbeeIndirectQueue q(storage, 3);

  const uint8_t msg[] = {0xDE, 0xAD, 0xBE, 0xEF};
  check(!q.hasPending(0x0031), "nothing pending before enqueue");
  check(q.enqueue(0x0031, msg, sizeof(msg), 1000), "enqueue for child 0x0031");
  check(q.hasPending(0x0031), "pending after enqueue (ack would set pending bit)");
  check(!q.hasPending(0x0099), "other child still has nothing");

  IndirectEntry* e = q.pending(0x0031);
  check(e != nullptr && e->length == 4, "pending() returns the buffered frame");
  check(e && memcmp(e->payload, msg, 4) == 0, "buffered bytes intact");

  // pending() must NOT remove it (a lost frame can be re-polled).
  check(q.hasPending(0x0031), "still pending after pending() peek");
  q.dequeue(e);
  check(!q.hasPending(0x0031), "gone after dequeue (delivery confirmed)");
}

void testReplaceAndCapacity() {
  Serial.println("Replace-latest + capacity:");
  IndirectEntry storage[2];
  ZigbeeIndirectQueue q(storage, 2);

  const uint8_t a[] = {1, 2};
  const uint8_t b[] = {3, 4, 5};
  q.enqueue(0x0031, a, 2, 1000);
  q.enqueue(0x0031, b, 3, 1100);  // same child -> replaces, keeps latest
  check(q.activeCount() == 1, "same child replaced, not duplicated");
  IndirectEntry* e = q.pending(0x0031);
  check(e && e->length == 3 && e->payload[2] == 5, "latest frame kept");

  q.enqueue(0x0040, a, 2, 1200);
  check(q.activeCount() == 2, "second distinct child queued (full)");
  // Full table: a third distinct child reuses the oldest slot (0x0031 expires
  // first since it was enqueued earliest of the two remaining... 0x0031 @1100).
  q.enqueue(0x0050, a, 2, 1300);
  check(q.activeCount() == 2, "still 2 (reused a slot)");
  check(q.hasPending(0x0050), "newest child present");
}

void testExpiry() {
  Serial.println("Transaction persistence (expiry):");
  IndirectEntry storage[2];
  ZigbeeIndirectQueue q(storage, 2);

  const uint8_t m[] = {9};
  q.enqueue(0x0031, m, 1, 1000, /*persistenceMs=*/7680);  // expires at 8680
  q.enqueue(0x0032, m, 1, 5000, /*persistenceMs=*/7680);  // expires at 12680

  check(q.expire(8000) == 0, "nothing expired before deadline");
  check(q.expire(9000) == 1, "first transaction expired past 8680");
  check(!q.hasPending(0x0031), "expired child cleared");
  check(q.hasPending(0x0032), "later child still pending");
  check(q.expire(13000) == 1, "second expired past 12680");
  check(q.activeCount() == 0, "queue empty after both expire");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee indirect-transmission self-test ===");

  testDataRequestFrame();
  testQueueEnqueueAndPoll();
  testReplaceAndCapacity();
  testExpiry();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
