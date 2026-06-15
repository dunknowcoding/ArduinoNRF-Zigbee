/*
  CC2530_DeviceClusters - self-test for the device clusters added to NiusZigbee:
  Door Lock (0x0101), Temperature Measurement (0x0402), Relative Humidity
  Measurement (0x0405), Occupancy Sensing (0x0406), and Electrical Measurement
  (0x0B04).

  For each cluster it drives the local state, builds a ZCL Read Attributes
  Response payload, parses the records back, and checks the attribute id, ZCL
  data type and decoded value. Door Lock additionally exercises the
  Lock/Unlock/Toggle command handler (including the actuator-disabled failure
  path). No radio traffic is needed, so it runs on board1 via J-Link.
*/

#include <ZigbeeZcl.h>
#include <ZigbeeDoorLockCluster.h>
#include <ZigbeeTemperatureMeasurementCluster.h>
#include <ZigbeeHumidityMeasurementCluster.h>
#include <ZigbeeOccupancyCluster.h>
#include <ZigbeeElectricalMeasurementCluster.h>

using namespace nzb;

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

// Find a Read-Attributes-Response record for attrId in [buf,len). A record is
// attrId(2) + status(1) + type(1) + value. Returns the value offset (and type),
// or -1 if not found. Only fixed-width types used by these clusters are walked.
static int findRecord(const uint8_t* buf, uint8_t len, uint16_t attrId,
                      uint8_t& outType) {
  uint8_t i = 0;
  while (i + 4 <= len) {
    uint16_t id = (uint16_t)buf[i] | ((uint16_t)buf[i + 1] << 8);
    uint8_t status = buf[i + 2];
    uint8_t type = buf[i + 3];
    uint8_t vlen = 0;
    if (status == ZCL_STATUS_SUCCESS) {
      switch (type) {
        case ZCL_TYPE_BOOLEAN:
        case ZCL_TYPE_UINT8:
        case ZCL_TYPE_INT8:
        case ZCL_TYPE_MAP8:
        case ZCL_TYPE_ENUM8: vlen = 1; break;
        case ZCL_TYPE_UINT16:
        case ZCL_TYPE_INT16: vlen = 2; break;
        default: return -1;  // unexpected type for this test set
      }
    }
    if (id == attrId) { outType = type; return (int)(i + 4); }
    i += 4 + vlen;
  }
  return -1;
}

