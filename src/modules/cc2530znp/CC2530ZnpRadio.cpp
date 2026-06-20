/*
  CC2530ZnpRadio.cpp - Z-Stack ZNP (Monitor-and-Test API) host driver.
  See CC2530ZnpRadio.h for the MT frame format and design notes.
*/
#include "CC2530ZnpRadio.h"

namespace nzb {

// MT command ids (CMD1), grouped by subsystem.
namespace {
// SYS subsystem.
const uint8_t SYS_RESET_REQ = 0x00;   // AREQ
const uint8_t SYS_RESET_IND = 0x80;   // AREQ
const uint8_t SYS_PING = 0x01;        // SREQ
const uint8_t SYS_VERSION = 0x02;     // SREQ
const uint8_t SYS_OSAL_NV_WRITE = 0x09;  // SREQ
// ZDO subsystem.
const uint8_t ZDO_STARTUP_FROM_APP = 0x40;  // SREQ
const uint8_t ZDO_STATE_CHANGE_IND = 0xC0;  // AREQ
// AF subsystem.
const uint8_t AF_REGISTER = 0x00;      // SREQ
const uint8_t AF_DATA_REQUEST = 0x01;  // SREQ
const uint8_t AF_INCOMING_MSG = 0x81;  // AREQ
// UTIL subsystem.
const uint8_t UTIL_GET_DEVICE_INFO = 0x00;  // SREQ

// Z-Stack NV item ids.
const uint16_t ZCD_NV_STARTUP_OPTION = 0x0003;
const uint16_t ZCD_NV_LOGICAL_TYPE = 0x0087;
const uint16_t ZCD_NV_PANID = 0x0083;
const uint16_t ZCD_NV_CHANLIST = 0x0084;
}  // namespace

CC2530ZnpRadio::CC2530ZnpRadio(HardwareSerial& serial)
    : serial_(&serial),
      rxState_(kWaitSof),
      rxLen_(0), rxIdx_(0), rxCmd0_(0), rxCmd1_(0), rxFcs_(0),
      frmCmd0_(0), frmCmd1_(0), frmLen_(0),
      respLen_(0),
      capabilities_(0),
      incomingCb_(nullptr), stateCb_(nullptr) {}

bool CC2530ZnpRadio::begin(uint32_t baud) {
  serial_->begin(baud);
  // Drain any boot chatter before talking MT.
  uint32_t t0 = millis();
  while (millis() - t0 < 50) {
    while (serial_->available()) serial_->read();
  }
  if (!reset(/*soft=*/false)) return false;
  return ping();
}

uint8_t CC2530ZnpRadio::computeFcs(uint8_t len, uint8_t cmd0, uint8_t cmd1,
                                   const uint8_t* data) {
  uint8_t fcs = (uint8_t)(len ^ cmd0 ^ cmd1);
  for (uint8_t i = 0; i < len; ++i) fcs ^= data[i];
  return fcs;
}

uint8_t CC2530ZnpRadio::encodeFrame(uint8_t* out, uint8_t outMax, uint8_t cmd0,
                                    uint8_t cmd1, const uint8_t* data,
                                    uint8_t len) {
  if (!out || (uint16_t)len + 5 > outMax) return 0;  // SOF+LEN+CMD0+CMD1+DATA+FCS
  out[0] = kSof;
  out[1] = len;
  out[2] = cmd0;
  out[3] = cmd1;
  for (uint8_t i = 0; i < len; ++i) out[4 + i] = data[i];
  out[4 + len] = computeFcs(len, cmd0, cmd1, data);
  return (uint8_t)(5 + len);
}

bool CC2530ZnpRadio::decodeFrame(const uint8_t* in, uint8_t inLen, uint8_t& cmd0,
                                 uint8_t& cmd1, const uint8_t** data,
                                 uint8_t& dataLen) {
  if (!in || inLen < 5 || in[0] != kSof) return false;
  uint8_t len = in[1];
  if ((uint16_t)len + 5 != inLen) return false;
  cmd0 = in[2];
  cmd1 = in[3];
  if (computeFcs(len, cmd0, cmd1, &in[4]) != in[4 + len]) return false;
  *data = &in[4];
  dataLen = len;
  return true;
}

void CC2530ZnpRadio::sendMt(uint8_t type, uint8_t sub, uint8_t cmd1,
                            const uint8_t* data, uint8_t len) {
  uint8_t cmd0 = (uint8_t)(type | (sub & 0x1F));
  uint8_t frame[5 + kMaxData];
  uint8_t n = encodeFrame(frame, sizeof(frame), cmd0, cmd1, data, len);
  for (uint8_t i = 0; i < n; ++i) serial_->write(frame[i]);
}

bool CC2530ZnpRadio::feed(uint8_t b) {
  switch (rxState_) {
    case kWaitSof:
      if (b == kSof) { rxState_ = kLen; }
      return false;
    case kLen:
      rxLen_ = b;
      rxIdx_ = 0;
      rxFcs_ = b;
      rxState_ = kCmd0;
      return false;
    case kCmd0:
      rxCmd0_ = b; rxFcs_ ^= b; rxState_ = kCmd1; return false;
    case kCmd1:
      rxCmd1_ = b; rxFcs_ ^= b;
      rxState_ = (rxLen_ > 0) ? kData : kFcs;
      return false;
    case kData:
      frm_[rxIdx_++] = b; rxFcs_ ^= b;
      if (rxIdx_ >= rxLen_) rxState_ = kFcs;
      return false;
    case kFcs:
      rxState_ = kWaitSof;
      if (b != rxFcs_) return false;  // bad checksum - drop
      frmCmd0_ = rxCmd0_;
      frmCmd1_ = rxCmd1_;
      frmLen_ = rxLen_;
      return true;
  }
  rxState_ = kWaitSof;
  return false;
}

bool CC2530ZnpRadio::readFrame(uint32_t timeoutMs) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (serial_->available()) {
      if (feed((uint8_t)serial_->read())) return true;
    }
    yield();
  }
  return false;
}

