/*
  CC2530_EndDeviceTimeout - NWK End Device Timeout self-test.

  A sleepy end device negotiates how long it may stay silent before its parent
  forgets it: it sends an End Device Timeout Request with a timeout from a fixed
  enumeration, and the parent answers with a Response granting it and listing
  the keep-alive methods it supports. This sketch self-tests the request/response
  payloads and the timeout-enumeration -> seconds mapping. Pairs with the
  indirect-transmission queue to complete the sleepy-end-device lifecycle.

  No radio traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testTimeoutTable() {
  Serial.println("Timeout enumeration -> seconds:");
  check(ZigbeeEndDeviceTimeout::timeoutSeconds(0) == 10, "index 0 = 10 s");
  check(ZigbeeEndDeviceTimeout::timeoutSeconds(1) == 120, "index 1 = 2 min");
  check(ZigbeeEndDeviceTimeout::timeoutSeconds(2) == 240, "index 2 = 4 min");
  check(ZigbeeEndDeviceTimeout::timeoutSeconds(3) == 480, "index 3 = 8 min");
  check(ZigbeeEndDeviceTimeout::timeoutSeconds(14) == 60UL * 16384UL,
        "index 14 = 16384 min");
  check(ZigbeeEndDeviceTimeout::timeoutSeconds(15) == 0, "out-of-range -> 0");
}

void testRequest() {
  Serial.println("End Device Timeout Request:");
  NwkEdTimeoutRequest req;
  req.timeoutIndex = 5;   // 32 min
  req.configuration = 0;
  uint8_t buf[4];
  uint8_t n = ZigbeeEndDeviceTimeout::buildRequest(buf, sizeof(buf), req);
  check(n == 2, "request payload is 2 bytes");
  check(buf[0] == 5, "timeout index encoded");

  NwkEdTimeoutRequest parsed;
  check(ZigbeeEndDeviceTimeout::parseRequest(buf, n, parsed), "parse request");
  check(parsed.timeoutIndex == 5 && parsed.configuration == 0,
        "round-trip index + configuration");
  check(ZigbeeEndDeviceTimeout::timeoutSeconds(parsed.timeoutIndex) == 1920,
        "parsed index resolves to 32 min (1920 s)");

  // An out-of-range index must be refused on build and parse.
  req.timeoutIndex = 20;
  check(ZigbeeEndDeviceTimeout::buildRequest(buf, sizeof(buf), req) == 0,
        "build rejects out-of-range index");
  uint8_t bad[2] = {20, 0};
  check(!ZigbeeEndDeviceTimeout::parseRequest(bad, 2, parsed),
        "parse rejects out-of-range index");
}

void testResponse() {
  Serial.println("End Device Timeout Response:");
  NwkEdTimeoutResponse rsp;
  rsp.status = ED_TIMEOUT_SUCCESS;
  rsp.parentInfo = ED_PARENT_MAC_DATA_POLL | ED_PARENT_ED_TIMEOUT;
  uint8_t buf[4];
  uint8_t n = ZigbeeEndDeviceTimeout::buildResponse(buf, sizeof(buf), rsp);
  check(n == 2, "response payload is 2 bytes");

  NwkEdTimeoutResponse parsed;
  check(ZigbeeEndDeviceTimeout::parseResponse(buf, n, parsed), "parse response");
  check(parsed.status == ED_TIMEOUT_SUCCESS, "status round-trip");
  check((parsed.parentInfo & ED_PARENT_MAC_DATA_POLL) &&
            (parsed.parentInfo & ED_PARENT_ED_TIMEOUT),
        "parent advertises MAC poll + ED timeout keep-alive");

  // A parent that cannot honour the value answers INCORRECT_VALUE.
  rsp.status = ED_TIMEOUT_INCORRECT_VALUE;
  rsp.parentInfo = ED_PARENT_MAC_DATA_POLL;
  ZigbeeEndDeviceTimeout::buildResponse(buf, sizeof(buf), rsp);
  ZigbeeEndDeviceTimeout::parseResponse(buf, 2, parsed);
  check(parsed.status == ED_TIMEOUT_INCORRECT_VALUE &&
            !(parsed.parentInfo & ED_PARENT_ED_TIMEOUT),
        "incorrect-value + MAC-poll-only round-trip");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee End Device Timeout self-test ===");

  testTimeoutTable();
  testRequest();
  testResponse();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