static uint16_t rd16(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

void testTemperature() {
  Serial.println("Temperature Measurement (0x0402):");
  ZigbeeTemperatureMeasurementCluster t;
  t.setMeasuredCelsius(21.5f);
  check(t.measuredRaw() == 2150, "21.5 C -> raw 2150");
  t.setMeasuredCelsius(-5.0f);
  check(t.measuredRaw() == -500, "-5.0 C -> raw -500");

  uint16_t ids[3] = {t.kAttrMeasuredValue, t.kAttrMinMeasuredValue,
                     t.kAttrMaxMeasuredValue};
  uint8_t req[16];
  uint8_t reqLen = ZigbeeZcl::buildReadAttributesPayload(req, sizeof(req), ids, 3);
  uint8_t resp[48];
  uint8_t n = t.buildReadAttributesResponsePayload(req, reqLen, resp, sizeof(resp));
  check(n > 0, "build read-attrs response");

  uint8_t type = 0;
  int off = findRecord(resp, n, t.kAttrMeasuredValue, type);
  check(off >= 0 && type == ZCL_TYPE_INT16, "MeasuredValue is int16");
  check(off >= 0 && (int16_t)rd16(&resp[off]) == -500, "MeasuredValue == -500");

  uint8_t rep[8];
  uint8_t rn = t.buildReport(rep, sizeof(rep));
  check(rn == 5 && rep[2] == ZCL_TYPE_INT16 && (int16_t)rd16(&rep[3]) == -500,
        "report payload int16 == -500");
}

void testHumidity() {
  Serial.println("Relative Humidity (0x0405):");
  ZigbeeHumidityMeasurementCluster h;
  h.setMeasuredPercent(48.3f);
  check(h.measuredRaw() == 4830, "48.3 % -> raw 4830");
  h.setMeasuredPercent(120.0f);
  check(h.measuredRaw() == 10000, "clamp >100 % -> 10000");
  h.setMeasuredPercent(48.3f);

  uint16_t ids[1] = {h.kAttrMeasuredValue};
  uint8_t req[8];
  uint8_t reqLen = ZigbeeZcl::buildReadAttributesPayload(req, sizeof(req), ids, 1);
  uint8_t resp[16];
  uint8_t n = h.buildReadAttributesResponsePayload(req, reqLen, resp, sizeof(resp));
  uint8_t type = 0;
  int off = findRecord(resp, n, h.kAttrMeasuredValue, type);
  check(off >= 0 && type == ZCL_TYPE_UINT16, "MeasuredValue is uint16");
  check(off >= 0 && rd16(&resp[off]) == 4830, "MeasuredValue == 4830");
}

void testOccupancy() {
  Serial.println("Occupancy Sensing (0x0406):");
  ZigbeeOccupancyCluster o(OCCUPANCY_SENSOR_PIR);
  check(o.occupancyBitmap() == 0x00, "starts unoccupied");
  o.setOccupied(true);
  check(o.occupancyBitmap() == 0x01, "occupied -> bit0 set");

  uint16_t ids[2] = {o.kAttrOccupancy, o.kAttrOccupancySensorType};
  uint8_t req[12];
  uint8_t reqLen = ZigbeeZcl::buildReadAttributesPayload(req, sizeof(req), ids, 2);
  uint8_t resp[24];
  uint8_t n = o.buildReadAttributesResponsePayload(req, reqLen, resp, sizeof(resp));
  uint8_t type = 0;
  int off = findRecord(resp, n, o.kAttrOccupancy, type);
  check(off >= 0 && type == ZCL_TYPE_MAP8 && resp[off] == 0x01,
        "Occupancy is map8 == 0x01");
  off = findRecord(resp, n, o.kAttrOccupancySensorType, type);
  check(off >= 0 && type == ZCL_TYPE_ENUM8 && resp[off] == OCCUPANCY_SENSOR_PIR,
        "SensorType is enum8 == PIR");

  uint8_t rep[8];
  uint8_t rn = o.buildReport(rep, sizeof(rep));
  check(rn == 4 && rep[2] == ZCL_TYPE_MAP8 && rep[3] == 0x01, "report map8 == 0x01");
}

void testDoorLock() {
  Serial.println("Door Lock (0x0101):");
  ZigbeeDoorLockCluster lock;
  check(lock.isLocked(), "starts locked");

  uint8_t status = 0xFF;
  check(lock.applyCommand(DOOR_LOCK_CMD_UNLOCK, status) &&
            status == ZCL_STATUS_SUCCESS && !lock.isLocked(),
        "Unlock -> unlocked, status SUCCESS");
  check(lock.applyCommand(DOOR_LOCK_CMD_TOGGLE, status) && lock.isLocked(),
        "Toggle -> locked");
  check(lock.applyCommand(DOOR_LOCK_CMD_TOGGLE, status) && !lock.isLocked(),
        "Toggle -> unlocked");
  check(!lock.applyCommand(0x7F, status), "unknown command rejected");

  // Actuator disabled -> command acknowledged but FAILURE, state unchanged.
  lock.applyCommand(DOOR_LOCK_CMD_LOCK, status);
  lock.setActuatorEnabled(false);
  bool locked = lock.isLocked();
  check(lock.applyCommand(DOOR_LOCK_CMD_UNLOCK, status) &&
            status == ZCL_STATUS_FAILURE && lock.isLocked() == locked,
        "actuator disabled -> FAILURE, state held");
  lock.setActuatorEnabled(true);

  uint8_t cr[2];
  uint8_t crn = ZigbeeDoorLockCluster::buildCommandResponse(cr, sizeof(cr),
                                                            ZCL_STATUS_SUCCESS);
  check(crn == 1 && cr[0] == ZCL_STATUS_SUCCESS, "command response status byte");

  uint16_t ids[1] = {lock.kAttrLockState};
  uint8_t req[8];
  uint8_t reqLen = ZigbeeZcl::buildReadAttributesPayload(req, sizeof(req), ids, 1);
  uint8_t resp[12];
  uint8_t n = lock.buildReadAttributesResponsePayload(req, reqLen, resp, sizeof(resp));
  uint8_t type = 0;
  int off = findRecord(resp, n, lock.kAttrLockState, type);
  check(off >= 0 && type == ZCL_TYPE_ENUM8 && resp[off] == lock.lockState(),
        "LockState is enum8 matching state");
}

void testElectrical() {
  Serial.println("Electrical Measurement (0x0B04):");
  ZigbeeElectricalMeasurementCluster e;
  e.setRmsVoltage(231);
  e.setRmsCurrent(1500);   // mA
  e.setActivePower(-345);  // exporting (negative)

  uint16_t ids[2] = {e.kAttrRmsVoltage, e.kAttrActivePower};
  uint8_t req[12];
  uint8_t reqLen = ZigbeeZcl::buildReadAttributesPayload(req, sizeof(req), ids, 2);
  uint8_t resp[24];
  uint8_t n = e.buildReadAttributesResponsePayload(req, reqLen, resp, sizeof(resp));
  uint8_t type = 0;
  int off = findRecord(resp, n, e.kAttrRmsVoltage, type);
  check(off >= 0 && type == ZCL_TYPE_UINT16 && rd16(&resp[off]) == 231,
        "RMSVoltage is uint16 == 231");
  off = findRecord(resp, n, e.kAttrActivePower, type);
  check(off >= 0 && type == ZCL_TYPE_INT16 && (int16_t)rd16(&resp[off]) == -345,
        "ActivePower is int16 == -345");

  uint8_t rep[8];
  uint8_t rn = e.buildReport(rep, sizeof(rep));
  check(rn == 5 && rep[2] == ZCL_TYPE_INT16 && (int16_t)rd16(&rep[3]) == -345,
        "report ActivePower int16 == -345");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 4000) {
  }
  Serial.println();
  Serial.println("=== NiusZigbee device-cluster self-test ===");
  testTemperature();
  testHumidity();
  testOccupancy();
  testDoorLock();
  testElectrical();
  Serial.println("-------------------------------------------");
  Serial.print("RESULT: ");
  Serial.print(passes);
  Serial.print(" passed, ");
  Serial.print(fails);
  Serial.print(" failed -> ");
  Serial.println(fails == 0 ? "ALL PASS" : "FAILURES");
}

void loop() {
  delay(1000);
}