void CC2530ZnpRadio::dispatchAreq() {
  // AF incoming message.
  if (frmCmd0_ == (uint8_t)(kTypeAreq | kSubAf) && frmCmd1_ == AF_INCOMING_MSG) {
    if (!incomingCb_ || frmLen_ < 17) return;
    ZnpIncomingMsg m;
    m.groupId = (uint16_t)frm_[0] | ((uint16_t)frm_[1] << 8);
    m.clusterId = (uint16_t)frm_[2] | ((uint16_t)frm_[3] << 8);
    m.srcAddr = (uint16_t)frm_[4] | ((uint16_t)frm_[5] << 8);
    m.srcEndpoint = frm_[6];
    m.dstEndpoint = frm_[7];
    m.wasBroadcast = frm_[8] != 0;
    m.linkQuality = frm_[9];
    // frm_[10] securityUse, frm_[11..14] timestamp
    m.transSeqNum = frm_[15];
    m.len = frm_[16];
    if ((uint16_t)17 + m.len > frmLen_) return;
    m.data = &frm_[17];
    incomingCb_(m);
    return;
  }
  // ZDO device-state change.
  if (frmCmd0_ == (uint8_t)(kTypeAreq | kSubZdo) &&
      frmCmd1_ == ZDO_STATE_CHANGE_IND) {
    if (stateCb_ && frmLen_ >= 1) stateCb_(frm_[0]);
    return;
  }
}

bool CC2530ZnpRadio::sreq(uint8_t sub, uint8_t cmd1, const uint8_t* data,
                          uint8_t len, uint32_t timeoutMs) {
  sendMt(kTypeSreq, sub, cmd1, data, len);
  uint8_t wantCmd0 = (uint8_t)(kTypeSrsp | (sub & 0x1F));
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (!readFrame(timeoutMs)) break;
    if (frmCmd0_ == wantCmd0 && frmCmd1_ == cmd1) {
      respLen_ = frmLen_;
      for (uint8_t i = 0; i < frmLen_; ++i) resp_[i] = frm_[i];
      return true;
    }
    dispatchAreq();  // an indication arrived while we waited - deliver it
  }
  return false;
}

bool CC2530ZnpRadio::reset(bool soft, uint32_t timeoutMs) {
  uint8_t type = soft ? 1 : 0;
  sendMt(kTypeAreq, kSubSys, SYS_RESET_REQ, &type, 1);
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (!readFrame(timeoutMs)) break;
    if (frmCmd0_ == (uint8_t)(kTypeAreq | kSubSys) &&
        frmCmd1_ == SYS_RESET_IND) {
      return true;
    }
    dispatchAreq();
  }
  return false;
}

bool CC2530ZnpRadio::ping() {
  if (!sreq(kSubSys, SYS_PING, nullptr, 0)) return false;
  if (respLen_ >= 2) capabilities_ = (uint16_t)resp_[0] | ((uint16_t)resp_[1] << 8);
  return true;
}

bool CC2530ZnpRadio::getVersion(ZnpVersion& out) {
  if (!sreq(kSubSys, SYS_VERSION, nullptr, 0) || respLen_ < 5) return false;
  out.transportRev = resp_[0];
  out.product = resp_[1];
  out.major = resp_[2];
  out.minor = resp_[3];
  out.maint = resp_[4];
  return true;
}

bool CC2530ZnpRadio::getDeviceInfo(ZnpDeviceInfo& out) {
  if (!sreq(kSubUtil, UTIL_GET_DEVICE_INFO, nullptr, 0) || respLen_ < 14)
    return false;
  out.status = resp_[0];
  for (uint8_t i = 0; i < 8; ++i) out.ieee[i] = resp_[1 + i];
  out.shortAddr = (uint16_t)resp_[9] | ((uint16_t)resp_[10] << 8);
  out.deviceType = resp_[11];
  out.deviceState = resp_[12];
  out.numAssocDevices = resp_[13];
  return true;
}

