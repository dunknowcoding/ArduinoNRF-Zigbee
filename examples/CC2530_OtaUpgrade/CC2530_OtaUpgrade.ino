/*
  CC2530_OtaUpgrade - ZCL OTA Upgrade cluster self-test.

  Simulates the full over-the-air firmware update flow between an upgrade server
  and a client: Query Next Image, block-by-block Image Block download, and
  Upgrade End - reassembling the image on the client and checking it matches the
  server's. No radio traffic; runs on board1 via J-Link.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

// The server's firmware image.
static const uint16_t kImgSize = 128;
uint8_t serverImage[kImgSize];
OtaImageId serverImg;       // version 2 image the server hosts
uint8_t clientImage[kImgSize];

void buildServerImage() {
  for (uint16_t i = 0; i < kImgSize; ++i) serverImage[i] = (uint8_t)(i ^ 0x5A);
  serverImg.manufacturerCode = 0x1234;
  serverImg.imageType = 0x0001;
  serverImg.fileVersion = 2;
}

void testQuery() {
  Serial.println("Query Next Image:");
  // Client running version 1 asks for a newer image.
  OtaImageId cur;
  cur.manufacturerCode = 0x1234; cur.imageType = 0x0001; cur.fileVersion = 1;
  uint8_t req[16];
  uint8_t n = ZigbeeOtaCluster::buildQueryNextImageRequest(req, sizeof(req), cur);
  check(n == 9, "Query Next Image Request is 9 bytes");

  OtaImageId pcur;
  ZigbeeOtaCluster::parseQueryNextImageRequest(req, n, pcur);
  // Server: it has version 2 > the client's 1, so it offers it.
  uint8_t rsp[16];
  bool newer = serverImg.fileVersion > pcur.fileVersion;
  uint8_t rn = ZigbeeOtaCluster::buildQueryNextImageResponse(
      rsp, sizeof(rsp), newer ? OTA_STATUS_SUCCESS : OTA_STATUS_NO_IMAGE_AVAILABLE,
      serverImg, kImgSize);
  check(newer && rn == 13, "server offers the newer image (13-byte response)");

  uint8_t status = 0xFF; OtaImageId offered; uint32_t size = 0;
  check(ZigbeeOtaCluster::parseQueryNextImageResponse(rsp, rn, status, offered, size) &&
            status == OTA_STATUS_SUCCESS && offered.fileVersion == 2 &&
            size == kImgSize,
        "client sees version 2, size 128");

  // A client already at version 2 gets NO_IMAGE_AVAILABLE.
  pcur.fileVersion = 2;
  rn = ZigbeeOtaCluster::buildQueryNextImageResponse(
      rsp, sizeof(rsp),
      serverImg.fileVersion > pcur.fileVersion ? OTA_STATUS_SUCCESS
                                               : OTA_STATUS_NO_IMAGE_AVAILABLE,
      serverImg, kImgSize);
  ZigbeeOtaCluster::parseQueryNextImageResponse(rsp, rn, status, offered, size);
  check(status == OTA_STATUS_NO_IMAGE_AVAILABLE, "up-to-date client -> NO_IMAGE");
}

void testBlockTransfer() {
  Serial.println("Image Block download:");
  const uint8_t blockMax = 50;
  uint32_t offset = 0;
  uint8_t blocks = 0;
  bool ok = true;

  while (offset < kImgSize) {
    // Client requests the next block.
    uint8_t req[16];
    uint8_t rn = ZigbeeOtaCluster::buildImageBlockRequest(req, sizeof(req),
                                                          serverImg, offset,
                                                          blockMax);
    OtaImageId rimg; uint32_t roff = 0; uint8_t rmax = 0;
    if (!ZigbeeOtaCluster::parseImageBlockRequest(req, rn, rimg, roff, rmax)) { ok = false; break; }

    // Server responds with up to rmax bytes from roff.
    uint8_t dataSize = (uint8_t)((kImgSize - roff) < rmax ? (kImgSize - roff) : rmax);
    uint8_t rsp[80];
    uint8_t sn = ZigbeeOtaCluster::buildImageBlockResponse(
        rsp, sizeof(rsp), OTA_STATUS_SUCCESS, serverImg, roff,
        &serverImage[roff], dataSize);

    // Client parses + stores the block.
    uint8_t status = 0xFF; OtaImageId bimg; uint32_t boff = 0;
    const uint8_t* data = nullptr; uint8_t dsz = 0;
    if (!ZigbeeOtaCluster::parseImageBlockResponse(rsp, sn, status, bimg, boff, data, dsz) ||
        status != OTA_STATUS_SUCCESS) { ok = false; break; }
    for (uint8_t i = 0; i < dsz; ++i) clientImage[boff + i] = data[i];
    offset = boff + dsz;
    ++blocks;
  }

  check(ok && blocks == 3, "downloaded in 3 blocks (50 + 50 + 28)");
  bool match = true;
  for (uint16_t i = 0; i < kImgSize; ++i) if (clientImage[i] != serverImage[i]) match = false;
  check(match, "reassembled client image matches the server image");
}

void testUpgradeEnd() {
  Serial.println("Upgrade End:");
  uint8_t req[12];
  uint8_t n = ZigbeeOtaCluster::buildUpgradeEndRequest(req, sizeof(req),
                                                       OTA_STATUS_SUCCESS,
                                                       serverImg);
  uint8_t status = 0xFF; OtaImageId img;
  check(n == 9 && ZigbeeOtaCluster::parseUpgradeEndRequest(req, n, status, img) &&
            status == OTA_STATUS_SUCCESS && img.fileVersion == 2,
        "Upgrade End Request: success for version 2");

  uint8_t rsp[20];
  uint8_t rn = ZigbeeOtaCluster::buildUpgradeEndResponse(rsp, sizeof(rsp),
                                                         serverImg, 1000, 1000);
  check(rn == 16, "Upgrade End Response is 16 bytes (apply immediately)");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee OTA Upgrade self-test ===");

  buildServerImage();
  testQuery();
  testBlockTransfer();
  testUpgradeEnd();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
