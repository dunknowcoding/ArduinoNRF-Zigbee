#include "ZigbeeZdo.h"

namespace nzb {

uint64_t ZigbeeZdo::readLe64(const uint8_t* p) {
  uint64_t v = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    v |= (uint64_t)p[i] << (8 * i);
  }
  return v;
}

void ZigbeeZdo::writeLe64(uint8_t* p, uint64_t v) {
  for (uint8_t i = 0; i < 8; ++i) {
    p[i] = (uint8_t)(v >> (8 * i));
  }
}

uint8_t ZigbeeZdo::buildNwkAddressRequest(uint8_t* out, uint8_t outMax,
                                          uint8_t sequence,
                                          uint64_t ieeeAddress,
                                          uint8_t requestType,
                                          uint8_t startIndex) {
  if (!out || outMax < 11) return 0;
  out[0] = sequence;
  writeLe64(&out[1], ieeeAddress);
  out[9] = requestType;
  out[10] = startIndex;
  return 11;
}

bool ZigbeeZdo::parseNwkAddressRequest(const uint8_t* payload,
                                       uint8_t payloadLen,
                                       ZdoAddressRequest& request) {
  request = ZdoAddressRequest();
  if (!payload || payloadLen < 11) return false;
  request.sequence = payload[0];
  request.ieeeAddress = readLe64(&payload[1]);
  request.requestType = payload[9];
  request.startIndex = payload[10];
  return true;
}

uint8_t ZigbeeZdo::buildIeeeAddressRequest(uint8_t* out, uint8_t outMax,
                                           uint8_t sequence,
                                           uint16_t nwkAddress,
                                           uint8_t requestType,
                                           uint8_t startIndex) {
  if (!out || outMax < 5) return 0;
  out[0] = sequence;
  writeLe16(&out[1], nwkAddress);
  out[3] = requestType;
  out[4] = startIndex;
  return 5;
}

bool ZigbeeZdo::parseIeeeAddressRequest(const uint8_t* payload,
                                        uint8_t payloadLen,
                                        ZdoAddressRequest& request) {
  request = ZdoAddressRequest();
  if (!payload || payloadLen < 5) return false;
  request.sequence = payload[0];
  request.nwkAddress = readLe16(&payload[1]);
  request.requestType = payload[3];
  request.startIndex = payload[4];
  return true;
}

uint8_t ZigbeeZdo::buildAddressResponse(uint8_t* out, uint8_t outMax,
                                        uint8_t sequence, uint8_t status,
                                        uint64_t ieeeAddress,
                                        uint16_t nwkAddress,
                                        const uint16_t* associatedDevices,
                                        uint8_t associatedDeviceCount,
                                        uint8_t startIndex) {
  uint8_t n = (uint8_t)(12 + 2 + associatedDeviceCount * 2);
  if (!out || outMax < n) return 0;
  if (associatedDeviceCount > 0 && !associatedDevices) return 0;
  out[0] = sequence;
  out[1] = status;
  writeLe64(&out[2], ieeeAddress);
  writeLe16(&out[10], nwkAddress);
  out[12] = associatedDeviceCount;
  out[13] = startIndex;
  for (uint8_t i = 0; i < associatedDeviceCount; ++i) {
    writeLe16(&out[14 + i * 2], associatedDevices[i]);
  }
  return n;
}

bool ZigbeeZdo::parseAddressResponse(const uint8_t* payload, uint8_t payloadLen,
                                     ZdoAddressResponse& response) {
  response = ZdoAddressResponse();
  response.associatedDevices = nullptr;
  if (!payload || payloadLen < 12) return false;
  response.sequence = payload[0];
  response.status = payload[1];
  response.ieeeAddress = readLe64(&payload[2]);
  response.nwkAddress = readLe16(&payload[10]);
  if (payloadLen >= 14) {
    response.associatedDeviceCount = payload[12];
    response.startIndex = payload[13];
    if (payloadLen < (uint8_t)(14 + response.associatedDeviceCount * 2)) {
      return false;
    }
    response.associatedDevices = (const uint16_t*)&payload[14];
  }
  return true;
}

uint8_t ZigbeeZdo::buildActiveEndpointRequest(uint8_t* out, uint8_t outMax,
                                              uint8_t sequence,
                                              uint16_t nwkAddress) {
  if (!out || outMax < 3) return 0;
  out[0] = sequence;
  writeLe16(&out[1], nwkAddress);
  return 3;
}

