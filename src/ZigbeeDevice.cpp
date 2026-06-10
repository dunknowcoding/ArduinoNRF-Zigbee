#include "ZigbeeDevice.h"

#include "ZigbeeAps.h"

namespace nzb {

ZigbeeDeviceObject::ZigbeeDeviceObject()
    : ieeeAddress_(0), nwkAddress_(0xFFFF), endpoints_(nullptr),
      endpointCount_(0) {}

ZigbeeDeviceObject::ZigbeeDeviceObject(
    uint64_t ieeeAddress, uint16_t nwkAddress,
    const ZigbeeEndpointDescriptor* endpoints, uint8_t endpointCount)
    : ieeeAddress_(ieeeAddress), nwkAddress_(nwkAddress),
      endpoints_(endpoints), endpointCount_(endpointCount) {}

void ZigbeeDeviceObject::begin(uint64_t ieeeAddress, uint16_t nwkAddress,
                               const ZigbeeEndpointDescriptor* endpoints,
                               uint8_t endpointCount) {
  ieeeAddress_ = ieeeAddress;
  nwkAddress_ = nwkAddress;
  endpoints_ = endpoints;
  endpointCount_ = endpointCount;
}

const ZigbeeEndpointDescriptor* ZigbeeDeviceObject::findEndpoint(
    uint8_t endpoint) const {
  for (uint8_t i = 0; i < endpointCount_; ++i) {
    if (endpoints_[i].endpoint == endpoint) return &endpoints_[i];
  }
  return nullptr;
}

bool ZigbeeDeviceObject::containsCluster(const uint16_t* clusters,
                                         uint8_t count,
                                         uint16_t clusterId) {
  for (uint8_t i = 0; i < count; ++i) {
    if (clusters[i] == clusterId) return true;
  }
  return false;
}

bool ZigbeeDeviceObject::endpointMatches(
    uint8_t endpoint, uint16_t profileId, const uint16_t* inputClusters,
    uint8_t inputClusterCount, const uint16_t* outputClusters,
    uint8_t outputClusterCount) const {
  const ZigbeeEndpointDescriptor* desc = findEndpoint(endpoint);
  if (!desc || desc->profileId != profileId) return false;
  for (uint8_t i = 0; i < inputClusterCount; ++i) {
    if (!containsCluster(desc->inputClusters, desc->inputClusterCount,
                         inputClusters[i])) {
      return false;
    }
  }
  for (uint8_t i = 0; i < outputClusterCount; ++i) {
    if (!containsCluster(desc->outputClusters, desc->outputClusterCount,
                         outputClusters[i])) {
      return false;
    }
  }
  return true;
}

ZdoSimpleDescriptor ZigbeeDeviceObject::toZdoDescriptor(
    const ZigbeeEndpointDescriptor& endpoint) {
  ZdoSimpleDescriptor desc;
  desc.endpoint = endpoint.endpoint;
  desc.profileId = endpoint.profileId;
  desc.deviceId = endpoint.deviceId;
  desc.deviceVersion = endpoint.deviceVersion;
  desc.inputClusterCount = endpoint.inputClusterCount;
  desc.inputClusters = endpoint.inputClusters;
  desc.outputClusterCount = endpoint.outputClusterCount;
  desc.outputClusters = endpoint.outputClusters;
  return desc;
}

uint8_t ZigbeeDeviceObject::handleRequest(
    uint16_t requestClusterId, const uint8_t* requestPayload,
    uint8_t requestPayloadLen, uint8_t* responsePayload,
    uint8_t responsePayloadMax, uint16_t& responseClusterId) const {
  responseClusterId = 0;
  if (!responsePayload) return 0;

  if (requestClusterId == ZDO_IEEE_ADDR_REQ) {
    ZdoAddressRequest req;
    if (!ZigbeeZdo::parseIeeeAddressRequest(
            requestPayload, requestPayloadLen, req)) {
      return 0;
    }
    if (req.nwkAddress != nwkAddress_) return 0;
    responseClusterId = ZDO_IEEE_ADDR_RSP;
    return ZigbeeZdo::buildAddressResponse(
        responsePayload, responsePayloadMax, req.sequence, ZDO_STATUS_SUCCESS,
        ieeeAddress_, nwkAddress_);
  }

  if (requestClusterId == ZDO_NWK_ADDR_REQ) {
    ZdoAddressRequest req;
    if (!ZigbeeZdo::parseNwkAddressRequest(
            requestPayload, requestPayloadLen, req)) {
      return 0;
    }
    if (req.ieeeAddress != ieeeAddress_) return 0;
    responseClusterId = ZDO_NWK_ADDR_RSP;
    return ZigbeeZdo::buildAddressResponse(
        responsePayload, responsePayloadMax, req.sequence, ZDO_STATUS_SUCCESS,
        ieeeAddress_, nwkAddress_);
  }

  if (requestClusterId == ZDO_ACTIVE_EP_REQ) {
    ZdoActiveEndpointRequest req;
    if (!ZigbeeZdo::parseActiveEndpointRequest(
            requestPayload, requestPayloadLen, req)) {
      return 0;
    }
    if (req.nwkAddress != nwkAddress_) return 0;
    uint8_t endpoints[16];
    uint8_t count = endpointCount_ > sizeof(endpoints) ? sizeof(endpoints)
                                                       : endpointCount_;
    for (uint8_t i = 0; i < count; ++i) endpoints[i] = endpoints_[i].endpoint;
    responseClusterId = ZDO_ACTIVE_EP_RSP;
    return ZigbeeZdo::buildActiveEndpointResponse(
        responsePayload, responsePayloadMax, req.sequence, ZDO_STATUS_SUCCESS,
        nwkAddress_, endpoints, count);
  }

  if (requestClusterId == ZDO_SIMPLE_DESC_REQ) {
    ZdoSimpleDescriptorRequest req;
    if (!ZigbeeZdo::parseSimpleDescriptorRequest(
            requestPayload, requestPayloadLen, req)) {
      return 0;
    }
    if (req.nwkAddress != nwkAddress_) return 0;
    const ZigbeeEndpointDescriptor* endpoint = findEndpoint(req.endpoint);
    responseClusterId = ZDO_SIMPLE_DESC_RSP;
    if (!endpoint) {
      return ZigbeeZdo::buildSimpleDescriptorResponse(
          responsePayload, responsePayloadMax, req.sequence,
          ZDO_STATUS_NO_DESCRIPTOR, nwkAddress_, nullptr);
    }
    ZdoSimpleDescriptor desc = toZdoDescriptor(*endpoint);
    return ZigbeeZdo::buildSimpleDescriptorResponse(
        responsePayload, responsePayloadMax, req.sequence, ZDO_STATUS_SUCCESS,
        nwkAddress_, &desc);
  }

  if (requestClusterId == ZDO_MATCH_DESC_REQ) {
    ZdoMatchDescriptorRequest req;
    if (!ZigbeeZdo::parseMatchDescriptorRequest(
            requestPayload, requestPayloadLen, req)) {
      return 0;
    }
    if (req.nwkAddress != nwkAddress_) return 0;
    uint8_t matches[16];
    uint8_t count = 0;
    for (uint8_t i = 0; i < endpointCount_ && count < sizeof(matches); ++i) {
      if (endpointMatches(endpoints_[i].endpoint, req.profileId,
                          req.inputClusters, req.inputClusterCount,
                          req.outputClusters, req.outputClusterCount)) {
        matches[count++] = endpoints_[i].endpoint;
      }
    }
    responseClusterId = ZDO_MATCH_DESC_RSP;
    return ZigbeeZdo::buildMatchDescriptorResponse(
        responsePayload, responsePayloadMax, req.sequence, ZDO_STATUS_SUCCESS,
        nwkAddress_, matches, count);
  }

  return 0;
}

}  // namespace nzb
