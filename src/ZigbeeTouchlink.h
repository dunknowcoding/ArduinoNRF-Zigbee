/*
  ZigbeeTouchlink.h - Touchlink (ZLL) commissioning: inter-PAN scan / identify /
  network start-join + the ZLL network-key transport.

  Touchlink is Zigbee 3.0's proximity commissioning: an initiator broadcasts an
  inter-PAN Scan Request, nearby targets answer with a Scan Response, the
  initiator makes the chosen target Identify (blink), then sends a Network
  Start / Network Join carrying the network key encrypted under a ZLL key. It
  needs no existing network - the frames ride inter-PAN (a stub NWK/APS).

  This header builds/parses those commissioning-cluster (0x1000) command
  payloads and implements the ZLL key transport: the transport key is
  AES-ECB-Encrypt(masterKey, expanded transaction/response id), the network key
  is AES-ECB-Encrypt(transportKey, networkKey) on the initiator, and recovered
  with AES-ECB-Decrypt on the target. Encryption uses the hardware ECB
  (NrfEcb); recovery uses the software inverse cipher (ZigbeeAes128Decrypt),
  since the chip's ECB is encrypt-only.

  Note: the expanded-input byte order for key index 4 follows the common ZLL
  layout (transId, transId, responseId, responseId); confirm against a
  reference vector before interoperating with certified ZLL devices.
*/
#ifndef NIUS_ZIGBEE_TOUCHLINK_H
#define NIUS_ZIGBEE_TOUCHLINK_H

#include <Arduino.h>
#include <NrfCrypto.h>  // hardware AES-128 ECB (encrypt)

#include "ZigbeeAes128Decrypt.h"

namespace nzb {

// Touchlink commissioning cluster (0x1000) command ids.
enum ZllCommandId : uint8_t {
  ZLL_CMD_SCAN_REQUEST = 0x00,
  ZLL_CMD_SCAN_RESPONSE = 0x01,
  ZLL_CMD_DEVICE_INFO_REQUEST = 0x02,
  ZLL_CMD_DEVICE_INFO_RESPONSE = 0x03,
  ZLL_CMD_IDENTIFY_REQUEST = 0x06,
  ZLL_CMD_RESET_TO_FACTORY = 0x07,
  ZLL_CMD_NETWORK_START_REQUEST = 0x10,
  ZLL_CMD_NETWORK_START_RESPONSE = 0x11,
  ZLL_CMD_NETWORK_JOIN_ROUTER_REQUEST = 0x12,
  ZLL_CMD_NETWORK_JOIN_ROUTER_RESPONSE = 0x13,
  ZLL_CMD_NETWORK_JOIN_END_DEVICE_REQUEST = 0x14,
  ZLL_CMD_NETWORK_JOIN_END_DEVICE_RESPONSE = 0x15,
};

// ZLL key indices (which pre-shared key protects the network-key transport).
enum ZllKeyIndex : uint8_t {
  ZLL_KEY_DEVELOPMENT = 0,
  ZLL_KEY_MASTER = 4,        // certification key
  ZLL_KEY_CERTIFICATION = 15,
};

struct ZllScanResponse {
  uint32_t transactionId;
  int8_t rssiCorrection;
  uint8_t zigbeeInfo;
  uint8_t zllInfo;
  uint16_t keyBitmask;
  uint32_t responseId;
  uint64_t extendedPanId;
  uint8_t nwkUpdateId;
  uint8_t logicalChannel;
  uint16_t panId;
  uint16_t networkAddress;
  uint8_t numberSubDevices;
  uint8_t totalGroupIds;
};

struct ZllNetworkJoinRequest {
  uint32_t transactionId;
  uint8_t keyIndex;
  uint8_t encryptedNetworkKey[16];
  uint8_t nwkUpdateId;
  uint8_t logicalChannel;
  uint16_t panId;
  uint16_t networkAddress;  // address assigned to the joining device
};

class ZigbeeTouchlink {
 public:
  // --------------------------------------------------- Scan Request (0x00)
  /** transId(4) + ZigBee info(1) + ZLL info(1). */
  static uint8_t buildScanRequest(uint8_t* out, uint8_t outMax,
                                  uint32_t transactionId, uint8_t zigbeeInfo,
                                  uint8_t zllInfo) {
    if (!out || outMax < 6) return 0;
    putLe32(out, transactionId);
    out[4] = zigbeeInfo;
    out[5] = zllInfo;
    return 6;
  }
  static bool parseScanRequest(const uint8_t* p, uint8_t len,
                               uint32_t& transactionId, uint8_t& zigbeeInfo,
                               uint8_t& zllInfo) {
    if (!p || len < 6) return false;
    transactionId = getLe32(p);
    zigbeeInfo = p[4];
    zllInfo = p[5];
    return true;
  }

