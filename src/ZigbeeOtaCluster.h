/*
  ZigbeeOtaCluster.h - ZCL OTA Upgrade cluster (0x0019): firmware update.

  Over-the-air upgrade is how a Zigbee device pulls a new firmware image from an
  upgrade server. The client polls the server (Query Next Image), the server
  offers an image (version + size), the client downloads it block by block
  (Image Block Request/Response), then signals it is done (Upgrade End). This
  header builds/parses those command payloads so a node can host or receive an
  OTA image; the block loop drives the transfer.

  Multi-byte fields are little-endian. The OTA file is identified by
  (manufacturer code, image type, file version).
*/
#ifndef NIUS_ZIGBEE_OTA_CLUSTER_H
#define NIUS_ZIGBEE_OTA_CLUSTER_H

#include <Arduino.h>

namespace nzb {

enum ZclOtaCommandId : uint8_t {
  OTA_CMD_IMAGE_NOTIFY = 0x00,            // server -> client
  OTA_CMD_QUERY_NEXT_IMAGE_REQUEST = 0x01,
  OTA_CMD_QUERY_NEXT_IMAGE_RESPONSE = 0x02,
  OTA_CMD_IMAGE_BLOCK_REQUEST = 0x03,
  OTA_CMD_IMAGE_BLOCK_RESPONSE = 0x05,
  OTA_CMD_UPGRADE_END_REQUEST = 0x06,
  OTA_CMD_UPGRADE_END_RESPONSE = 0x07,
};

enum ZclOtaStatus : uint8_t {
  OTA_STATUS_SUCCESS = 0x00,
  OTA_STATUS_ABORT = 0x95,
  OTA_STATUS_NOT_AUTHORIZED = 0x7E,
  OTA_STATUS_NO_IMAGE_AVAILABLE = 0x98,
  OTA_STATUS_MALFORMED_COMMAND = 0x80,
};

struct OtaImageId {
  uint16_t manufacturerCode;
  uint16_t imageType;
  uint32_t fileVersion;
  OtaImageId() : manufacturerCode(0), imageType(0), fileVersion(0) {}
};

class ZigbeeOtaCluster {
 public:
  // ---- Query Next Image (client asks "is there a newer image for me?") ----
  /** field control(1)=0 + manufacturer(2) + image type(2) + current version(4). */
  static uint8_t buildQueryNextImageRequest(uint8_t* out, uint8_t outMax,
                                            const OtaImageId& cur) {
    if (!out || outMax < 9) return 0;
    out[0] = 0;  // field control
    putLe16(&out[1], cur.manufacturerCode);
    putLe16(&out[3], cur.imageType);
    putLe32(&out[5], cur.fileVersion);
    return 9;
  }
  static bool parseQueryNextImageRequest(const uint8_t* p, uint8_t len,
                                         OtaImageId& cur) {
    cur = OtaImageId();
    if (!p || len < 9) return false;
    cur.manufacturerCode = getLe16(&p[1]);
    cur.imageType = getLe16(&p[3]);
    cur.fileVersion = getLe32(&p[5]);
    return true;
  }

  /** status(1); on SUCCESS also manufacturer(2)+image type(2)+version(4)+size(4). */
  static uint8_t buildQueryNextImageResponse(uint8_t* out, uint8_t outMax,
                                             uint8_t status,
                                             const OtaImageId& img,
                                             uint32_t imageSize) {
    if (!out) return 0;
    if (status != OTA_STATUS_SUCCESS) {
      if (outMax < 1) return 0;
      out[0] = status;
      return 1;
    }
    if (outMax < 13) return 0;
    out[0] = status;
    putLe16(&out[1], img.manufacturerCode);
    putLe16(&out[3], img.imageType);
    putLe32(&out[5], img.fileVersion);
    putLe32(&out[9], imageSize);
    return 13;
  }
  static bool parseQueryNextImageResponse(const uint8_t* p, uint8_t len,
                                          uint8_t& status, OtaImageId& img,
                                          uint32_t& imageSize) {
    img = OtaImageId();
    imageSize = 0;
    if (!p || len < 1) return false;
    status = p[0];
    if (status != OTA_STATUS_SUCCESS) return true;  // no image fields
    if (len < 13) return false;
    img.manufacturerCode = getLe16(&p[1]);
    img.imageType = getLe16(&p[3]);
    img.fileVersion = getLe32(&p[5]);
    imageSize = getLe32(&p[9]);
    return true;
  }

