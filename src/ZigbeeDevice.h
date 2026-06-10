/*
  ZigbeeDevice.h - small local Zigbee Device Object descriptor store.

  This answers a useful subset of ZDO discovery requests for statically-defined
  endpoints. It is not a join manager, binding table, or security manager.
*/
#ifndef NIUS_ZIGBEE_DEVICE_H
#define NIUS_ZIGBEE_DEVICE_H

#include <Arduino.h>
#include "ZigbeeZdo.h"

namespace nzb {

struct ZigbeeEndpointDescriptor {
  uint8_t endpoint;
  uint16_t profileId;
  uint16_t deviceId;
  uint8_t deviceVersion;
  const uint16_t* inputClusters;
  uint8_t inputClusterCount;
  const uint16_t* outputClusters;
  uint8_t outputClusterCount;
};

class ZigbeeDeviceObject {
 public:
  ZigbeeDeviceObject();
  ZigbeeDeviceObject(uint64_t ieeeAddress, uint16_t nwkAddress,
                     const ZigbeeEndpointDescriptor* endpoints,
                     uint8_t endpointCount);

  void begin(uint64_t ieeeAddress, uint16_t nwkAddress,
             const ZigbeeEndpointDescriptor* endpoints,
             uint8_t endpointCount);
  void setNetworkAddress(uint16_t nwkAddress) { nwkAddress_ = nwkAddress; }
  uint64_t ieeeAddress() const { return ieeeAddress_; }
  uint16_t nwkAddress() const { return nwkAddress_; }
  uint8_t endpointCount() const { return endpointCount_; }

  const ZigbeeEndpointDescriptor* findEndpoint(uint8_t endpoint) const;
  bool endpointMatches(uint8_t endpoint, uint16_t profileId,
                       const uint16_t* inputClusters,
                       uint8_t inputClusterCount,
                       const uint16_t* outputClusters,
                       uint8_t outputClusterCount) const;

  /**
   * Build a response for a supported ZDO request.
   *
   * @return response payload length, or 0 if the request is unsupported,
   *         malformed, or not addressed to this device.
   */
  uint8_t handleRequest(uint16_t requestClusterId,
                        const uint8_t* requestPayload,
                        uint8_t requestPayloadLen,
                        uint8_t* responsePayload,
                        uint8_t responsePayloadMax,
                        uint16_t& responseClusterId) const;

 private:
  uint64_t ieeeAddress_;
  uint16_t nwkAddress_;
  const ZigbeeEndpointDescriptor* endpoints_;
  uint8_t endpointCount_;

  static bool containsCluster(const uint16_t* clusters, uint8_t count,
                              uint16_t clusterId);
  static ZdoSimpleDescriptor toZdoDescriptor(
      const ZigbeeEndpointDescriptor& endpoint);
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_DEVICE_H
