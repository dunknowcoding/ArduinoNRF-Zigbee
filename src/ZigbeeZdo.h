/*
  ZigbeeZdo.h - Zigbee Device Object payload helpers used by NiusZigbee.

  This is ZDP frame tooling for endpoint/profile discovery. It does not manage
  joining, device state, binding tables, security, or persistent descriptors.
*/
#ifndef NIUS_ZIGBEE_ZDO_H
#define NIUS_ZIGBEE_ZDO_H

#include <Arduino.h>

namespace nzb {

enum ZdoClusterId : uint16_t {
  ZDO_NWK_ADDR_REQ = 0x0000,
  ZDO_IEEE_ADDR_REQ = 0x0001,
  ZDO_NODE_DESC_REQ = 0x0002,
  ZDO_SIMPLE_DESC_REQ = 0x0004,
  ZDO_ACTIVE_EP_REQ = 0x0005,
  ZDO_MATCH_DESC_REQ = 0x0006,
  ZDO_DEVICE_ANNCE = 0x0013,

  ZDO_NWK_ADDR_RSP = 0x8000,
  ZDO_IEEE_ADDR_RSP = 0x8001,
  ZDO_NODE_DESC_RSP = 0x8002,
  ZDO_SIMPLE_DESC_RSP = 0x8004,
  ZDO_ACTIVE_EP_RSP = 0x8005,
  ZDO_MATCH_DESC_RSP = 0x8006
};

enum ZdoStatus : uint8_t {
  ZDO_STATUS_SUCCESS = 0x00,
  ZDO_STATUS_INVALID_REQTYPE = 0x80,
  ZDO_STATUS_DEVICE_NOT_FOUND = 0x81,
  ZDO_STATUS_NOT_ACTIVE = 0x82,
  ZDO_STATUS_NOT_SUPPORTED = 0x84,
  ZDO_STATUS_NO_DESCRIPTOR = 0x89
};

enum ZdoAddressRequestType : uint8_t {
  ZDO_ADDR_REQ_SINGLE = 0x00,
  ZDO_ADDR_REQ_EXTENDED = 0x01
};

struct ZdoAddressRequest {
  uint8_t sequence;
  uint64_t ieeeAddress;
  uint16_t nwkAddress;
  uint8_t requestType;
  uint8_t startIndex;
};

struct ZdoAddressResponse {
  uint8_t sequence;
  uint8_t status;
  uint64_t ieeeAddress;
  uint16_t nwkAddress;
  uint8_t associatedDeviceCount;
  uint8_t startIndex;
  const uint16_t* associatedDevices;
};

struct ZdoActiveEndpointRequest {
  uint8_t sequence;
  uint16_t nwkAddress;
};

struct ZdoActiveEndpointResponse {
  uint8_t sequence;
  uint8_t status;
  uint16_t nwkAddress;
  uint8_t endpointCount;
  const uint8_t* endpoints;
};

struct ZdoSimpleDescriptor {
  uint8_t endpoint;
  uint16_t profileId;
  uint16_t deviceId;
  uint8_t deviceVersion;
  uint8_t inputClusterCount;
  const uint16_t* inputClusters;
  uint8_t outputClusterCount;
  const uint16_t* outputClusters;
};

struct ZdoSimpleDescriptorRequest {
  uint8_t sequence;
  uint16_t nwkAddress;
  uint8_t endpoint;
};

struct ZdoSimpleDescriptorResponse {
  uint8_t sequence;
  uint8_t status;
  uint16_t nwkAddress;
  ZdoSimpleDescriptor descriptor;
};

struct ZdoMatchDescriptorRequest {
  uint8_t sequence;
  uint16_t nwkAddress;
  uint16_t profileId;
  uint8_t inputClusterCount;
  const uint16_t* inputClusters;
  uint8_t outputClusterCount;
  const uint16_t* outputClusters;
};

struct ZdoMatchDescriptorResponse {
  uint8_t sequence;
  uint8_t status;
  uint16_t nwkAddress;
  uint8_t endpointCount;
  const uint8_t* endpoints;
};

struct ZdoDeviceAnnounce {
  uint8_t sequence;
  uint16_t nwkAddress;
  uint64_t ieeeAddress;
  uint8_t capability;
};

class ZigbeeZdo {
 public:
  static const uint8_t kEndpoint = 0;
  static const uint8_t kMaxPayload = 96;