  // ---- Image Block (client downloads the image piece by piece) ----
  /** field control(1)=0 + manufacturer(2) + image type(2) + version(4) +
      file offset(4) + max data size(1) = 14. */
  static uint8_t buildImageBlockRequest(uint8_t* out, uint8_t outMax,
                                        const OtaImageId& img,
                                        uint32_t fileOffset,
                                        uint8_t maxDataSize) {
    if (!out || outMax < 14) return 0;
    out[0] = 0;
    putLe16(&out[1], img.manufacturerCode);
    putLe16(&out[3], img.imageType);
    putLe32(&out[5], img.fileVersion);
    putLe32(&out[9], fileOffset);
    out[13] = maxDataSize;
    return 14;
  }
  static bool parseImageBlockRequest(const uint8_t* p, uint8_t len,
                                     OtaImageId& img, uint32_t& fileOffset,
                                     uint8_t& maxDataSize) {
    img = OtaImageId();
    if (!p || len < 14) return false;
    img.manufacturerCode = getLe16(&p[1]);
    img.imageType = getLe16(&p[3]);
    img.fileVersion = getLe32(&p[5]);
    fileOffset = getLe32(&p[9]);
    maxDataSize = p[13];
    return true;
  }

  /** status(1); on SUCCESS manufacturer(2)+image type(2)+version(4)+offset(4)+
      data size(1)+data. */
  static uint8_t buildImageBlockResponse(uint8_t* out, uint8_t outMax,
                                         uint8_t status, const OtaImageId& img,
                                         uint32_t fileOffset,
                                         const uint8_t* data, uint8_t dataSize) {
    if (!out) return 0;
    if (status != OTA_STATUS_SUCCESS) {
      if (outMax < 1) return 0;
      out[0] = status;
      return 1;
    }
    uint8_t need = (uint8_t)(14 + dataSize);
    if (outMax < need || (dataSize > 0 && !data)) return 0;
    out[0] = status;
    putLe16(&out[1], img.manufacturerCode);
    putLe16(&out[3], img.imageType);
    putLe32(&out[5], img.fileVersion);
    putLe32(&out[9], fileOffset);
    out[13] = dataSize;
    for (uint8_t i = 0; i < dataSize; ++i) out[14 + i] = data[i];
    return need;
  }
  static bool parseImageBlockResponse(const uint8_t* p, uint8_t len,
                                      uint8_t& status, OtaImageId& img,
                                      uint32_t& fileOffset, const uint8_t*& data,
                                      uint8_t& dataSize) {
    img = OtaImageId();
    data = nullptr;
    dataSize = 0;
    if (!p || len < 1) return false;
    status = p[0];
    if (status != OTA_STATUS_SUCCESS) return true;
    if (len < 14) return false;
    img.manufacturerCode = getLe16(&p[1]);
    img.imageType = getLe16(&p[3]);
    img.fileVersion = getLe32(&p[5]);
    fileOffset = getLe32(&p[9]);
    dataSize = p[13];
    if (len < (uint8_t)(14 + dataSize)) return false;
    data = &p[14];
    return true;
  }

  // ---- Upgrade End (client signals the download finished) ----
  /** status(1) + manufacturer(2) + image type(2) + version(4). */
  static uint8_t buildUpgradeEndRequest(uint8_t* out, uint8_t outMax,
                                        uint8_t status, const OtaImageId& img) {
    if (!out || outMax < 9) return 0;
    out[0] = status;
    putLe16(&out[1], img.manufacturerCode);
    putLe16(&out[3], img.imageType);
    putLe32(&out[5], img.fileVersion);
    return 9;
  }
  static bool parseUpgradeEndRequest(const uint8_t* p, uint8_t len,
                                     uint8_t& status, OtaImageId& img) {
    img = OtaImageId();
    if (!p || len < 9) return false;
    status = p[0];
    img.manufacturerCode = getLe16(&p[1]);
    img.imageType = getLe16(&p[3]);
    img.fileVersion = getLe32(&p[5]);
    return true;
  }
  /** manufacturer(2)+image type(2)+version(4)+current time(4)+upgrade time(4). */
  static uint8_t buildUpgradeEndResponse(uint8_t* out, uint8_t outMax,
                                         const OtaImageId& img,
                                         uint32_t currentTime,
                                         uint32_t upgradeTime) {
    if (!out || outMax < 16) return 0;
    putLe16(&out[0], img.manufacturerCode);
    putLe16(&out[2], img.imageType);
    putLe32(&out[4], img.fileVersion);
    putLe32(&out[8], currentTime);
    putLe32(&out[12], upgradeTime);
    return 16;
  }

 private:
  static void putLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
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
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_OTA_CLUSTER_H
