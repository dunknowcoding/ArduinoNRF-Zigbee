/*
  CC2530_SourceRouting - Zigbee PRO many-to-one / source routing self-test.

  A concentrator broadcasts a many-to-one route request; routers install a cheap
  route toward it and send Route Records that accumulate their relay path. The
  concentrator stores those paths and source-routes replies back down the same
  hops. This sketch self-tests the many-to-one route request and Route Record
  frames (which already exist) plus the new concentrator-side
  ZigbeeSourceRouteTable that stores and reverses the paths.

  No radio traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testManyToOneRequest() {
  Serial.println("Many-to-one route request:");
  uint8_t buf[16];
  // Concentrator MTORR: dest = all-routers broadcast, many-to-one set.
  uint8_t n = ZigbeeNwk::buildRouteRequestPayload(buf, sizeof(buf), 9,
                                                  ZigbeeNwk::kBroadcastAllRouters,
                                                  0, /*manyToOne=*/true);
  check(n > 0, "build many-to-one route request");
  NwkRouteRequestCommand req;
  check(ZigbeeNwk::parseRouteRequestPayload(buf, n, req), "parse it");
  check(req.manyToOne, "many-to-one flag set");
  check(req.routeRequestId == 9, "route request id round-trip");

  // A normal (one-to-one) request must NOT carry the many-to-one flag.
  n = ZigbeeNwk::buildRouteRequestPayload(buf, sizeof(buf), 10, 0x1234, 0,
                                          /*manyToOne=*/false);
  ZigbeeNwk::parseRouteRequestPayload(buf, n, req);
  check(!req.manyToOne && req.destination == 0x1234,
        "one-to-one request: flag clear, dest kept");
}

void testRouteRecord() {
  Serial.println("Route Record frame:");
  const uint16_t relays[3] = {0x0002, 0x0005, 0x0009};
  uint8_t buf[16];
  uint8_t n = ZigbeeNwk::buildRouteRecordPayload(buf, sizeof(buf), relays, 3);
  check(n > 0, "build Route Record with 3 relays");

  NwkRouteRecordCommand rec;
  check(ZigbeeNwk::parseRouteRecordPayload(buf, n, rec), "parse Route Record");
  check(rec.relayCount == 3, "relay count round-trip");
  uint16_t r0 = 0, r1 = 0, r2 = 0;
  ZigbeeNwk::getRouteRecordRelay(rec, 0, r0);
  ZigbeeNwk::getRouteRecordRelay(rec, 1, r1);
  ZigbeeNwk::getRouteRecordRelay(rec, 2, r2);
  check(r0 == 0x0002 && r1 == 0x0005 && r2 == 0x0009, "relays round-trip in order");
}

// Pull the relay list out of a parsed Route Record into a uint16_t array (the
// concentrator does this before storing the path).
uint8_t extractRelays(const NwkRouteRecordCommand& rec, uint16_t* out, uint8_t max) {
  uint8_t n = rec.relayCount < max ? rec.relayCount : max;
  for (uint8_t i = 0; i < n; ++i) ZigbeeNwk::getRouteRecordRelay(rec, i, out[i]);
  return n;
}

void testSourceRouteTable() {
  Serial.println("Source route table (concentrator):");
  SourceRouteEntry storage[3];
  ZigbeeSourceRouteTable srt(storage, 3);

  // Device 0x0031 reaches the concentrator via 0x0009 then 0x0002 (travel
  // order device->concentrator), carried in a Route Record.
  const uint16_t path[2] = {0x0009, 0x0002};
  uint8_t buf[16];
  uint8_t n = ZigbeeNwk::buildRouteRecordPayload(buf, sizeof(buf), path, 2);
  NwkRouteRecordCommand rec;
  ZigbeeNwk::parseRouteRecordPayload(buf, n, rec);
  uint16_t relays[8];
  uint8_t rc = extractRelays(rec, relays, 8);

  check(srt.install(0x0031, relays, rc, 1000), "install path to 0x0031");
  check(srt.has(0x0031), "path present");
  check(!srt.has(0x0099), "unknown device has no path");

  // Downstream source route = reversed: concentrator -> 0x0002 -> 0x0009 -> dev.
  uint16_t ds[8];
  uint8_t dn = srt.downstreamRoute(0x0031, ds, 8);
  check(dn == 2, "downstream route has 2 relays");
  check(ds[0] == 0x0002 && ds[1] == 0x0009, "downstream relays reversed");
  check(srt.downstreamRoute(0x0099, ds, 8) == 255, "no route -> 255");

  // A direct neighbor sends a 0-relay Route Record (valid, distinct from none).
  check(srt.install(0x0040, nullptr, 0, 1100), "install direct (0 relays)");
  check(srt.downstreamRoute(0x0040, ds, 8) == 0, "direct route -> 0 relays (not 255)");

  // Re-install replaces + refreshes; table reuses oldest when full.
  const uint16_t path2[1] = {0x0007};
  srt.install(0x0031, path2, 1, 2000);
  check(srt.lookup(0x0031)->relayCount == 1, "re-install replaced the path");
  check(srt.activeCount() == 2, "two distinct devices stored");

  srt.install(0x0050, path2, 1, 2100);  // fills to 3
  check(srt.activeCount() == 3, "table full (3/3)");
  srt.install(0x0060, path2, 1, 2200);  // reuses oldest (0x0040 @1100)
  check(srt.activeCount() == 3 && !srt.has(0x0040) && srt.has(0x0060),
        "oldest path recycled when full");

  // Expiry.
  uint8_t freed = srt.expire(2200 + 60000, 60000);  // 0x0031@2000 ages out
  check(freed >= 1 && !srt.has(0x0031), "stale path expired");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee many-to-one / source routing self-test ===");

  testManyToOneRequest();
  testRouteRecord();
  testSourceRouteTable();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
