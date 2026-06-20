/*
  CC2530ZnpRadio.h - host driver for a CC2530 running TI Z-Stack ZNP.

  This is the SECOND module backend anticipated by ZigbeeModule.h. Unlike
  CC2530Radio (which drives a CC2530 flashed with this library's raw 802.15.4
  SDCC firmware while the nRF host owns the whole Zigbee stack), this driver
  talks to a CC2530 flashed with TI's Z-Stack ZNP firmware: the CC2530 runs the
  certified Zigbee PRO stack itself, and the nRF host drives it over the UART
  using the Z-Stack Monitor-and-Test (MT) API.

  MT frame layout (UART transport):
      SOF(0xFE) LEN(1) CMD0(1) CMD1(1) DATA(LEN) FCS(1)
  where FCS = XOR over LEN, CMD0, CMD1 and every DATA byte. CMD0 packs a type in
  its top 3 bits (SREQ=0x20, AREQ=0x40, SRSP=0x60) and a subsystem in the low 5
  (SYS=0x01, AF=0x04, ZDO=0x05, UTIL=0x07). A synchronous request (SREQ) is
  answered by exactly one SRSP; asynchronous indications (AREQ) arrive whenever
  the stack has something to report and are dispatched from poll().

  The ZNP firmware itself is built with IAR EW 8051 (see
  extras/firmware/cc2530znp/BUILD.md) and flashed with the same built-in
  CC-Debugger as the SDCC firmware - this only changes the CC2530 module image,
  never an Arduino board bootloader.

  Status: host driver is complete and compile-clean; on-air bring-up is gated on
  building/flashing the ZNP firmware with IAR.
*/
#ifndef ARDUINONRF_ZIGBEE_CC2530ZNPRADIO_H
#define ARDUINONRF_ZIGBEE_CC2530ZNPRADIO_H

#include <Arduino.h>

namespace nzb {

/** Logical device type passed to startup; matches Z-Stack LOGICAL_TYPE NV. */
enum ZnpLogicalType : uint8_t {
  ZNP_COORDINATOR = 0x00,
  ZNP_ROUTER = 0x01,
  ZNP_END_DEVICE = 0x02,
};

/** ZNP firmware version, from SYS_VERSION. */
struct ZnpVersion {
  uint8_t transportRev;
  uint8_t product;
  uint8_t major;
  uint8_t minor;
  uint8_t maint;
};

/** Device info, from UTIL_GET_DEVICE_INFO. */
struct ZnpDeviceInfo {
  uint8_t status;
  uint8_t ieee[8];
  uint16_t shortAddr;
  uint8_t deviceType;
  uint8_t deviceState;   // see ZDO state (9 = started as coordinator)
  uint8_t numAssocDevices;
};

/** One received AF (application framework) message, from AF_INCOMING_MSG. */
struct ZnpIncomingMsg {
  uint16_t groupId;
  uint16_t clusterId;
  uint16_t srcAddr;
  uint8_t srcEndpoint;
  uint8_t dstEndpoint;
  bool wasBroadcast;
  uint8_t linkQuality;
  uint8_t transSeqNum;
  const uint8_t* data;
  uint8_t len;
};

typedef void (*ZnpIncomingCallback)(const ZnpIncomingMsg& msg);
typedef void (*ZnpStateChangeCallback)(uint8_t deviceState);

class CC2530ZnpRadio {
 public:
  static const uint8_t kSof = 0xFE;
  static const uint8_t kMaxData = 250;       // MT DATA field cap
  static const uint8_t kMaxAfPayload = 100;  // practical AF data-request payload

  // CMD0 type bits and subsystem ids.
  static const uint8_t kTypeSreq = 0x20;
  static const uint8_t kTypeAreq = 0x40;
  static const uint8_t kTypeSrsp = 0x60;
  static const uint8_t kSubSys = 0x01;
  static const uint8_t kSubAf = 0x04;
  static const uint8_t kSubZdo = 0x05;
  static const uint8_t kSubUtil = 0x07;

  explicit CC2530ZnpRadio(HardwareSerial& serial = Serial1);

  /** Open the UART, reset the ZNP, and confirm it answers SYS_PING. */
  bool begin(uint32_t baud = 115200);

  /** Hardware/soft reset; waits for SYS_RESET_IND. @return true if it arrived. */
  bool reset(bool soft = false, uint32_t timeoutMs = 2000);

