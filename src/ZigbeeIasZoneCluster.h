/*
  ZigbeeIasZoneCluster.h - ZCL IAS Zone cluster (0x0500): security sensors.

  IAS Zone is how a security sensor (door/window contact, motion, smoke, water,
  ...) reports to a control panel. The sensor enrolls with the panel (Zone
  Enroll Request/Response), then reports state with a Zone Status Change
  Notification whose 16-bit zone status carries the alarm/tamper/battery bits.

  This header builds/parses those commands and the zone status bitmask, so a
  sensor device can announce alarms and a panel can decode them - the sensor
  counterpart to the light/switch clusters.
*/
#ifndef NIUS_ZIGBEE_IAS_ZONE_CLUSTER_H
#define NIUS_ZIGBEE_IAS_ZONE_CLUSTER_H

#include <Arduino.h>

namespace nzb {

enum ZclIasZoneCommandId : uint8_t {
  // server -> client
  IAS_CMD_ZONE_STATUS_CHANGE_NOTIFICATION = 0x00,
  IAS_CMD_ZONE_ENROLL_REQUEST = 0x01,
  // client -> server
  IAS_CMD_ZONE_ENROLL_RESPONSE = 0x00,
};

// Zone Status (attribute 0x0002) bitmask.
enum ZclIasZoneStatus : uint16_t {
  IAS_STATUS_ALARM1 = 0x0001,       // e.g. opened / motion
  IAS_STATUS_ALARM2 = 0x0002,
  IAS_STATUS_TAMPER = 0x0004,
  IAS_STATUS_BATTERY = 0x0008,      // low battery
  IAS_STATUS_SUPERVISION = 0x0010,
  IAS_STATUS_RESTORE = 0x0020,
  IAS_STATUS_TROUBLE = 0x0040,
  IAS_STATUS_AC_MAINS = 0x0080,
};

// A common zone type or two (attribute 0x0001).
enum ZclIasZoneType : uint16_t {
  IAS_ZONE_TYPE_CONTACT = 0x0015,   // door/window contact
  IAS_ZONE_TYPE_MOTION = 0x000D,
  IAS_ZONE_TYPE_FIRE = 0x0028,
  IAS_ZONE_TYPE_WATER = 0x002A,
};

enum ZclIasEnrollResponseCode : uint8_t {
  IAS_ENROLL_SUCCESS = 0x00,
  IAS_ENROLL_NOT_SUPPORTED = 0x01,
  IAS_ENROLL_NO_ENROLL_PERMIT = 0x02,
  IAS_ENROLL_TOO_MANY_ZONES = 0x03,
};

class ZigbeeIasZoneCluster {
 public:
  /** Zone Status Change Notification: zone status(2) + extended status(1) +
      zone id(1) + delay(2, 1/4 s). */
  static uint8_t buildStatusChangeNotification(uint8_t* out, uint8_t outMax,
                                               uint16_t zoneStatus,
                                               uint8_t zoneId, uint16_t delay) {
    if (!out || outMax < 6) return 0;
    putLe16(&out[0], zoneStatus);
    out[2] = 0;  // extended status (reserved)
    out[3] = zoneId;
    putLe16(&out[4], delay);
    return 6;
  }
  static bool parseStatusChangeNotification(const uint8_t* payload, uint8_t len,
                                            uint16_t& zoneStatus,
                                            uint8_t& zoneId, uint16_t& delay) {
    if (!payload || len < 6) return false;
    zoneStatus = getLe16(&payload[0]);
    zoneId = payload[3];
    delay = getLe16(&payload[4]);
    return true;
  }

  /** Zone Enroll Request: zone type(2) + manufacturer code(2). */
  static uint8_t buildEnrollRequest(uint8_t* out, uint8_t outMax,
                                    uint16_t zoneType, uint16_t manufacturer) {
    if (!out || outMax < 4) return 0;
    putLe16(&out[0], zoneType);
    putLe16(&out[2], manufacturer);
    return 4;
  }
  static bool parseEnrollRequest(const uint8_t* payload, uint8_t len,
                                 uint16_t& zoneType, uint16_t& manufacturer) {
    if (!payload || len < 4) return false;
    zoneType = getLe16(&payload[0]);
    manufacturer = getLe16(&payload[2]);
    return true;
  }

  /** Zone Enroll Response: enroll response code(1) + zone id(1). */
  static uint8_t buildEnrollResponse(uint8_t* out, uint8_t outMax,
                                     uint8_t responseCode, uint8_t zoneId) {
    if (!out || outMax < 2) return 0;
    out[0] = responseCode;
    out[1] = zoneId;
    return 2;
  }
  static bool parseEnrollResponse(const uint8_t* payload, uint8_t len,
                                  uint8_t& responseCode, uint8_t& zoneId) {
    if (!payload || len < 2) return false;
    responseCode = payload[0];
    zoneId = payload[1];
    return true;
  }

 private:
  static void putLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8);
  }
  static uint16_t getLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_IAS_ZONE_CLUSTER_H
