/*
  CC2530_ZdoDiscovery - two-node Zigbee Device Object discovery demo.

  Flash this sketch to two ArduinoNRF+CC2530 setups. Build one with
  NIUS_ZIGBEE_THIS_NODE=0x0001 and the other with 0x0002. Each node programs
  the CC2530 MAC filter, then exchanges ZDO address, active endpoint, simple
  descriptor, and match descriptor requests on endpoint 0/profile 0.
*/

#include <CC2530Radio.h>

#ifndef NIUS_ZIGBEE_PAN_ID
#define NIUS_ZIGBEE_PAN_ID 0x1A62
#endif

#ifndef NIUS_ZIGBEE_THIS_NODE
#define NIUS_ZIGBEE_THIS_NODE 0x0001
#endif

static const uint16_t PAN_ID = NIUS_ZIGBEE_PAN_ID;
static const uint16_t THIS_NODE = NIUS_ZIGBEE_THIS_NODE;
static const uint16_t PEER_NODE = (THIS_NODE == 0x0001) ? 0x0002 : 0x0001;
static const uint64_t THIS_IEEE = 0x1A62195E00000000ULL | THIS_NODE;
static const uint64_t PEER_IEEE = 0x1A62195E00000000ULL | PEER_NODE;
static const uint8_t ENDPOINT = 1;
static const uint16_t INPUT_CLUSTERS[] = {
  nzb::ZigbeeZcl::kClusterBasic,
  nzb::ZigbeeZcl::kClusterOnOff
};
static const nzb::ZigbeeEndpointDescriptor ENDPOINTS[] = {
  {
    ENDPOINT,
    nzb::ZigbeeAps::kProfileHomeAutomation,
    0x0100,  // On/Off Light
    1,
    INPUT_CLUSTERS,
    (uint8_t)(sizeof(INPUT_CLUSTERS) / sizeof(INPUT_CLUSTERS[0])),
    nullptr,
    0
  }
};

CC2530Radio radio;
nzb::ZigbeeDeviceObject device(THIS_IEEE, THIS_NODE, ENDPOINTS, 1);
uint8_t zdoSequence = 0;
uint32_t nextAction = 0;
uint8_t actionStep = 0;

void printHex8(uint8_t v) {
  if (v < 0x10) Serial.print('0');
  Serial.print(v, HEX);
}

void printHex16(uint16_t v) {
  if (v < 0x1000) Serial.print('0');
  if (v < 0x0100) Serial.print('0');
  if (v < 0x0010) Serial.print('0');
  Serial.print(v, HEX);
}

void printHex64(uint64_t v) {
  for (int i = 7; i >= 0; --i) {
    printHex8((uint8_t)(v >> (i * 8)));
  }
}

bool sendZdo(uint16_t dstShort, uint16_t clusterId,
             const uint8_t* payload, uint8_t payloadLen) {
  return radio.sendZdoCommand(PAN_ID, dstShort, THIS_NODE, dstShort, THIS_NODE,
                              clusterId, payload, payloadLen,
                              nzb::ZigbeeNwk::kDefaultRadius, true);
}

void handleZdoRequest(const nzb::NwkDataFrame& nwk, const nzb::ApsDataFrame& aps) {
  uint8_t payload[nzb::ZigbeeZdo::kMaxPayload];
  uint16_t responseCluster = 0;
  uint8_t n = device.handleRequest(aps.clusterId, aps.payload, aps.payloadLen,
                                   payload, sizeof(payload), responseCluster);
  if (n == 0) return;
  bool ok = sendZdo(nwk.srcShort, responseCluster, payload, n);
  Serial.println(ok ? "  ZDO response sent" : "  ZDO response FAILED");
}

void printAddressResponse(const nzb::ApsDataFrame& aps) {
  nzb::ZdoAddressResponse rsp;
  if (!nzb::ZigbeeZdo::parseAddressResponse(aps.payload, aps.payloadLen, rsp)) return;
  Serial.print("  status=0x");
  printHex8(rsp.status);
  Serial.print(" ieee=0x");
  printHex64(rsp.ieeeAddress);
  Serial.print(" nwk=0x");
  printHex16(rsp.nwkAddress);
  Serial.println();
}

void printActiveEndpointResponse(const nzb::ApsDataFrame& aps) {
  nzb::ZdoActiveEndpointResponse rsp;
  if (!nzb::ZigbeeZdo::parseActiveEndpointResponse(aps.payload, aps.payloadLen, rsp)) return;
  Serial.print("  status=0x");
  printHex8(rsp.status);
  Serial.print(" endpoints=");
  for (uint8_t i = 0; i < rsp.endpointCount; ++i) {
    if (i) Serial.print(',');
    Serial.print(rsp.endpoints[i]);
  }
  Serial.println();
}

void printSimpleDescriptorResponse(const nzb::ApsDataFrame& aps) {
  nzb::ZdoSimpleDescriptorResponse rsp;
  if (!nzb::ZigbeeZdo::parseSimpleDescriptorResponse(aps.payload, aps.payloadLen, rsp)) return;
  Serial.print("  status=0x");
  printHex8(rsp.status);
  Serial.print(" endpoint=");
  Serial.print(rsp.descriptor.endpoint);
  Serial.print(" profile=0x");
  printHex16(rsp.descriptor.profileId);
  Serial.print(" device=0x");
  printHex16(rsp.descriptor.deviceId);
  Serial.print(" inputs=");
  for (uint8_t i = 0; i < rsp.descriptor.inputClusterCount; ++i) {
    if (i) Serial.print(',');
    Serial.print("0x");
    printHex16(rsp.descriptor.inputClusters[i]);
  }
  Serial.println();
}

