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

void testSourceRoutedFrame() {
  Serial.println("Source-routed NWK data frame:");
  const uint16_t relays[2] = {0x0002, 0x0009};  // concentrator -> ... -> dest
  const char* msg = "hi";
  uint8_t npdu[32];
  // Originator sets relay index = relay count (all relays still ahead).
  uint8_t n = ZigbeeNwk::buildDataFrameSourceRouted(
      npdu, sizeof(npdu), 0x0031, 0x0000, 30, 7, relays, 2, 2,
      (const uint8_t*)msg, 2);
  check(n == 8 + (2 + 2 * 2) + 2, "frame = base(8) + subframe(6) + payload(2)");

  NwkDataFrame f;
  check(ZigbeeNwk::parseDataFrame(npdu, n, f), "parse source-routed frame");
  check(f.sourceRoute, "source-route flag set");
  check(f.srRelayCount == 2 && f.srRelayIndex == 2, "relay count + index");
  uint16_t r0 = 0, r1 = 0;
  ZigbeeNwk::getDataFrameRelay(f, 0, r0);
  ZigbeeNwk::getDataFrameRelay(f, 1, r1);
  check(r0 == 0x0002 && r1 == 0x0009, "relays parsed in order");
  check(f.dstShort == 0x0031 && f.srcShort == 0x0000, "addresses past subframe");
  check(f.payloadLen == 2 && f.payload[0] == 'h' && f.payload[1] == 'i',
        "payload correctly located after the subframe");

  // Regression: a plain data frame still parses with no source route.
  uint8_t plain[16];
  uint8_t pn = ZigbeeNwk::buildDataFrame(plain, sizeof(plain), 0x0031, 0x0000,
                                         30, 8, (const uint8_t*)msg, 2);
  NwkDataFrame pf;
  check(ZigbeeNwk::parseDataFrame(plain, pn, pf) && !pf.sourceRoute &&
            pf.payloadLen == 2,
        "plain data frame unaffected (no source route)");
}

// Build a source-routed frame at a given relay index and return the parsed form
// (so each hop can run sourceRouteAction on it).
static bool srFrame(NwkDataFrame& f, const uint16_t* relays, uint8_t relayCount,
                    uint8_t relayIndex, uint16_t dst) {
  static uint8_t npdu[40];
  uint8_t n = ZigbeeNwk::buildDataFrameSourceRouted(npdu, sizeof(npdu), dst,
                                                    0x0000, 30, 1, relays,
                                                    relayCount, relayIndex,
                                                    (const uint8_t*)"x", 1);
  return n > 0 && ZigbeeNwk::parseDataFrame(npdu, n, f);
}

void testForwarding() {
  Serial.println("Source-route forwarding (per-hop decision):");
  const uint16_t relays[2] = {0x0002, 0x0009};  // A -> 0x0002 -> 0x0009 -> dst
  const uint16_t dst = 0x0031;
  uint16_t nextHop = 0;
  uint8_t outIdx = 0;
  NwkDataFrame f;

  // Hop 1: the frame leaves A at index 0, arrives at relay 0x0002.
  srFrame(f, relays, 2, 0, dst);
  uint8_t a = ZigbeeNwk::sourceRouteAction(f, 0x0002, nextHop, outIdx);
  check(a == NWK_SR_RELAY && nextHop == 0x0009 && outIdx == 1,
        "relay 0x0002 -> next relay 0x0009, index 1");

  // Hop 2: index 1 arrives at relay 0x0009; next hop is the destination.
  srFrame(f, relays, 2, 1, dst);
  a = ZigbeeNwk::sourceRouteAction(f, 0x0009, nextHop, outIdx);
  check(a == NWK_SR_RELAY && nextHop == dst && outIdx == 2,
        "last relay 0x0009 -> destination 0x0031, index 2");

  // At the destination: deliver to the app.
  srFrame(f, relays, 2, 2, dst);
  a = ZigbeeNwk::sourceRouteAction(f, dst, nextHop, outIdx);
  check(a == NWK_SR_DELIVER, "destination delivers locally");

  // A node not on the path drops it.
  srFrame(f, relays, 2, 0, dst);
  a = ZigbeeNwk::sourceRouteAction(f, 0x00AB, nextHop, outIdx);
  check(a == NWK_SR_DROP, "off-path node drops the frame");
}

