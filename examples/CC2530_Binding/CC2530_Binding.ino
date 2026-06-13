/*
  CC2530_Binding - Zigbee APS source binding: tables, ZDO Bind_req frames,
  and indirect (bind-driven) addressing.

  A binding ties a local (source endpoint, cluster) to a remote destination,
  so the app sends to "whatever is bound" instead of an explicit address -
  the basis of Zigbee's bind-then-control model (a switch bound to one or more
  lamps). This sketch is self-contained: it exercises the binding table and
  the Bind_req frame tooling on one board (printing PASS/FAIL), then shows how
  a sender walks the table to deliver to every bound destination.

  In a real network the Bind_req would be unicast to the source device (or the
  coordinator's binding manager) over the air; here we build/parse it in place
  to validate the encoding, and keep the binding table local to the node.
*/

#include <CC2530Radio.h>

static const uint64_t SELF_IEEE = 0x1A62195E00000010ULL;
static const uint8_t SWITCH_ENDPOINT = 1;
static const uint16_t CLUSTER_ONOFF = 0x0006;

// Two lamps the switch will be bound to, plus a group binding.
static const uint64_t LAMP_A_IEEE = 0x1A62195E000000A1ULL;
static const uint64_t LAMP_B_IEEE = 0x1A62195E000000B2ULL;
static const uint16_t LAMP_GROUP = 0x0007;

ZigbeeBinding bindingStorage[8];
ZigbeeBindingTable bindings(bindingStorage, 8);
uint8_t zdoSeq = 0;
uint32_t passes = 0, fails = 0;

void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

ZigbeeBinding makeIeeeBinding(uint64_t dstIeee, uint8_t dstEp) {
  ZigbeeBinding b = ZigbeeBinding();
  b.srcIeee = SELF_IEEE;
  b.srcEndpoint = SWITCH_ENDPOINT;
  b.clusterId = CLUSTER_ONOFF;
  b.dstAddrMode = ZB_BIND_ADDR_IEEE;
  b.dstIeee = dstIeee;
  b.dstEndpoint = dstEp;
  return b;
}

void testBindingTable() {
  Serial.println("Binding table:");
  bindings.begin(bindingStorage, 8);

  check(bindings.add(makeIeeeBinding(LAMP_A_IEEE, 1)), "add lamp A");
  check(bindings.add(makeIeeeBinding(LAMP_B_IEEE, 1)), "add lamp B");

  ZigbeeBinding grp = ZigbeeBinding();
  grp.srcIeee = SELF_IEEE;
  grp.srcEndpoint = SWITCH_ENDPOINT;
  grp.clusterId = CLUSTER_ONOFF;
  grp.dstAddrMode = ZB_BIND_ADDR_GROUP;
  grp.dstGroup = LAMP_GROUP;
  check(bindings.add(grp), "add group binding");

  check(bindings.count() == 3, "count == 3");
  // Adding lamp A again must be idempotent (no duplicate).
  check(bindings.add(makeIeeeBinding(LAMP_A_IEEE, 1)) && bindings.count() == 3,
        "duplicate add is idempotent");

  // Iterate the bindings for (endpoint, cluster) - this is what a sender does.
  uint8_t cursor = 0, seen = 0;
  const ZigbeeBinding* b;
  while ((b = bindings.next(SWITCH_ENDPOINT, CLUSTER_ONOFF, cursor)) != nullptr) {
    ++seen;
  }
  check(seen == 3, "iterate finds all 3 bound destinations");

  check(bindings.remove(makeIeeeBinding(LAMP_B_IEEE, 1)) &&
            bindings.count() == 2,
        "remove lamp B");
}

void testBindRequestFrame() {
  Serial.println("Bind_req frame:");

  ZdoBindRequest req = ZdoBindRequest();
  req.sequence = 0x42;
  req.srcAddress = SELF_IEEE;
  req.srcEndpoint = SWITCH_ENDPOINT;
  req.clusterId = CLUSTER_ONOFF;
  req.dstAddrMode = ZigbeeZdo::kBindAddrModeIeee;
  req.dstAddress = LAMP_A_IEEE;
  req.dstEndpoint = 1;

  uint8_t buf[32];
  uint8_t n = ZigbeeZdo::buildBindRequest(buf, sizeof(buf), req);
  check(n == 22, "IEEE-mode Bind_req is 22 bytes");

  ZdoBindRequest parsed;
  bool ok = ZigbeeZdo::parseBindRequest(buf, n, parsed);
  check(ok, "parse Bind_req");
  check(parsed.sequence == req.sequence && parsed.srcAddress == req.srcAddress &&
            parsed.clusterId == req.clusterId &&
            parsed.dstAddress == req.dstAddress &&
            parsed.dstEndpoint == req.dstEndpoint,
        "round-trip fields match");

  // Group-mode variant.
  req.dstAddrMode = ZigbeeZdo::kBindAddrModeGroup;
  req.dstGroup = LAMP_GROUP;
  n = ZigbeeZdo::buildBindRequest(buf, sizeof(buf), req);
  check(n == 15, "group-mode Bind_req is 15 bytes");
  ok = ZigbeeZdo::parseBindRequest(buf, n, parsed);
  check(ok && parsed.dstAddrMode == ZigbeeZdo::kBindAddrModeGroup &&
            parsed.dstGroup == LAMP_GROUP,
        "group round-trip");

  uint8_t rsp[4];
  uint8_t rn = ZigbeeZdo::buildBindResponse(rsp, sizeof(rsp), 0x42, 0x00);
  ZdoBindResponse bresp;
  check(rn == 2 && ZigbeeZdo::parseBindResponse(rsp, rn, bresp) &&
            bresp.sequence == 0x42 && bresp.status == 0x00,
        "Bind_rsp round-trip");
}

void showIndirectSend() {
  Serial.println("Indirect send (what the app does on a switch press):");
  uint8_t cursor = 0;
  const ZigbeeBinding* b;
  while ((b = bindings.next(SWITCH_ENDPOINT, CLUSTER_ONOFF, cursor)) != nullptr) {
    Serial.print("  -> OnOff to ");
    if (b->dstAddrMode == ZB_BIND_ADDR_GROUP) {
      Serial.print("group 0x");
      Serial.println(b->dstGroup, HEX);
    } else {
      Serial.print("ieee 0x");
      for (int i = 7; i >= 0; --i) {
        uint8_t by = (uint8_t)(b->dstIeee >> (i * 8));
        if (by < 0x10) Serial.print('0');
        Serial.print(by, HEX);
      }
      Serial.print(" ep "); Serial.println(b->dstEndpoint);
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee binding self-test ===");

  testBindingTable();
  testBindRequestFrame();
  showIndirectSend();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