bool ZigbeeZdo::parseActiveEndpointRequest(
    const uint8_t* payload, uint8_t payloadLen,
    ZdoActiveEndpointRequest& request) {
  request = ZdoActiveEndpointRequest();
  if (!payload || payloadLen < 3) return false;
  request.sequence = payload[0];
  request.nwkAddress = readLe16(&payload[1]);
  return true;
}

uint8_t ZigbeeZdo::buildActiveEndpointResponse(
    uint8_t* out, uint8_t outMax, uint8_t sequence, uint8_t status,
    uint16_t nwkAddress, const uint8_t* endpoints, uint8_t endpointCount) {
  if (!out || outMax < (uint8_t)(5 + endpointCount)) return 0;
  if (endpointCount > 0 && !endpoints) return 0;
  out[0] = sequence;
  out[1] = status;
  writeLe16(&out[2], nwkAddress);
  out[4] = endpointCount;
  for (uint8_t i = 0; i < endpointCount; ++i) out[5 + i] = endpoints[i];
  return (uint8_t)(5 + endpointCount);
}

bool ZigbeeZdo::parseActiveEndpointResponse(
    const uint8_t* payload, uint8_t payloadLen,
    ZdoActiveEndpointResponse& response) {
  response = ZdoActiveEndpointResponse();
  response.endpoints = nullptr;
  if (!payload || payloadLen < 5) return false;
  response.sequence = payload[0];
  response.status = payload[1];
  response.nwkAddress = readLe16(&payload[2]);
  response.endpointCount = payload[4];
  if (payloadLen < (uint8_t)(5 + response.endpointCount)) return false;
  response.endpoints = &payload[5];
  return true;
}

uint8_t ZigbeeZdo::buildSimpleDescriptor(
    uint8_t* out, uint8_t outMax, const ZdoSimpleDescriptor& descriptor) {
  uint8_t n = (uint8_t)(8 + descriptor.inputClusterCount * 2 +
                        descriptor.outputClusterCount * 2);
  if (!out || outMax < n) return 0;
  if (descriptor.inputClusterCount > 0 && !descriptor.inputClusters) return 0;
  if (descriptor.outputClusterCount > 0 && !descriptor.outputClusters) return 0;
  out[0] = descriptor.endpoint;
  writeLe16(&out[1], descriptor.profileId);
  writeLe16(&out[3], descriptor.deviceId);
  out[5] = (uint8_t)(descriptor.deviceVersion & 0x0F);
  out[6] = descriptor.inputClusterCount;
  uint8_t idx = 7;
  for (uint8_t i = 0; i < descriptor.inputClusterCount; ++i) {
    writeLe16(&out[idx], descriptor.inputClusters[i]);
    idx += 2;
  }
  out[idx++] = descriptor.outputClusterCount;
  for (uint8_t i = 0; i < descriptor.outputClusterCount; ++i) {
    writeLe16(&out[idx], descriptor.outputClusters[i]);
    idx += 2;
  }
  return idx;
}

bool ZigbeeZdo::parseSimpleDescriptor(const uint8_t* payload,
                                      uint8_t payloadLen,
                                      ZdoSimpleDescriptor& descriptor) {
  descriptor = ZdoSimpleDescriptor();
  if (!payload || payloadLen < 8) return false;
  descriptor.endpoint = payload[0];
  descriptor.profileId = readLe16(&payload[1]);
  descriptor.deviceId = readLe16(&payload[3]);
  descriptor.deviceVersion = (uint8_t)(payload[5] & 0x0F);
  descriptor.inputClusterCount = payload[6];
  uint8_t idx = 7;
  if (payloadLen < (uint8_t)(idx + descriptor.inputClusterCount * 2 + 1)) {
    return false;
  }
  descriptor.inputClusters = (const uint16_t*)&payload[idx];
  idx = (uint8_t)(idx + descriptor.inputClusterCount * 2);
  descriptor.outputClusterCount = payload[idx++];
  if (payloadLen < (uint8_t)(idx + descriptor.outputClusterCount * 2)) {
    return false;
  }
  descriptor.outputClusters = (const uint16_t*)&payload[idx];
  return true;
}