void testOrigination() {
  Serial.println("Concentrator origination (Route Record -> originate -> relay):");
  SourceRouteEntry storage[2];
  ZigbeeSourceRouteTable srt(storage, 2);

  // Device 0x0031's Route Record arrives at the concentrator with the relay
  // path in travel order device -> concentrator: 0x0005 then 0x0002.
  const uint16_t travel[2] = {0x0005, 0x0002};
  uint8_t rr[16];
  uint8_t rrn = ZigbeeNwk::buildRouteRecordPayload(rr, sizeof(rr), travel, 2);
  NwkRouteRecordCommand rec;
  ZigbeeNwk::parseRouteRecordPayload(rr, rrn, rec);
  uint16_t relays[8];
  uint8_t rc = 0;
  for (uint8_t i = 0; i < rec.relayCount && i < 8; ++i)
    ZigbeeNwk::getRouteRecordRelay(rec, i, relays[i]), ++rc;
  check(srt.install(0x0031, relays, rc, 1000), "store the device's Route Record");

  // The concentrator builds a downstream source route (reversed): 0x0002 ->
  // 0x0005 -> device.
  uint16_t ds[8];
  uint8_t dn = srt.downstreamRoute(0x0031, ds, 8);
  check(dn == 2 && ds[0] == 0x0002 && ds[1] == 0x0005, "downstream route reversed");

  // ...and originates a source-routed data frame from it (relay index 0).
  uint8_t npdu[40];
  uint8_t n = ZigbeeNwk::buildDataFrameSourceRouted(npdu, sizeof(npdu), 0x0031,
                                                    0x0000, 30, 5, ds, dn, 0,
                                                    (const uint8_t*)"go", 2);
  NwkDataFrame f;
  check(n > 0 && ZigbeeNwk::parseDataFrame(npdu, n, f) && f.sourceRoute &&
            f.dstShort == 0x0031 && f.srRelayCount == 2 && f.srRelayIndex == 0,
        "originated source-routed frame to the device");

  // The first relay (0x0002) forwards it on toward 0x0005.
  uint16_t nextHop = 0; uint8_t outIdx = 0;
  uint8_t act = ZigbeeNwk::sourceRouteAction(f, 0x0002, nextHop, outIdx);
  check(act == NWK_SR_RELAY && nextHop == 0x0005,
        "first relay forwards toward the next relay");
}

void testSecuredSourceRoute() {
  Serial.println("Secured source-routed frame (mutable relay index):");
  const uint16_t relays[2] = {0x0002, 0x0009};
  uint8_t npdu[48];
  uint8_t n = ZigbeeNwk::buildDataFrameSourceRouted(npdu, sizeof(npdu), 0x0031,
                                                    0x0000, 30, 1, relays, 2, 0,
                                                    (const uint8_t*)"secure", 6);
  uint8_t headerLen = (uint8_t)(8 + 2 + 2 * 2);  // base + subframe(count+index+2 relays)

  ZigbeeSecurity sec;
  uint8_t key[16]; for (uint8_t i = 0; i < 16; ++i) key[i] = (uint8_t)(0xC0 + i);
  sec.setNetworkKey(key, 0);

  uint8_t secured[80];
  uint8_t sn = sec.secureNpdu(npdu, n, headerLen, 0x1A62195E00000000ULL, 1,
                              secured, sizeof(secured));
  check(sn > 0, "secure a source-routed NPDU");

  // Baseline: it opens.
  uint8_t out[64];
  sec.resetReplayTable();
  check(sec.openNpdu(secured, sn, headerLen, out, sizeof(out)) > 0,
        "opens before any relay touches it");

  // A relay rewrites the mutable relay index (byte 9). The frame must still
  // decrypt - the index is excluded from the CCM* AAD.
  uint8_t relayed[80]; memcpy(relayed, secured, sn);
  relayed[9] = 1;  // advance the relay index
  sec.resetReplayTable();
  uint8_t rn = sec.openNpdu(relayed, sn, headerLen, out, sizeof(out));
  check(rn > 0, "still decrypts after the relay index is changed");
  check(rn > 0 && out[9] == 1, "the new relay index is delivered");

  // But the rest of the header is still authenticated: change the destination.
  uint8_t tampered[80]; memcpy(tampered, secured, sn);
  tampered[2] ^= 0x01;  // destination short address (in the base header)
  sec.resetReplayTable();
  check(sec.openNpdu(tampered, sn, headerLen, out, sizeof(out)) == 0,
        "a changed destination still fails the MIC");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee many-to-one / source routing self-test ===");

  testManyToOneRequest();
  testRouteRecord();
  testSourceRouteTable();
  testSourceRoutedFrame();
  testForwarding();
  testOrigination();
  testSecuredSourceRoute();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
