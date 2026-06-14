/*
  CC2530_GroupCast - Zigbee group addressing (multicast) self-test.

  Group addressing lets one APS frame reach a whole set of endpoints - the
  classic "turn this group of lights on". A group-addressed APS data frame
  (delivery mode = group) carries a 16-bit group address instead of a
  destination endpoint and is sent as a NWK broadcast; each receiver acts on it
  only if it is a member of that group. This sketch self-tests the group APS
  frame (ZigbeeAps::buildGroupDataFrame / parseDataFrame) and the membership
  store (ZigbeeGroupTable), including the receiver's accept/ignore decision.

  No radio traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testGroupFrame() {
  Serial.println("Group-addressed APS data frame:");
  const uint8_t payload[3] = {0x01, 0x21, 0x00};  // e.g. a ZCL On command
  uint8_t apdu[24];
  uint8_t n = ZigbeeAps::buildGroupDataFrame(apdu, sizeof(apdu), 0xBEEF,
                                             ZigbeeZcl::kClusterOnOff,
                                             ZigbeeAps::kProfileHomeAutomation,
                                             1, 7, payload, 3);
  check(n == 9 + 3, "group frame = 9-byte header + payload");

  ApsDataFrame f;
  check(ZigbeeAps::parseDataFrame(apdu, n, f), "parse group frame");
  check(f.deliveryMode == APS_DELIVERY_GROUP, "delivery mode = group");
  check(f.groupAddress == 0xBEEF, "group address round-trip");
  check(f.clusterId == ZigbeeZcl::kClusterOnOff && f.srcEndpoint == 1 &&
            f.counter == 7,
        "cluster / src endpoint / counter past the group address");
  check(f.payloadLen == 3 && f.payload[0] == 0x01 && f.payload[1] == 0x21,
        "payload located after the 9-byte group header");

  // Regression: a unicast frame still parses with the 8-byte header.
  uint8_t uni[16];
  uint8_t un = ZigbeeAps::buildDataFrame(uni, sizeof(uni), 5,
                                         ZigbeeZcl::kClusterOnOff,
                                         ZigbeeAps::kProfileHomeAutomation, 1, 8,
                                         payload, 3);
  ApsDataFrame uf;
  check(ZigbeeAps::parseDataFrame(uni, un, uf) &&
            uf.deliveryMode == APS_DELIVERY_UNICAST && uf.dstEndpoint == 5 &&
            uf.payloadLen == 3,
        "unicast frame unaffected (8-byte header, dst endpoint)");
}

void testGroupTable() {
  Serial.println("Group membership table:");
  uint16_t storage[3];
  ZigbeeGroupTable groups(storage, 3);

  check(!groups.isMember(0x0001), "not a member before joining");
  check(groups.add(0x0001), "join group 0x0001");
  check(groups.add(0x0002), "join group 0x0002");
  check(groups.isMember(0x0001) && groups.isMember(0x0002), "both memberships");
  check(groups.count() == 2, "count = 2");

  check(groups.add(0x0001), "re-join is idempotent");
  check(groups.count() == 2, "count unchanged after re-join");

  check(groups.add(0x0003), "join third (fills table)");
  check(!groups.add(0x0004), "join rejected when full");
  check(!groups.isMember(0x0004), "rejected group not a member");

  check(groups.remove(0x0002), "leave group 0x0002");
  check(!groups.isMember(0x0002) && groups.count() == 2, "membership shrank");
  check(groups.add(0x0004), "room again after leaving");
  check(!groups.remove(0x00FF), "leaving a non-member returns false");
}

void testReceiverDecision() {
  Serial.println("Receiver accept/ignore by membership:");
  uint16_t storage[2];
  ZigbeeGroupTable groups(storage, 2);
  groups.add(0xBEEF);

  // Two group frames arrive; the receiver acts only on the group it joined.
  uint8_t apdu[16];
  uint8_t n = ZigbeeAps::buildGroupDataFrame(apdu, sizeof(apdu), 0xBEEF,
                                             ZigbeeZcl::kClusterOnOff,
                                             ZigbeeAps::kProfileHomeAutomation,
                                             1, 1, nullptr, 0);
  ApsDataFrame f;
  ZigbeeAps::parseDataFrame(apdu, n, f);
  check(groups.isMember(f.groupAddress), "frame to joined group 0xBEEF accepted");

  n = ZigbeeAps::buildGroupDataFrame(apdu, sizeof(apdu), 0x1234,
                                     ZigbeeZcl::kClusterOnOff,
                                     ZigbeeAps::kProfileHomeAutomation, 1, 2,
                                     nullptr, 0);
  ZigbeeAps::parseDataFrame(apdu, n, f);
  check(!groups.isMember(f.groupAddress), "frame to other group 0x1234 ignored");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee group addressing self-test ===");

  testGroupFrame();
  testGroupTable();
  testReceiverDecision();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
