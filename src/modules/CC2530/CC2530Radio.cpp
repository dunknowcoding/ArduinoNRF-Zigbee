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
const uint8_t CMD_SET_ADDR = 0x05;
const uint8_t CMD_SET_MAC = 0x06;
const uint8_t CMD_GET_MAC = 0x07;
const uint8_t CMD_TX_ADV = 0x08;
const uint8_t CMD_SET_TX_POWER = 0x09;
// module -> host
const uint8_t RSP_RESET_IND = 0x80;
const uint8_t RSP_PONG = 0x81;
const uint8_t RSP_OK = 0x82;
const uint8_t RSP_TXSTAT = 0x83;
const uint8_t RSP_RX_FRAME = 0x84;
const uint8_t RSP_MAC_INFO = 0x85;
// The CC2530 appends RSSI with this offset (datasheet): dBm = raw - 73.
const int16_t RSSI_OFFSET = 73;
}  // namespace

CC2530Radio::CC2530Radio(HardwareSerial& serial)
    : serial_(&serial), rxCb_(nullptr), dataCb_(nullptr),
      macCommandCb_(nullptr), beaconCb_(nullptr), security_(nullptr),
      securityIeee_(0), securityCounter_(0), nwkCb_(nullptr),
      nwkCommandCb_(nullptr), apsCb_(nullptr), zdoCb_(nullptr), zclCb_(nullptr),
      apsAckCb_(nullptr),
      version_(0), channel_(11),
      macSequence_(0), nwkSequence_(0), apsCounter_(0), zclSequence_(0),
      lastTxAttempts_(0),
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
          if (dataCb_ || macCommandCb_ || beaconCb_ || nwkCb_ ||
              nwkCommandCb_ || apsCb_ || zdoCb_ || zclCb_) {
            MacDataFrame frame;
            if (ZigbeeMac::parseShortDataFrame(psdu, psduLen, frame)) {
              if (dataCb_) {
                dataCb_(frame, rssi, lqi);
              }

              // NWK security: verify + decrypt before any NWK parsing. A
              // secured frame that fails the MIC or replay check is dropped
              // for the NWK-and-above callbacks (raw callbacks already ran).
              const uint8_t* npdu = frame.payload;
              uint8_t npduLen = frame.payloadLen;
              bool nwkDrop = false;
              // Skip decryption while promiscuous (active scan): the frame
              // filter is off so we hear other networks' secured frames, and
              // the CC2530 keeps the FCS in the PSDU in this mode, which would
              // misplace the trailing MIC and inflate the failure count. Real
              // decryption resumes once we join and re-enable the filter.
              if (security_ && security_->hasKey() && !promiscuous_ &&
                  npduLen >= 8 && (npdu[1] & 0x02) != 0) {
                uint8_t headerLen = nwkHeaderLength(npdu, npduLen);
                uint8_t n = security_->openNpdu(npdu, npduLen, headerLen,
                                                securedScratch_,
                                                sizeof(securedScratch_));
                if (n == 0) {
                  nwkDrop = true;
                } else {
                  npdu = securedScratch_;
                  npduLen = n;
                }
              }

              if (!nwkDrop && (nwkCb_ || apsCb_ || zdoCb_ || zclCb_ || apsAckCb_)) {
                NwkDataFrame nwk;
                if (ZigbeeNwk::parseDataFrame(npdu, npduLen, nwk)) {
                  if (nwkCb_) {
                    nwkCb_(frame, nwk, rssi, lqi);
                  }
                  // APS ACK frames have their own frame type; branch before
                  // the data-frame parse (which rejects non-data types).
                  if (apsAckCb_ &&
                      ZigbeeAps::frameType(nwk.payload, nwk.payloadLen) ==
                          APS_FRAME_ACK) {
                    ApsAckFrame ack;
                    if (ZigbeeAps::parseAckFrame(nwk.payload, nwk.payloadLen,
                                                 ack)) {
                      apsAckCb_(frame, nwk, ack, rssi, lqi);
                    }
                  }
                  if (apsCb_ || zdoCb_ || zclCb_) {
                    ApsDataFrame aps;
                    if (ZigbeeAps::parseDataFrame(nwk.payload, nwk.payloadLen, aps)) {
                      if (apsCb_) {
                        apsCb_(frame, nwk, aps, rssi, lqi);
                      }
                      if (zdoCb_ &&
                          aps.profileId == ZigbeeAps::kProfileZigbeeDevice &&
                          aps.dstEndpoint == ZigbeeZdo::kEndpoint) {
                        zdoCb_(frame, nwk, aps, rssi, lqi);
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
              if (!nwkDrop && nwkCommandCb_) {
                NwkCommandFrame nwkCommand;
                if (ZigbeeNwk::parseCommandFrame(npdu, npduLen, nwkCommand)) {
                  nwkCommandCb_(frame, nwkCommand, rssi, lqi);
                }
              }
            } else {
              if (macCommandCb_) {
                MacCommandFrame command;
                if (ZigbeeMac::parseCommandFrame(psdu, psduLen, command)) {
                  macCommandCb_(command, rssi, lqi);
                }
              }
              if (beaconCb_) {
                MacBeaconFrame beacon;
                if (ZigbeeMac::parseBeacon(psdu, psduLen, beacon)) {
                  beaconCb_(beacon, rssi, lqi);
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
  promiscuous_ = on;
  sendFrame(CMD_SET_PROMISC, &v, 1);
  return waitResp(RSP_OK, 300);
}

bool CC2530Radio::setAddress(uint16_t panId, uint16_t shortAddress,
                             const uint8_t ieeeAddress[8]) {
  uint8_t d[12];
  d[0] = (uint8_t)panId;
  d[1] = (uint8_t)(panId >> 8);
  d[2] = (uint8_t)shortAddress;
  d[3] = (uint8_t)(shortAddress >> 8);
  for (uint8_t i = 0; i < 8; ++i) d[4 + i] = ieeeAddress ? ieeeAddress[i] : 0;
  sendFrame(CMD_SET_ADDR, d, sizeof(d));
  return waitResp(RSP_OK, 300);
}

bool CC2530Radio::configureMac(uint8_t flags, uint8_t retries) {
  uint8_t d[2] = {
      (uint8_t)(flags & (kMacFilter | kMacAutoAck | kMacCcaTx)),
      retries
  };
  // Enabling the hardware frame filter is the opposite of promiscuous mode;
  // keep the cached flag in sync so NWK decryption (gated on !promiscuous_)
  // resumes. Without this a coordinator that only ever calls configureMac()
  // - never setPromiscuous(false) - would stay flagged promiscuous from the
  // setPromiscuous(true) in begin() and silently skip all decryption.
  if (flags & kMacFilter) promiscuous_ = false;
  sendFrame(CMD_SET_MAC, d, sizeof(d));
  return waitResp(RSP_OK, 300);
}

bool CC2530Radio::getMacInfo(CC2530MacInfo& info) {
  sendFrame(CMD_GET_MAC, nullptr, 0);
  if (!waitResp(RSP_MAC_INFO, 300) || respLen_ < 14) return false;
  info.flags = respData_[0];
  info.retries = respData_[1];
  info.panId = (uint16_t)respData_[2] | ((uint16_t)respData_[3] << 8);
  info.shortAddress = (uint16_t)respData_[4] | ((uint16_t)respData_[5] << 8);
  for (uint8_t i = 0; i < 8; ++i) info.ieeeAddress[i] = respData_[6 + i];
  return true;
}

bool CC2530Radio::setTxPowerRaw(uint8_t txpower) {
  sendFrame(CMD_SET_TX_POWER, &txpower, 1);
  return waitResp(RSP_OK, 300);
}

bool CC2530Radio::send(const uint8_t* payload, uint8_t len) {
  if (len > kMaxPayload) return false;
  sendFrame(CMD_TX, payload, len);
  if (!waitResp(RSP_TXSTAT, 500)) return false;
  lastTxAttempts_ = respLen_ >= 2 ? respData_[1] : 0;
  return respLen_ >= 1 && respData_[0] == 0;  // 0 = TXDONE, 1 = fail
}

bool CC2530Radio::sendWithRetries(const uint8_t* payload, uint8_t len,
                                  uint8_t retries) {
  if (len > kMaxPayload) return false;
  uint8_t d[kMaxPayload + 1];
  d[0] = retries;
  for (uint8_t i = 0; i < len; ++i) d[1 + i] = payload[i];
  sendFrame(CMD_TX_ADV, d, (uint8_t)(len + 1));
  if (!waitResp(RSP_TXSTAT, 500)) return false;
  lastTxAttempts_ = respLen_ >= 2 ? respData_[1] : 0;
  return respLen_ >= 1 && respData_[0] == 0;
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

bool CC2530Radio::sendAssociationRequest(uint16_t panId, uint16_t coordShort,
                                         uint64_t srcIeee, uint8_t capability,
                                         bool ackRequest) {
  uint8_t psdu[kMaxPayload];
  uint8_t psduLen = ZigbeeMac::buildAssociationRequest(
      psdu, sizeof(psdu), panId, coordShort, srcIeee, macSequence_++,
      capability, ackRequest);
  if (psduLen == 0) return false;
  return send(psdu, psduLen);
}

bool CC2530Radio::sendAssociationResponse(uint16_t panId, uint64_t dstIeee,
                                          uint16_t srcShort,
                                          uint16_t assignedShort,
                                          uint8_t status,
                                          bool ackRequest) {
  uint8_t psdu[kMaxPayload];
  uint8_t psduLen = ZigbeeMac::buildAssociationResponse(
      psdu, sizeof(psdu), panId, dstIeee, srcShort, macSequence_++,
      assignedShort, status, ackRequest);
  if (psduLen == 0) return false;
  return send(psdu, psduLen);
}

bool CC2530Radio::sendBeaconRequest() {
  uint8_t psdu[kMaxPayload];
  uint8_t psduLen =
      ZigbeeMac::buildBeaconRequest(psdu, sizeof(psdu), macSequence_++);
  if (psduLen == 0) return false;
  return send(psdu, psduLen);
}

bool CC2530Radio::sendBeacon(uint16_t panId, uint16_t srcShort,
                             bool panCoordinator, bool associationPermit,
                             const uint8_t* payload, uint8_t payloadLen) {
  uint8_t psdu[kMaxPayload];
  uint8_t psduLen = ZigbeeMac::buildBeacon(
      psdu, sizeof(psdu), panId, srcShort, macSequence_++, panCoordinator,
      associationPermit, payload, payloadLen);
  if (psduLen == 0) return false;
  return send(psdu, psduLen);
}

uint8_t CC2530Radio::nwkHeaderLength(const uint8_t* npdu, uint8_t len) {
  if (!npdu || len < 8) return 8;
  uint16_t fcf = (uint16_t)npdu[0] | ((uint16_t)npdu[1] << 8);
  uint8_t headerLen = 8;
  if (fcf & (1u << 8)) headerLen += 1;   // multicast control
  if (fcf & (1u << 11)) headerLen += 8;  // destination IEEE
  if (fcf & (1u << 12)) headerLen += 8;  // source IEEE
  return headerLen;
}

bool CC2530Radio::applyTxSecurity(uint8_t* npdu, uint8_t& npduLen,
                                  uint8_t scratchMax) {
  if (!security_ || !security_->hasKey()) return true;
  uint8_t headerLen = nwkHeaderLength(npdu, npduLen);
  uint8_t secured[ZigbeeNwk::kMaxFrame + ZigbeeSecurity::kAuxLen +
                  ZigbeeSecurity::kMicLen];
  uint8_t n = security_->secureNpdu(npdu, npduLen, headerLen, securityIeee_,
                                    ++securityCounter_, secured,
                                    sizeof(secured));
  if (n == 0 || n > scratchMax) return false;
  memcpy(npdu, secured, n);
  npduLen = n;
  return true;
}

bool CC2530Radio::sendNwkData(uint16_t panId, uint16_t macDstShort,
                              uint16_t macSrcShort, uint16_t nwkDstShort,
                              uint16_t nwkSrcShort, const uint8_t* payload,
                              uint8_t len, uint8_t radius,
                              bool ackRequest) {
  uint8_t npdu[ZigbeeNwk::kMaxFrame + ZigbeeSecurity::kAuxLen +
               ZigbeeSecurity::kMicLen];
  uint8_t npduLen = ZigbeeNwk::buildDataFrame(
      npdu, ZigbeeNwk::kMaxFrame, nwkDstShort, nwkSrcShort, radius,
      nwkSequence_++, payload, len);
  if (npduLen == 0) return false;
  if (!applyTxSecurity(npdu, npduLen, sizeof(npdu))) return false;
  return sendData(panId, macDstShort, macSrcShort, npdu, npduLen, ackRequest);
}

bool CC2530Radio::sendNwkDataUnsecured(uint16_t panId, uint16_t macDstShort,
                                       uint16_t macSrcShort,
                                       uint16_t nwkDstShort,
                                       uint16_t nwkSrcShort,
                                       const uint8_t* payload, uint8_t len,
                                       uint8_t radius, bool ackRequest) {
  // Deliberately skips applyTxSecurity(): the frame carries the network key
  // (APS-protected under the link key) to a device that cannot yet decrypt the
  // NWK layer, so the NWK security bit stays clear.
  uint8_t npdu[ZigbeeNwk::kMaxFrame];
  uint8_t npduLen = ZigbeeNwk::buildDataFrame(
      npdu, sizeof(npdu), nwkDstShort, nwkSrcShort, radius, nwkSequence_++,
      payload, len);
  if (npduLen == 0) return false;
  return sendData(panId, macDstShort, macSrcShort, npdu, npduLen, ackRequest);
}

bool CC2530Radio::sendNwkCommand(uint16_t panId, uint16_t macDstShort,
                                 uint16_t macSrcShort, uint16_t nwkDstShort,
                                 uint16_t nwkSrcShort, uint8_t commandId,
                                 const uint8_t* payload, uint8_t len,
                                 uint8_t radius, bool ackRequest) {
  uint8_t npdu[ZigbeeNwk::kMaxFrame + ZigbeeSecurity::kAuxLen +
               ZigbeeSecurity::kMicLen];
  uint8_t npduLen = ZigbeeNwk::buildCommandFrame(
      npdu, ZigbeeNwk::kMaxFrame, nwkDstShort, nwkSrcShort, radius,
      nwkSequence_++, commandId, payload, len);
  if (npduLen == 0) return false;
  if (!applyTxSecurity(npdu, npduLen, sizeof(npdu))) return false;
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

bool CC2530Radio::sendZdoCommand(uint16_t panId, uint16_t macDstShort,
                                 uint16_t macSrcShort, uint16_t nwkDstShort,
                                 uint16_t nwkSrcShort, uint16_t clusterId,
                                 const uint8_t* payload, uint8_t len,
                                 uint8_t radius, bool ackRequest) {
  return sendApsData(panId, macDstShort, macSrcShort, nwkDstShort, nwkSrcShort,
                     ZigbeeZdo::kEndpoint, clusterId,
                     ZigbeeAps::kProfileZigbeeDevice, ZigbeeZdo::kEndpoint,
                     payload, len, radius, ackRequest);
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