uint8_t ZigbeeZdo::buildSimpleDescriptorRequest(uint8_t* out, uint8_t outMax,
                                                uint8_t sequence,
                                                uint16_t nwkAddress,
                                                uint8_t endpoint) {
  if (!out || outMax < 4) return 0;
  out[0] = sequence;
  writeLe16(&out[1], nwkAddress);
  out[3] = endpoint;
  return 4;
}

bool ZigbeeZdo::parseSimpleDescriptorRequest(
    const uint8_t* payload, uint8_t payloadLen,
    ZdoSimpleDescriptorRequest& request) {
  request = ZdoSimpleDescriptorRequest();
  if (!payload || payloadLen < 4) return false;
  request.sequence = payload[0];
  request.nwkAddress = readLe16(&payload[1]);
  request.endpoint = payload[3];
  return true;
}

uint8_t ZigbeeZdo::buildSimpleDescriptorResponse(
    uint8_t* out, uint8_t outMax, uint8_t sequence, uint8_t status,
    uint16_t nwkAddress, const ZdoSimpleDescriptor* descriptor) {
  if (!out || outMax < 5) return 0;
  out[0] = sequence;
  out[1] = status;
  writeLe16(&out[2], nwkAddress);
  out[4] = 0;
  if (status != ZDO_STATUS_SUCCESS || !descriptor) return 5;
  uint8_t n = buildSimpleDescriptor(&out[5], (uint8_t)(outMax - 5), *descriptor);
  if (n == 0) return 0;
  out[4] = n;
  return (uint8_t)(5 + n);
}

bool ZigbeeZdo::parseSimpleDescriptorResponse(
    const uint8_t* payload, uint8_t payloadLen,
    ZdoSimpleDescriptorResponse& response) {
  response = ZdoSimpleDescriptorResponse();
  if (!payload || payloadLen < 5) return false;
  response.sequence = payload[0];
  response.status = payload[1];
  response.nwkAddress = readLe16(&payload[2]);
  uint8_t descLen = payload[4];
  if (payloadLen < (uint8_t)(5 + descLen)) return false;
  if (descLen == 0) return true;
  return parseSimpleDescriptor(&payload[5], descLen, response.descriptor);
}

uint8_t ZigbeeZdo::buildMatchDescriptorRequest(
    uint8_t* out, uint8_t outMax, uint8_t sequence, uint16_t nwkAddress,
    uint16_t profileId, const uint16_t* inputClusters,
    uint8_t inputClusterCount, const uint16_t* outputClusters,
    uint8_t outputClusterCount) {
  uint8_t n = (uint8_t)(6 + inputClusterCount * 2 + outputClusterCount * 2);
  if (!out || outMax < n) return 0;
  if (inputClusterCount > 0 && !inputClusters) return 0;
  if (outputClusterCount > 0 && !outputClusters) return 0;
  out[0] = sequence;
  writeLe16(&out[1], nwkAddress);
  writeLe16(&out[3], profileId);
  out[5] = inputClusterCount;
  uint8_t idx = 6;
  for (uint8_t i = 0; i < inputClusterCount; ++i) {
    writeLe16(&out[idx], inputClusters[i]);
    idx += 2;
  }
  out[idx++] = outputClusterCount;
  for (uint8_t i = 0; i < outputClusterCount; ++i) {
    writeLe16(&out[idx], outputClusters[i]);
    idx += 2;
  }
  return idx;
}

bool ZigbeeZdo::parseMatchDescriptorRequest(
    const uint8_t* payload, uint8_t payloadLen,
    ZdoMatchDescriptorRequest& request) {
  request = ZdoMatchDescriptorRequest();
  if (!payload || payloadLen < 6) return false;
  request.sequence = payload[0];
  request.nwkAddress = readLe16(&payload[1]);
  request.profileId = readLe16(&payload[3]);
  request.inputClusterCount = payload[5];
  uint8_t idx = 6;
  if (payloadLen < (uint8_t)(idx + request.inputClusterCount * 2 + 1)) {
    return false;
  }
  request.inputClusters = (const uint16_t*)&payload[idx];
  idx = (uint8_t)(idx + request.inputClusterCount * 2);
  request.outputClusterCount = payload[idx++];
  if (payloadLen < (uint8_t)(idx + request.outputClusterCount * 2)) {
    return false;
  }
  request.outputClusters = (const uint16_t*)&payload[idx];
  return true;
}