void printMatchDescriptorResponse(const nzb::ApsDataFrame& aps) {
  nzb::ZdoMatchDescriptorResponse rsp;
  if (!nzb::ZigbeeZdo::parseMatchDescriptorResponse(aps.payload, aps.payloadLen, rsp)) return;
  Serial.print("  status=0x");
  printHex8(rsp.status);
  Serial.print(" matches=");
  for (uint8_t i = 0; i < rsp.endpointCount; ++i) {
    if (i) Serial.print(',');
    Serial.print(rsp.endpoints[i]);
  }
  Serial.println();
}

void onZdoFrame(const nzb::MacDataFrame& mac, const nzb::NwkDataFrame& nwk,
                const nzb::ApsDataFrame& aps, int8_t rssi, uint8_t lqi) {
  if (mac.dstPanId != PAN_ID) return;
  if (nwk.dstShort != THIS_NODE) return;

  Serial.print("ZDO RX src=0x");
  printHex16(nwk.srcShort);
  Serial.print(" cluster=0x");
  printHex16(aps.clusterId);
  Serial.print(" rssi=");
  Serial.print(rssi);
  Serial.print("dBm lqi=");
  Serial.println(lqi & 0x7F);

  if (aps.clusterId < 0x8000) {
    handleZdoRequest(nwk, aps);
  } else if (aps.clusterId == nzb::ZDO_IEEE_ADDR_RSP ||
             aps.clusterId == nzb::ZDO_NWK_ADDR_RSP) {
    printAddressResponse(aps);
  } else if (aps.clusterId == nzb::ZDO_ACTIVE_EP_RSP) {
    printActiveEndpointResponse(aps);
  } else if (aps.clusterId == nzb::ZDO_SIMPLE_DESC_RSP) {
    printSimpleDescriptorResponse(aps);
  } else if (aps.clusterId == nzb::ZDO_MATCH_DESC_RSP) {
    printMatchDescriptorResponse(aps);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  if (!radio.begin(15)) {
    Serial.println("CC2530 not found - check wiring/firmware.");
    while (true) delay(1000);
  }

  uint8_t ieeeBytes[8];
  for (uint8_t i = 0; i < 8; ++i) ieeeBytes[i] = (uint8_t)(THIS_IEEE >> (8 * i));
  bool ok = radio.setAddress(PAN_ID, THIS_NODE, ieeeBytes);
  ok = ok && radio.configureMac(
      CC2530Radio::kMacFilter | CC2530Radio::kMacAutoAck | CC2530Radio::kMacCcaTx,
      2);
  radio.onZdoReceive(onZdoFrame);

  Serial.print("ZDO discovery node up. PAN=0x");
  printHex16(PAN_ID);
  Serial.print(" this=0x");
  printHex16(THIS_NODE);
  Serial.print(" peer=0x");
  printHex16(PEER_NODE);
  Serial.print(" ieee=0x");
  printHex64(THIS_IEEE);
  Serial.println(ok ? " mac=filtered" : " mac=configure FAILED");

  nextAction = millis() + ((THIS_NODE == 0x0001) ? 1000 : 2500);
}

void loop() {
  radio.poll();

  if ((int32_t)(millis() - nextAction) < 0) return;
  nextAction += 5000;

  uint8_t payload[nzb::ZigbeeZdo::kMaxPayload];
  uint8_t n = 0;
  uint16_t cluster = 0;
  switch (actionStep++ % 5) {
    case 0:
      n = nzb::ZigbeeZdo::buildIeeeAddressRequest(
          payload, sizeof(payload), zdoSequence++, PEER_NODE);
      cluster = nzb::ZDO_IEEE_ADDR_REQ;
      Serial.print("ZDO TX IEEE_addr_req ");
      break;
    case 1:
      n = nzb::ZigbeeZdo::buildNwkAddressRequest(
          payload, sizeof(payload), zdoSequence++, PEER_IEEE);
      cluster = nzb::ZDO_NWK_ADDR_REQ;
      Serial.print("ZDO TX NWK_addr_req ");
      break;
    case 2:
      n = nzb::ZigbeeZdo::buildActiveEndpointRequest(
          payload, sizeof(payload), zdoSequence++, PEER_NODE);
      cluster = nzb::ZDO_ACTIVE_EP_REQ;
      Serial.print("ZDO TX Active_EP_req ");
      break;
    case 3:
      n = nzb::ZigbeeZdo::buildSimpleDescriptorRequest(
          payload, sizeof(payload), zdoSequence++, PEER_NODE, ENDPOINT);
      cluster = nzb::ZDO_SIMPLE_DESC_REQ;
      Serial.print("ZDO TX Simple_Desc_req ");
      break;
    default:
      n = nzb::ZigbeeZdo::buildMatchDescriptorRequest(
          payload, sizeof(payload), zdoSequence++, PEER_NODE,
          nzb::ZigbeeAps::kProfileHomeAutomation,
          &INPUT_CLUSTERS[1], 1, nullptr, 0);
      cluster = nzb::ZDO_MATCH_DESC_REQ;
      Serial.print("ZDO TX Match_Desc_req ");
      break;
  }

  bool ok = (n > 0) && sendZdo(PEER_NODE, cluster, payload, n);
  Serial.println(ok ? "ok" : "FAILED");
}