  static uint8_t buildNwkAddressRequest(uint8_t* out, uint8_t outMax,
                                        uint8_t sequence,
                                        uint64_t ieeeAddress,
                                        uint8_t requestType = ZDO_ADDR_REQ_SINGLE,
                                        uint8_t startIndex = 0);
  static bool parseNwkAddressRequest(const uint8_t* payload, uint8_t payloadLen,
                                     ZdoAddressRequest& request);

  static uint8_t buildIeeeAddressRequest(uint8_t* out, uint8_t outMax,
                                         uint8_t sequence,
                                         uint16_t nwkAddress,
                                         uint8_t requestType = ZDO_ADDR_REQ_SINGLE,
                                         uint8_t startIndex = 0);
  static bool parseIeeeAddressRequest(const uint8_t* payload, uint8_t payloadLen,
                                      ZdoAddressRequest& request);

  static uint8_t buildAddressResponse(uint8_t* out, uint8_t outMax,
                                      uint8_t sequence, uint8_t status,
                                      uint64_t ieeeAddress,
                                      uint16_t nwkAddress,
                                      const uint16_t* associatedDevices = nullptr,
                                      uint8_t associatedDeviceCount = 0,
                                      uint8_t startIndex = 0);
  static bool parseAddressResponse(const uint8_t* payload, uint8_t payloadLen,
                                   ZdoAddressResponse& response);

  static uint8_t buildActiveEndpointRequest(uint8_t* out, uint8_t outMax,
                                            uint8_t sequence,
                                            uint16_t nwkAddress);
  static bool parseActiveEndpointRequest(const uint8_t* payload,
                                         uint8_t payloadLen,
                                         ZdoActiveEndpointRequest& request);

  static uint8_t buildActiveEndpointResponse(uint8_t* out, uint8_t outMax,
                                             uint8_t sequence, uint8_t status,
                                             uint16_t nwkAddress,
                                             const uint8_t* endpoints,
                                             uint8_t endpointCount);
  static bool parseActiveEndpointResponse(const uint8_t* payload,
                                          uint8_t payloadLen,
                                          ZdoActiveEndpointResponse& response);

  static uint8_t buildSimpleDescriptor(uint8_t* out, uint8_t outMax,
                                       const ZdoSimpleDescriptor& descriptor);
  static bool parseSimpleDescriptor(const uint8_t* payload, uint8_t payloadLen,
                                    ZdoSimpleDescriptor& descriptor);

  static uint8_t buildSimpleDescriptorRequest(uint8_t* out, uint8_t outMax,
                                              uint8_t sequence,
                                              uint16_t nwkAddress,
                                              uint8_t endpoint);
  static bool parseSimpleDescriptorRequest(const uint8_t* payload,
                                           uint8_t payloadLen,
                                           ZdoSimpleDescriptorRequest& request);

  static uint8_t buildSimpleDescriptorResponse(uint8_t* out, uint8_t outMax,
                                               uint8_t sequence, uint8_t status,
                                               uint16_t nwkAddress,
                                               const ZdoSimpleDescriptor* descriptor);
  static bool parseSimpleDescriptorResponse(const uint8_t* payload,
                                            uint8_t payloadLen,
                                            ZdoSimpleDescriptorResponse& response);

  static uint8_t buildMatchDescriptorRequest(uint8_t* out, uint8_t outMax,
                                             uint8_t sequence,
                                             uint16_t nwkAddress,
                                             uint16_t profileId,
                                             const uint16_t* inputClusters,
                                             uint8_t inputClusterCount,
                                             const uint16_t* outputClusters,
                                             uint8_t outputClusterCount);
  static bool parseMatchDescriptorRequest(const uint8_t* payload,
                                          uint8_t payloadLen,
                                          ZdoMatchDescriptorRequest& request);

  static uint8_t buildMatchDescriptorResponse(uint8_t* out, uint8_t outMax,
                                              uint8_t sequence, uint8_t status,
                                              uint16_t nwkAddress,
                                              const uint8_t* endpoints,
                                              uint8_t endpointCount);
  static bool parseMatchDescriptorResponse(const uint8_t* payload,
                                           uint8_t payloadLen,
                                           ZdoMatchDescriptorResponse& response);

  static uint8_t buildDeviceAnnounce(uint8_t* out, uint8_t outMax,
                                     uint8_t sequence, uint16_t nwkAddress,
                                     uint64_t ieeeAddress,
                                     uint8_t capability);
  static bool parseDeviceAnnounce(const uint8_t* payload, uint8_t payloadLen,
                                  ZdoDeviceAnnounce& announce);

 private:
  static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  }
  static void writeLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
  }
  static uint64_t readLe64(const uint8_t* p);
  static void writeLe64(uint8_t* p, uint64_t v);
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_ZDO_H