uint8_t ZigbeeZdo::buildMatchDescriptorResponse(
    uint8_t* out, uint8_t outMax, uint8_t sequence, uint8_t status,
    uint16_t nwkAddress, const uint8_t* endpoints, uint8_t endpointCount) {
  if (!out || outMax < (uint8_t)(5 + endpointCount)) return 0;
  if (endpointCount > 0 && !endpoints) return 0;
  out[0] = sequence;
  out[1] = status;
  writeLe16(&out[2], nwkAddress);
  out[4] = endpointCount;
  for (uint8_t i = 0; i < endpointCount; ++i) out[5 + i] = endpoints[i];
  return (uint8_t)(5 + endpointCount);
}

bool ZigbeeZdo::parseMatchDescriptorResponse(
    const uint8_t* payload, uint8_t payloadLen,
    ZdoMatchDescriptorResponse& response) {
  response = ZdoMatchDescriptorResponse();
  response.endpoints = nullptr;
  if (!payload || payloadLen < 5) return false;
  response.sequence = payload[0];
  response.status = payload[1];
  response.nwkAddress = readLe16(&payload[2]);
  response.endpointCount = payload[4];
  if (payloadLen < (uint8_t)(5 + response.endpointCount)) return false;
  response.endpoints = &payload[5];
  return true;
}

uint8_t ZigbeeZdo::buildDeviceAnnounce(uint8_t* out, uint8_t outMax,
                                       uint8_t sequence, uint16_t nwkAddress,
                                       uint64_t ieeeAddress,
                                       uint8_t capability) {
  if (!out || outMax < 12) return 0;
  out[0] = sequence;
  writeLe16(&out[1], nwkAddress);
  writeLe64(&out[3], ieeeAddress);
  out[11] = capability;
  return 12;
}

bool ZigbeeZdo::parseDeviceAnnounce(const uint8_t* payload,
                                    uint8_t payloadLen,
                                    ZdoDeviceAnnounce& announce) {
  announce = ZdoDeviceAnnounce();
  if (!payload || payloadLen < 12) return false;
  announce.sequence = payload[0];
  announce.nwkAddress = readLe16(&payload[1]);
  announce.ieeeAddress = readLe64(&payload[3]);
  announce.capability = payload[11];
  return true;
}

// -- Network management (Mgmt_Lqi / Mgmt_Rtg) -----------------------------

uint8_t ZigbeeZdo::buildMgmtLqiRequest(uint8_t* out, uint8_t outMax,
                                       uint8_t sequence, uint8_t startIndex) {
  if (!out || outMax < 2) return 0;
  out[0] = sequence;
  out[1] = startIndex;
  return 2;
}

uint8_t ZigbeeZdo::buildMgmtRtgRequest(uint8_t* out, uint8_t outMax,
                                       uint8_t sequence, uint8_t startIndex) {
  return buildMgmtLqiRequest(out, outMax, sequence, startIndex);
}

bool ZigbeeZdo::parseMgmtRequest(const uint8_t* payload, uint8_t payloadLen,
                                 ZdoMgmtRequest& request) {
  request = ZdoMgmtRequest();
  if (!payload || payloadLen < 2) return false;
  request.sequence = payload[0];
  request.startIndex = payload[1];
  return true;
}

uint8_t ZigbeeZdo::buildMgmtLqiResponse(uint8_t* out, uint8_t outMax,
                                        uint8_t sequence, uint8_t status,
                                        uint8_t neighborTableEntries,
                                        uint8_t startIndex,
                                        const ZdoNeighborListEntry* entries,
                                        uint8_t listCount) {
  if (!out) return 0;
  if (listCount > 0 && !entries) return 0;
  uint16_t needed = (uint16_t)5 + (uint16_t)listCount * kNeighborEntryLen;
  if (outMax < needed) return 0;

  out[0] = sequence;
  out[1] = status;
  out[2] = neighborTableEntries;
  out[3] = startIndex;
  out[4] = listCount;
  uint8_t* p = &out[5];
  for (uint8_t i = 0; i < listCount; ++i) {
    const ZdoNeighborListEntry& e = entries[i];
    writeLe64(&p[0], e.extendedPanId);
    writeLe64(&p[8], e.extendedAddress);
    writeLe16(&p[16], e.nwkAddress);
    p[18] = (uint8_t)((e.deviceType & 0x03) | ((e.rxOnWhenIdle & 0x03) << 2) |
                      ((e.relationship & 0x07) << 4));
    p[19] = (uint8_t)(e.permitJoining & 0x03);
    p[20] = e.depth;
    p[21] = e.lqi;
    p += kNeighborEntryLen;
  }
  return (uint8_t)needed;
}