bool CC2530ZnpRadio::nvWrite(uint16_t nvId, uint8_t offset,
                             const uint8_t* value, uint8_t len) {
  uint8_t d[4 + 32];
  if (len > 32) return false;
  d[0] = (uint8_t)nvId;
  d[1] = (uint8_t)(nvId >> 8);
  d[2] = offset;
  d[3] = len;
  for (uint8_t i = 0; i < len; ++i) d[4 + i] = value[i];
  if (!sreq(kSubSys, SYS_OSAL_NV_WRITE, d, (uint8_t)(4 + len))) return false;
  return respLen_ >= 1 && resp_[0] == 0;  // 0 = ZSuccess
}

bool CC2530ZnpRadio::setLogicalType(ZnpLogicalType type) {
  uint8_t v = (uint8_t)type;
  return nvWrite(ZCD_NV_LOGICAL_TYPE, 0, &v, 1);
}

bool CC2530ZnpRadio::setStartupOption(uint8_t startopt) {
  return nvWrite(ZCD_NV_STARTUP_OPTION, 0, &startopt, 1);
}

bool CC2530ZnpRadio::setPanId(uint16_t panId) {
  uint8_t v[2] = {(uint8_t)panId, (uint8_t)(panId >> 8)};
  return nvWrite(ZCD_NV_PANID, 0, v, 2);
}

bool CC2530ZnpRadio::setChannel(uint8_t channel) {
  if (channel < 11 || channel > 26) return false;
  uint32_t mask = (uint32_t)1 << channel;
  uint8_t v[4] = {(uint8_t)mask, (uint8_t)(mask >> 8), (uint8_t)(mask >> 16),
                  (uint8_t)(mask >> 24)};
  return nvWrite(ZCD_NV_CHANLIST, 0, v, 4);
}

bool CC2530ZnpRadio::startupFromApp(uint16_t startDelayMs, uint8_t& statusOut) {
  uint8_t d[2] = {(uint8_t)startDelayMs, (uint8_t)(startDelayMs >> 8)};
  if (!sreq(kSubZdo, ZDO_STARTUP_FROM_APP, d, 2)) return false;
  statusOut = respLen_ >= 1 ? resp_[0] : 0xFF;
  return true;
}

bool CC2530ZnpRadio::registerEndpoint(uint8_t endpoint, uint16_t profileId,
                                      uint16_t deviceId, uint8_t deviceVer,
                                      const uint16_t* inClusters, uint8_t numIn,
                                      const uint16_t* outClusters,
                                      uint8_t numOut) {
  uint8_t d[kMaxData];
  uint8_t i = 0;
  d[i++] = endpoint;
  d[i++] = (uint8_t)profileId;
  d[i++] = (uint8_t)(profileId >> 8);
  d[i++] = (uint8_t)deviceId;
  d[i++] = (uint8_t)(deviceId >> 8);
  d[i++] = deviceVer;
  d[i++] = 0;  // LatencyReq: 0 = no latency
  d[i++] = numIn;
  for (uint8_t c = 0; c < numIn; ++c) {
    d[i++] = (uint8_t)inClusters[c];
    d[i++] = (uint8_t)(inClusters[c] >> 8);
  }
  d[i++] = numOut;
  for (uint8_t c = 0; c < numOut; ++c) {
    d[i++] = (uint8_t)outClusters[c];
    d[i++] = (uint8_t)(outClusters[c] >> 8);
  }
  if (!sreq(kSubAf, AF_REGISTER, d, i)) return false;
  return respLen_ >= 1 && resp_[0] == 0;
}

bool CC2530ZnpRadio::sendData(uint16_t dstAddr, uint8_t dstEndpoint,
                              uint8_t srcEndpoint, uint16_t clusterId,
                              uint8_t transId, const uint8_t* data, uint8_t len,
                              uint8_t radius, bool ackRequest) {
  if (len > kMaxAfPayload) return false;
  uint8_t d[kMaxData];
  uint8_t i = 0;
  d[i++] = (uint8_t)dstAddr;
  d[i++] = (uint8_t)(dstAddr >> 8);
  d[i++] = dstEndpoint;
  d[i++] = srcEndpoint;
  d[i++] = (uint8_t)clusterId;
  d[i++] = (uint8_t)(clusterId >> 8);
  d[i++] = transId;
  d[i++] = ackRequest ? 0x10 : 0x00;  // options: bit4 = APS ACK
  d[i++] = radius;
  d[i++] = len;
  for (uint8_t b = 0; b < len; ++b) d[i++] = data[b];
  if (!sreq(kSubAf, AF_DATA_REQUEST, d, i)) return false;
  return respLen_ >= 1 && resp_[0] == 0;
}

void CC2530ZnpRadio::poll() {
  while (serial_->available()) {
    if (feed((uint8_t)serial_->read())) {
      // Only AREQ indications are unsolicited; SRSPs are consumed inside sreq().
      if ((frmCmd0_ & 0xE0) == kTypeAreq) dispatchAreq();
    }
  }
}

}  // namespace nzb
