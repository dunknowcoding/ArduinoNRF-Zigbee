/*
  CC2530_ZdoMgmt - ZDO network-management command self-test.

  Self-tests the ZDP commands a gateway uses to run a network beyond discovery
  and Mgmt_Lqi/Rtg: Mgmt_Permit_Joining (open/close joining), Mgmt_Leave (evict a
  device), and Node_Desc (query a device's capabilities). No radio traffic; runs
  on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testPermitJoining() {
  Serial.println("Mgmt_Permit_Joining:");
  uint8_t buf[8];
  uint8_t n = ZigbeeZdoMgmt::buildPermitJoiningRequest(buf, sizeof(buf), 7, 60, true);
  check(n == 3 && buf[0] == 7 && buf[1] == 60 && buf[2] == 1,
        "request = seq + duration + TC significance");

  n = ZigbeeZdoMgmt::buildStatusResponse(buf, sizeof(buf), 7, ZDO_STATUS_SUCCESS);
  uint8_t seq = 0, status = 0xFF;
  check(n == 2 && ZigbeeZdoMgmt::parseStatusResponse(buf, n, seq, status) &&
            seq == 7 && status == ZDO_STATUS_SUCCESS,
        "response = seq + status round-trip");
}

void testLeave() {
  Serial.println("Mgmt_Leave:");
  const uint64_t target = 0x1A62195E00000031ULL;
  uint8_t buf[12];
  uint8_t n = ZigbeeZdoMgmt::buildLeaveRequest(buf, sizeof(buf), 9, target,
                                               /*removeChildren=*/true,
                                               /*rejoin=*/false);
  check(n == 10, "leave request is 10 bytes");

  uint8_t seq = 0; uint64_t ieee = 0; bool rc = false, rj = true;
  check(ZigbeeZdoMgmt::parseLeaveRequest(buf, n, seq, ieee, rc, rj), "parse leave");
  check(seq == 9 && ieee == target && rc && !rj,
        "round-trip: seq, target IEEE, remove-children set, rejoin clear");
}

void testNodeDesc() {
  Serial.println("Node_Desc:");
  uint8_t buf[20];
  uint8_t n = ZigbeeZdoMgmt::buildNodeDescRequest(buf, sizeof(buf), 3, 0x0031);
  uint8_t seq = 0; uint16_t addr = 0;
  check(n == 3 && ZigbeeZdoMgmt::parseNodeDescRequest(buf, n, seq, addr) &&
            seq == 3 && addr == 0x0031,
        "request = seq + NWK address of interest");

  ZdoNodeDescriptor d;
  d.logicalType = ZDO_TYPE_ROUTER;
  d.macCapabilityFlags = 0x8E;
  d.manufacturerCode = 0x1234;
  d.maxBufferSize = 82;
  d.maxIncomingTransferSize = 0x0080;
  d.serverMask = 0x2040;
  d.maxOutgoingTransferSize = 0x0080;
  n = ZigbeeZdoMgmt::buildNodeDescResponse(buf, sizeof(buf), 3,
                                           ZDO_STATUS_SUCCESS, 0x0031, d);
  check(n == 17, "response = seq + status + addr + 13-byte descriptor");

  uint8_t status = 0xFF;
  ZdoNodeDescriptor pd;
  check(ZigbeeZdoMgmt::parseNodeDescResponse(buf, n, seq, status, addr, pd),
        "parse node descriptor response");
  check(status == ZDO_STATUS_SUCCESS && addr == 0x0031 &&
            pd.logicalType == ZDO_TYPE_ROUTER &&
            pd.manufacturerCode == 0x1234 && pd.maxBufferSize == 82 &&
            pd.serverMask == 0x2040,
        "round-trip: type, manufacturer, buffer size, server mask");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee ZDO management self-test ===");

  testPermitJoining();
  testLeave();
  testNodeDesc();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