bool ZigbeeZdo::parseMgmtLqiResponse(const uint8_t* payload, uint8_t payloadLen,
                                     ZdoMgmtLqiResponse& response) {
  response = ZdoMgmtLqiResponse();
  if (!payload || payloadLen < 5) return false;
  response.sequence = payload[0];
  response.status = payload[1];
  response.neighborTableEntries = payload[2];
  response.startIndex = payload[3];
  response.listCount = payload[4];
  if (payloadLen < (uint16_t)5 + (uint16_t)response.listCount * kNeighborEntryLen) {
    return false;
  }
  response.list = &payload[5];
  return true;
}

bool ZigbeeZdo::getNeighborListEntry(const ZdoMgmtLqiResponse& response,
                                     uint8_t index,
                                     ZdoNeighborListEntry& entry) {
  entry = ZdoNeighborListEntry();
  if (!response.list || index >= response.listCount) return false;
  const uint8_t* p = &response.list[(uint16_t)index * kNeighborEntryLen];
  entry.extendedPanId = readLe64(&p[0]);
  entry.extendedAddress = readLe64(&p[8]);
  entry.nwkAddress = readLe16(&p[16]);
  entry.deviceType = (uint8_t)(p[18] & 0x03);
  entry.rxOnWhenIdle = (uint8_t)((p[18] >> 2) & 0x03);
  entry.relationship = (uint8_t)((p[18] >> 4) & 0x07);
  entry.permitJoining = (uint8_t)(p[19] & 0x03);
  entry.depth = p[20];
  entry.lqi = p[21];
  return true;
}

uint8_t ZigbeeZdo::buildMgmtRtgResponse(uint8_t* out, uint8_t outMax,
                                        uint8_t sequence, uint8_t status,
                                        uint8_t routingTableEntries,
                                        uint8_t startIndex,
                                        const ZdoRoutingListEntry* entries,
                                        uint8_t listCount) {
  if (!out) return 0;
  if (listCount > 0 && !entries) return 0;
  uint16_t needed = (uint16_t)5 + (uint16_t)listCount * kRoutingEntryLen;
  if (outMax < needed) return 0;

  out[0] = sequence;
  out[1] = status;
  out[2] = routingTableEntries;
  out[3] = startIndex;
  out[4] = listCount;
  uint8_t* p = &out[5];
  for (uint8_t i = 0; i < listCount; ++i) {
    const ZdoRoutingListEntry& e = entries[i];
    writeLe16(&p[0], e.destinationAddress);
    p[2] = (uint8_t)((e.status & 0x07) | (e.memoryConstrained ? 0x08 : 0) |
                     (e.manyToOne ? 0x10 : 0) |
                     (e.routeRecordRequired ? 0x20 : 0));
    writeLe16(&p[3], e.nextHopAddress);
    p += kRoutingEntryLen;
  }
  return (uint8_t)needed;
}

bool ZigbeeZdo::parseMgmtRtgResponse(const uint8_t* payload, uint8_t payloadLen,
                                     ZdoMgmtRtgResponse& response) {
  response = ZdoMgmtRtgResponse();
  if (!payload || payloadLen < 5) return false;
  response.sequence = payload[0];
  response.status = payload[1];
  response.routingTableEntries = payload[2];
  response.startIndex = payload[3];
  response.listCount = payload[4];
  if (payloadLen < (uint16_t)5 + (uint16_t)response.listCount * kRoutingEntryLen) {
    return false;
  }
  response.list = &payload[5];
  return true;
}

bool ZigbeeZdo::getRoutingListEntry(const ZdoMgmtRtgResponse& response,
                                    uint8_t index, ZdoRoutingListEntry& entry) {
  entry = ZdoRoutingListEntry();
  if (!response.list || index >= response.listCount) return false;
  const uint8_t* p = &response.list[(uint16_t)index * kRoutingEntryLen];
  entry.destinationAddress = readLe16(&p[0]);
  entry.status = (uint8_t)(p[2] & 0x07);
  entry.memoryConstrained = (p[2] & 0x08) != 0;
  entry.manyToOne = (p[2] & 0x10) != 0;
  entry.routeRecordRequired = (p[2] & 0x20) != 0;
  entry.nextHopAddress = readLe16(&p[3]);
  return true;
}

}  // namespace nzb
