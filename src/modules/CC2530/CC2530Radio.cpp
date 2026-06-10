#include "CC2530Radio.h"

namespace nzb {

// Framed UART protocol (must match extras/firmware/cc2530/cc2530_radio.c):
//   SOF 0xFE | LEN | CMD | DATA[LEN-1] | FCS   (FCS = XOR of LEN..last DATA)
namespace {
const uint8_t SOF = 0xFE;
// host -> module
const uint8_t CMD_PING = 0x01;
const uint8_t CMD_SET_CHANNEL = 0x02;
const uint8_t CMD_TX = 0x03;
const uint8_t CMD_SET_PROMISC = 0x04;
// module -> host
const uint8_t RSP_RESET_IND = 0x80;
const uint8_t RSP_PONG = 0x81;
const uint8_t RSP_OK = 0x82;
const uint8_t RSP_TXSTAT = 0x83;
const uint8_t RSP_RX_FRAME = 0x84;
// The CC2530 appends RSSI with this offset (datasheet): dBm = raw - 73.
const int16_t RSSI_OFFSET = 73;
}  // namespace

CC2530Radio::CC2530Radio(HardwareSerial& serial)
    : serial_(&serial), rxCb_(nullptr), dataCb_(nullptr), nwkCb_(nullptr),
      apsCb_(nullptr), zclCb_(nullptr), version_(0), channel_(11),
      macSequence_(0), nwkSequence_(0), apsCounter_(0), zclSequence_(0),
      state_(0), len_(0), idx_(0), fcs_(0),
      respCmd_(0), respLen_(0), respReady_(false) {}

void CC2530Radio::sendFrame(uint8_t cmd, const uint8_t* data, uint8_t n) {
  uint8_t fcs = (uint8_t)(n + 1) ^ cmd;
  serial_->write(SOF);
  serial_->write((uint8_t)(n + 1));
  serial_->write(cmd);
  for (uint8_t i = 0; i < n; ++i) {
    serial_->write(data[i]);
    fcs ^= data[i];
  }
  serial_->write(fcs);
  serial_->flush();
}

// Parse one received byte. On a complete, FCS-valid frame: deliver 0x84 to the
// RX callback, or stash any other frame as the pending command response.
void CC2530Radio::feed(uint8_t b) {
  switch (state_) {
    case 0:
      if (b == SOF) state_ = 1;
      break;
    case 1:
      len_ = b; idx_ = 0; fcs_ = b;
      if (len_ == 0 || len_ > sizeof(buf_)) state_ = 0; else state_ = 2;
      break;
    case 2:
      buf_[idx_++] = b; fcs_ ^= b;
      if (idx_ >= len_) state_ = 3;
      break;
    case 3: {  // b = received FCS
      state_ = 0;
      if (b != fcs_) break;            // bad checksum -> drop
      uint8_t cmd = buf_[0];
      if (cmd == RSP_RX_FRAME) {
        // DATA = [rssi][lqi][psdu...]
        if (len_ >= 3) {
          int8_t rssi = (int8_t)((int16_t)((int8_t)buf_[1]) - RSSI_OFFSET);
          uint8_t lqi = buf_[2];
          const uint8_t* psdu = &buf_[3];
          uint8_t psduLen = (uint8_t)(len_ - 3);
          if (rxCb_) {
            rxCb_(psdu, psduLen, rssi, lqi);
          }
          if (dataCb_ || nwkCb_ || apsCb_ || zclCb_) {
            MacDataFrame frame;
            if (ZigbeeMac::parseShortDataFrame(psdu, psduLen, frame)) {
              if (dataCb_) {
                dataCb_(frame, rssi, lqi);
              }
              if (nwkCb_ || apsCb_ || zclCb_) {
                NwkDataFrame nwk;
                if (ZigbeeNwk::parseDataFrame(frame.payload, frame.payloadLen, nwk)) {
                  if (nwkCb_) {
                    nwkCb_(frame, nwk, rssi, lqi);
                  }
                  if (apsCb_ || zclCb_) {
                    ApsDataFrame aps;
                    if (ZigbeeAps::parseDataFrame(nwk.payload, nwk.payloadLen, aps)) {
                      if (apsCb_) {
                        apsCb_(frame, nwk, aps, rssi, lqi);
                      }
                      if (zclCb_) {
                        ZclFrame zcl;
                        if (ZigbeeZcl::parseFrame(aps.payload, aps.payloadLen, zcl)) {
                          zclCb_(frame, nwk, aps, zcl, rssi, lqi);
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      } else if (cmd == RSP_RESET_IND || cmd == RSP_PONG) {
        if (len_ >= 3) version_ = ((uint16_t)buf_[1] << 8) | buf_[2];
        respCmd_ = cmd; respLen_ = (uint8_t)(len_ - 1);
        for (uint8_t i = 0; i < respLen_ && i < sizeof(respData_); ++i)
          respData_[i] = buf_[1 + i];
        respReady_ = true;
      } else {                         // OK / TXSTAT
        respCmd_ = cmd; respLen_ = (uint8_t)(len_ - 1);
        for (uint8_t i = 0; i < respLen_ && i < sizeof(respData_); ++i)
          respData_[i] = buf_[1 + i];
        respReady_ = true;
      }
      break;
    }
  }
}

bool CC2530Radio::waitResp(uint8_t cmd, uint32_t timeoutMs) {
  respReady_ = false;
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (serial_->available()) {
      feed((uint8_t)serial_->read());
      if (respReady_ && respCmd_ == cmd) { respReady_ = false; return true; }
    }
    yield();
  }
  return false;
}

bool CC2530Radio::begin(uint8_t channel, uint32_t baud) {
  serial_->begin(baud);
  delay(50);
  while (serial_->available()) serial_->read();  // drain boot announce/noise

  // The nRF host may reset/re-enumerate while the CC2530 keeps running. During
  // that window the UART line can leave the CC2530 firmware's simple frame
  // parser mid-packet. Send enough zero bytes to finish or reject any partial
  // frame before the first real command.
  for (uint8_t i = 0; i < 140; ++i) serial_->write((uint8_t)0x00);
  serial_->flush();
  delay(5);
  while (serial_->available()) serial_->read();

  bool alive = false;
  for (uint8_t attempt = 0; attempt < 3 && !alive; ++attempt) {
    alive = ping();
    if (!alive) delay(20);
  }
  if (!alive) return false;
  setPromiscuous(true);
  return setChannel(channel);
}

bool CC2530Radio::ping() {
  sendFrame(CMD_PING, nullptr, 0);
  return waitResp(RSP_PONG, 300);
}

bool CC2530Radio::setChannel(uint8_t ch) {
  if (ch < 11) ch = 11;
  if (ch > 26) ch = 26;
  sendFrame(CMD_SET_CHANNEL, &ch, 1);
  if (!waitResp(RSP_OK, 300)) return false;
  channel_ = ch;
  return true;
}

bool CC2530Radio::setPromiscuous(bool on) {
  // The firmware writes this value directly to FRMFILT0 bit 0:
  // 0 = frame filter disabled (promiscuous), 1 = frame filter enabled.
  uint8_t v = on ? 0 : 1;
  sendFrame(CMD_SET_PROMISC, &v, 1);
  return waitResp(RSP_OK, 300);
}

bool CC2530Radio::send(const uint8_t* payload, uint8_t len) {
  if (len > kMaxPayload) return false;
  sendFrame(CMD_TX, payload, len);
  if (!waitResp(RSP_TXSTAT, 500)) return false;
  return respLen_ >= 1 && respData_[0] == 0;  // 0 = TXDONE, 1 = fail
}

bool CC2530Radio::sendData(uint16_t panId, uint16_t dstShort, uint16_t srcShort,
                           const uint8_t* payload, uint8_t len,
                           bool ackRequest) {
  uint8_t psdu[kMaxPayload];
  uint8_t psduLen = ZigbeeMac::buildShortDataFrame(
      psdu, sizeof(psdu), panId, dstShort, srcShort, macSequence_++,
      payload, len, ackRequest);
  if (psduLen == 0) return false;
  return send(psdu, psduLen);
}

bool CC2530Radio::sendNwkData(uint16_t panId, uint16_t macDstShort,
                              uint16_t macSrcShort, uint16_t nwkDstShort,
                              uint16_t nwkSrcShort, const uint8_t* payload,
                              uint8_t len, uint8_t radius,
                              bool ackRequest) {
  uint8_t npdu[ZigbeeNwk::kMaxFrame];
  uint8_t npduLen = ZigbeeNwk::buildDataFrame(
      npdu, sizeof(npdu), nwkDstShort, nwkSrcShort, radius, nwkSequence_++,
      payload, len);
  if (npduLen == 0) return false;
  return sendData(panId, macDstShort, macSrcShort, npdu, npduLen, ackRequest);
}

bool CC2530Radio::sendApsData(uint16_t panId, uint16_t macDstShort,
                              uint16_t macSrcShort, uint16_t nwkDstShort,
                              uint16_t nwkSrcShort, uint8_t dstEndpoint,
                              uint16_t clusterId, uint16_t profileId,
                              uint8_t srcEndpoint, const uint8_t* payload,
                              uint8_t len, uint8_t radius,
                              bool ackRequest) {
  uint8_t apdu[ZigbeeAps::kMaxFrame];
  uint8_t apduLen = ZigbeeAps::buildDataFrame(
      apdu, sizeof(apdu), dstEndpoint, clusterId, profileId, srcEndpoint,
      apsCounter_++, payload, len);
  if (apduLen == 0) return false;
  return sendNwkData(panId, macDstShort, macSrcShort, nwkDstShort, nwkSrcShort,
                     apdu, apduLen, radius, ackRequest);
}

bool CC2530Radio::sendZclCommand(uint16_t panId, uint16_t macDstShort,
                                 uint16_t macSrcShort, uint16_t nwkDstShort,
                                 uint16_t nwkSrcShort, uint8_t dstEndpoint,
                                 uint16_t clusterId, uint16_t profileId,
                                 uint8_t srcEndpoint, uint8_t commandId,
                                 const uint8_t* payload, uint8_t len,
                                 uint8_t zclFrameType,
                                 uint8_t zclDirection, uint8_t radius,
                                 bool ackRequest) {
  uint8_t zcl[ZigbeeZcl::kMaxFrame];
  uint8_t zclLen = ZigbeeZcl::buildCommandFrame(
      zcl, sizeof(zcl), zclFrameType, zclSequence_++, commandId, payload, len,
      zclDirection);
  if (zclLen == 0) return false;
  return sendApsData(panId, macDstShort, macSrcShort, nwkDstShort, nwkSrcShort,
                     dstEndpoint, clusterId, profileId, srcEndpoint, zcl,
                     zclLen, radius, ackRequest);
}

void CC2530Radio::poll() {
  while (serial_->available()) feed((uint8_t)serial_->read());
}

}  // namespace nzb