  // -------------------------------------------------- Scan Response (0x01)
  static uint8_t buildScanResponse(uint8_t* out, uint8_t outMax,
                                   const ZllScanResponse& r) {
    if (!out || outMax < 28) return 0;
    putLe32(&out[0], r.transactionId);
    out[4] = (uint8_t)r.rssiCorrection;
    out[5] = r.zigbeeInfo;
    out[6] = r.zllInfo;
    putLe16(&out[7], r.keyBitmask);
    putLe32(&out[9], r.responseId);
    putLe64(&out[13], r.extendedPanId);
    out[21] = r.nwkUpdateId;
    out[22] = r.logicalChannel;
    putLe16(&out[23], r.panId);
    putLe16(&out[25], r.networkAddress);
    out[27] = r.numberSubDevices;
    // totalGroupIds would follow; kept minimal here.
    return 28;
  }
  static bool parseScanResponse(const uint8_t* p, uint8_t len,
                                ZllScanResponse& r) {
    r = ZllScanResponse();
    if (!p || len < 28) return false;
    r.transactionId = getLe32(&p[0]);
    r.rssiCorrection = (int8_t)p[4];
    r.zigbeeInfo = p[5];
    r.zllInfo = p[6];
    r.keyBitmask = getLe16(&p[7]);
    r.responseId = getLe32(&p[9]);
    r.extendedPanId = getLe64(&p[13]);
    r.nwkUpdateId = p[21];
    r.logicalChannel = p[22];
    r.panId = getLe16(&p[23]);
    r.networkAddress = getLe16(&p[25]);
    r.numberSubDevices = p[27];
    return true;
  }

  // ------------------------------------------------ Identify Request (0x06)
  /** transId(4) + identify duration(2, seconds; 0=stop, 0xFFFF=default). */
  static uint8_t buildIdentifyRequest(uint8_t* out, uint8_t outMax,
                                      uint32_t transactionId,
                                      uint16_t duration) {
    if (!out || outMax < 6) return 0;
    putLe32(out, transactionId);
    putLe16(&out[4], duration);
    return 6;
  }
  static bool parseIdentifyRequest(const uint8_t* p, uint8_t len,
                                   uint32_t& transactionId, uint16_t& duration) {
    if (!p || len < 6) return false;
    transactionId = getLe32(p);
    duration = getLe16(&p[4]);
    return true;
  }

  // ---------------------------------- Network Join Router Request (0x12)
  static uint8_t buildNetworkJoinRouterRequest(uint8_t* out, uint8_t outMax,
                                               const ZllNetworkJoinRequest& j) {
    if (!out || outMax < 27) return 0;
    putLe32(&out[0], j.transactionId);
    out[4] = j.keyIndex;
    memcpy(&out[5], j.encryptedNetworkKey, 16);
    out[21] = j.nwkUpdateId;
    out[22] = j.logicalChannel;
    putLe16(&out[23], j.panId);
    putLe16(&out[25], j.networkAddress);
    return 27;
  }
  static bool parseNetworkJoinRouterRequest(const uint8_t* p, uint8_t len,
                                            ZllNetworkJoinRequest& j) {
    j = ZllNetworkJoinRequest();
    if (!p || len < 27) return false;
    j.transactionId = getLe32(&p[0]);
    j.keyIndex = p[4];
    memcpy(j.encryptedNetworkKey, &p[5], 16);
    j.nwkUpdateId = p[21];
    j.logicalChannel = p[22];
    j.panId = getLe16(&p[23]);
    j.networkAddress = getLe16(&p[25]);
    return true;
  }

  // ------------------------------------------------- ZLL key transport
  /** Transport key = AES-ECB-Encrypt(masterKey, expandedInput), where the
      expanded input for key index 4 is (transId, transId, responseId,
      responseId), each 4 bytes big-endian. */
  static bool deriveTransportKey(const uint8_t masterKey[16],
                                 uint32_t transactionId, uint32_t responseId,
                                 uint8_t outKey[16]) {
    uint8_t expanded[16];
    putBe32(&expanded[0], transactionId);
    putBe32(&expanded[4], transactionId);
    putBe32(&expanded[8], responseId);
    putBe32(&expanded[12], responseId);
    return NrfEcb::encrypt(masterKey, expanded, outKey);
  }

  /** Encrypt the network key for transport: out = AES-ECB-Encrypt(TK, key). */
  static bool encryptNetworkKey(const uint8_t masterKey[16],
                                uint32_t transactionId, uint32_t responseId,
                                const uint8_t networkKey[16],
                                uint8_t outEncrypted[16]) {
    uint8_t tk[16];
    if (!deriveTransportKey(masterKey, transactionId, responseId, tk)) return false;
    return NrfEcb::encrypt(tk, networkKey, outEncrypted);
  }

  /** Recover the network key: out = AES-ECB-Decrypt(TK, encrypted). Uses the
      software inverse cipher (the hardware ECB is encrypt-only). */
  static bool decryptNetworkKey(const uint8_t masterKey[16],
                                uint32_t transactionId, uint32_t responseId,
                                const uint8_t encrypted[16],
                                uint8_t outNetworkKey[16]) {
    uint8_t tk[16];
    if (!deriveTransportKey(masterKey, transactionId, responseId, tk)) return false;
    ZigbeeAes128Decrypt::decryptBlock(tk, encrypted, outNetworkKey);
    return true;
  }

 private:
  static void putLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8);
  }
  static uint16_t getLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  }
  static void putLe32(uint8_t* p, uint32_t v) {
    for (uint8_t i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
  }
  static uint32_t getLe32(const uint8_t* p) {
    uint32_t v = 0;
    for (uint8_t i = 0; i < 4; ++i) v |= (uint32_t)p[i] << (8 * i);
    return v;
  }
  static void putBe32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
  }
  static void putLe64(uint8_t* p, uint64_t v) {
    for (uint8_t i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (8 * i));
  }
  static uint64_t getLe64(const uint8_t* p) {
    uint64_t v = 0;
    for (uint8_t i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (8 * i);
    return v;
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_TOUCHLINK_H