  /** SYS_PING; true if the ZNP replies. Capabilities are stored. */
  bool ping();
  uint16_t capabilities() const { return capabilities_; }

  /** SYS_VERSION. */
  bool getVersion(ZnpVersion& out);

  /** UTIL_GET_DEVICE_INFO (short address, IEEE, state). */
  bool getDeviceInfo(ZnpDeviceInfo& out);

  /** Write the LOGICAL_TYPE startup option (NV 0x87) before starting the stack. */
  bool setLogicalType(ZnpLogicalType type);

  /** Set STARTOPT NV (0x03): bit1 clears network state for a fresh form/join. */
  bool setStartupOption(uint8_t startopt);

  /** Set the 16-bit PAN ID NV (0x83); 0xFFFF = let the stack choose. */
  bool setPanId(uint16_t panId);

  /** Set the channel mask NV (0x84) to a single channel (11..26). */
  bool setChannel(uint8_t channel);

  /** ZDO_STARTUP_FROM_APP: bring the network up using the NV config. The result
      is reported asynchronously via the state-change callback. @return the SRSP
      status (0=restored, 1=new network, 2=leave). */
  bool startupFromApp(uint16_t startDelayMs, uint8_t& statusOut);

  /** AF_REGISTER an application endpoint (profile/device + in/out clusters). */
  bool registerEndpoint(uint8_t endpoint, uint16_t profileId, uint16_t deviceId,
                        uint8_t deviceVer, const uint16_t* inClusters,
                        uint8_t numIn, const uint16_t* outClusters,
                        uint8_t numOut);

  /** AF_DATA_REQUEST: unicast an AF message. @return SRSP status (0 = success). */
  bool sendData(uint16_t dstAddr, uint8_t dstEndpoint, uint8_t srcEndpoint,
                uint16_t clusterId, uint8_t transId, const uint8_t* data,
                uint8_t len, uint8_t radius = 30, bool ackRequest = true);

  void onIncoming(ZnpIncomingCallback cb) { incomingCb_ = cb; }
  void onStateChange(ZnpStateChangeCallback cb) { stateCb_ = cb; }

  /** Pump the UART: dispatches AREQ indications (incoming msgs, state changes).
      Call frequently from loop(). */
  void poll();

 private:
  // Build + send an MT frame. type = kTypeSreq/kTypeAreq, sub = subsystem.
  void sendMt(uint8_t type, uint8_t sub, uint8_t cmd1, const uint8_t* data,
              uint8_t len);
  // Send an SREQ and block for its matching SRSP. Returns true on a clean SRSP;
  // the response payload is left in resp_/respLen_.
  bool sreq(uint8_t sub, uint8_t cmd1, const uint8_t* data, uint8_t len,
            uint32_t timeoutMs = 1000);
  // Feed one received byte into the frame parser; returns true once a full,
  // FCS-valid frame is in frm_/frmCmd0_/frmCmd1_/frmLen_.
  bool feed(uint8_t b);
  // Block (up to timeoutMs) for the next complete frame. Returns true on a frame.
  bool readFrame(uint32_t timeoutMs);
  // Route a parsed AREQ indication to the registered callbacks.
  void dispatchAreq();
  // Write one NV item via SYS_OSAL_NV_WRITE (portable across Z-Stack versions).
  bool nvWrite(uint16_t nvId, uint8_t offset, const uint8_t* value, uint8_t len);

  HardwareSerial* serial_;

  // Frame-parser state.
  enum RxState { kWaitSof, kLen, kCmd0, kCmd1, kData, kFcs };
  RxState rxState_;
  uint8_t rxLen_, rxIdx_, rxCmd0_, rxCmd1_, rxFcs_;

  // Last fully-parsed frame.
  uint8_t frmCmd0_, frmCmd1_, frmLen_;
  uint8_t frm_[kMaxData];

  // Last SRSP payload (copied out of frm_ by sreq()).
  uint8_t resp_[kMaxData];
  uint8_t respLen_;

  uint16_t capabilities_;

  ZnpIncomingCallback incomingCb_;
  ZnpStateChangeCallback stateCb_;
};

}  // namespace nzb

#endif  // ARDUINONRF_ZIGBEE_CC2530ZNPRADIO_H
