/*
  CC2530Radio.h - host driver for the CC2530 802.15.4 radio co-processor.

  Runs on an ArduinoNRF (nRF52840) board and drives a CC2530 module that has been
  flashed with this library's SDCC transceiver firmware
  (extras/firmware/cc2530/cc2530_radio.c). The nRF talks to the CC2530 over a
  hardware UART (Serial1 by default) at 115200 baud using a small framed protocol;
  the CC2530 does the actual 2.4 GHz IEEE 802.15.4 PHY/MAC.

  This is a small 802.15.4 MAC/PHY helper, not a full Zigbee PRO stack: it can
  set hardware PAN/address filtering, Auto ACK, CCA TX, and retries, while the
  nRF host still owns NWK/APS/ZCL behavior. A future Z-Stack-backed driver can
  sit beside this one (see ZigbeeModule.h).

  Wiring and how to flash the CC2530 firmware (using ArduinoNRF's built-in
  CC-Debugger) are documented in docs/WIRING.md and docs/FLASHING.md.
*/
#ifndef ARDUINONRF_ZIGBEE_CC2530RADIO_H
#define ARDUINONRF_ZIGBEE_CC2530RADIO_H

#include <Arduino.h>
#include "../../ZigbeeMac.h"
#include "../../ZigbeeNwk.h"
#include "../../ZigbeeAps.h"
#include "../../ZigbeeZdo.h"
#include "../../ZigbeeZcl.h"

namespace nzb {

/**
 * Received-frame callback.
 * @param psdu  the 802.15.4 payload (MHR+MAC payload), WITHOUT the 2-byte FCS
 * @param len   length of @p psdu in bytes
 * @param rssi  received signal strength in dBm (already offset-corrected)
 * @param lqi   link-quality byte: bit7 = CRC OK, bits[6:0] = correlation value
 */
typedef void (*CC2530RxCallback)(const uint8_t* psdu, uint8_t len, int8_t rssi,
                                 uint8_t lqi);
typedef void (*CC2530DataCallback)(const MacDataFrame& frame, int8_t rssi,
                                   uint8_t lqi);
typedef void (*CC2530MacCommandCallback)(const MacCommandFrame& frame,
                                         int8_t rssi, uint8_t lqi);
typedef void (*CC2530BeaconCallback)(const MacBeaconFrame& frame, int8_t rssi,
                                     uint8_t lqi);
typedef void (*CC2530NwkCallback)(const MacDataFrame& mac,
                                  const NwkDataFrame& nwk, int8_t rssi,
                                  uint8_t lqi);
typedef void (*CC2530NwkCommandCallback)(const MacDataFrame& mac,
                                         const NwkCommandFrame& nwk,
                                         int8_t rssi, uint8_t lqi);
typedef void (*CC2530ApsCallback)(const MacDataFrame& mac,
                                  const NwkDataFrame& nwk,
                                  const ApsDataFrame& aps, int8_t rssi,
                                  uint8_t lqi);
typedef void (*CC2530ZdoCallback)(const MacDataFrame& mac,
                                  const NwkDataFrame& nwk,
                                  const ApsDataFrame& aps, int8_t rssi,
                                  uint8_t lqi);
typedef void (*CC2530ZclCallback)(const MacDataFrame& mac,
                                  const NwkDataFrame& nwk,
                                  const ApsDataFrame& aps,
                                  const ZclFrame& zcl, int8_t rssi,
                                  uint8_t lqi);

struct CC2530MacInfo {
  uint8_t flags;
  uint8_t retries;
  uint16_t panId;
  uint16_t shortAddress;
  uint8_t ieeeAddress[8];
};

class CC2530Radio {
 public:
  /** Largest 802.15.4 payload we accept (127-byte PHY frame minus 2-byte FCS). */
  static const uint8_t kMaxPayload = 125;
  static const uint8_t kMacFilter = 0x01;
  static const uint8_t kMacAutoAck = 0x02;
  static const uint8_t kMacCcaTx = 0x04;

  /** @param serial the UART wired to the module (Serial1 = D0/D1 on ProMicro). */
  explicit CC2530Radio(HardwareSerial& serial = Serial1);

  /**
   * Open the UART and bring the link up: pings the module and selects @p channel.
   * @return true if the module answered (firmware is running and wired correctly).
   */
  bool begin(uint8_t channel = 11, uint32_t baud = 115200);

  /** Round-trip PING; true if the module replies. Also refreshes firmwareVersion(). */
  bool ping();

  /** Firmware version (high<<8 | low), valid after a successful ping()/begin(). */
  uint16_t firmwareVersion() const { return version_; }

  /** Select the 802.15.4 channel (11..26). */
  bool setChannel(uint8_t channel);
  uint8_t channel() const { return channel_; }

  /**
   * Promiscuous mode: true = receive every frame on the channel (sniffer);
   * false = hardware address/PAN filtering. Default after begin() is true.
   */
  bool setPromiscuous(bool on);

  /** Program the CC2530 hardware PAN/short/IEEE address registers. */
  bool setAddress(uint16_t panId, uint16_t shortAddress,
                  const uint8_t ieeeAddress[8]);

  /** Configure low-level MAC assist flags and default TX retry count. */
  bool configureMac(uint8_t flags, uint8_t retries = 0);

  /** Read back the low-level MAC assist configuration from the CC2530. */
  bool getMacInfo(CC2530MacInfo& info);

  /** Set the raw CC2530 TXPOWER register value. */
  bool setTxPowerRaw(uint8_t txpower);

  /** Transmit a raw 802.15.4 frame (the radio appends the FCS). @return true on TXDONE. */
  bool send(const uint8_t* payload, uint8_t len);

  /** Transmit once with a per-call retry count, leaving the default retry count unchanged. */
  bool sendWithRetries(const uint8_t* payload, uint8_t len, uint8_t retries);

  uint8_t lastTxAttempts() const { return lastTxAttempts_; }

  /** Transmit a short-address IEEE 802.15.4 data frame. */
  bool sendData(uint16_t panId, uint16_t dstShort, uint16_t srcShort,
                const uint8_t* payload, uint8_t len,
                bool ackRequest = false);

  /** Transmit a MAC Association Request from an extended-address child. */
  bool sendAssociationRequest(uint16_t panId, uint16_t coordShort,
                              uint64_t srcIeee, uint8_t capability,
                              bool ackRequest = true);

  /** Transmit a MAC Association Response to an extended-address child. */
  bool sendAssociationResponse(uint16_t panId, uint64_t dstIeee,
                               uint16_t srcShort, uint16_t assignedShort,
                               uint8_t status, bool ackRequest = true);

  /** Broadcast a Beacon Request MAC command (active scan probe). */
  bool sendBeaconRequest();

  /** Transmit an 802.15.4 beacon carrying @p payload (beaconless PAN). */
  bool sendBeacon(uint16_t panId, uint16_t srcShort, bool panCoordinator,
                  bool associationPermit, const uint8_t* payload,
                  uint8_t payloadLen);

  /** Transmit a simple Zigbee NWK data frame inside a short-address MAC frame. */
  bool sendNwkData(uint16_t panId, uint16_t macDstShort, uint16_t macSrcShort,
                   uint16_t nwkDstShort, uint16_t nwkSrcShort,
                   const uint8_t* payload, uint8_t len,
                   uint8_t radius = ZigbeeNwk::kDefaultRadius,
                   bool ackRequest = false);

  /** Transmit a Zigbee NWK command frame inside a short-address MAC frame. */
  bool sendNwkCommand(uint16_t panId, uint16_t macDstShort,
                      uint16_t macSrcShort, uint16_t nwkDstShort,
                      uint16_t nwkSrcShort, uint8_t commandId,
                      const uint8_t* payload, uint8_t len,
                      uint8_t radius = ZigbeeNwk::kDefaultRadius,
                      bool ackRequest = false);

  /** Transmit a simple Zigbee APS data frame inside a NWK data frame. */
  bool sendApsData(uint16_t panId, uint16_t macDstShort, uint16_t macSrcShort,
                   uint16_t nwkDstShort, uint16_t nwkSrcShort,
                   uint8_t dstEndpoint, uint16_t clusterId,
                   uint16_t profileId, uint8_t srcEndpoint,
                   const uint8_t* payload, uint8_t len,
                   uint8_t radius = ZigbeeNwk::kDefaultRadius,
                   bool ackRequest = false);

  /** Transmit a Zigbee Device Profile command on endpoint 0. */
  bool sendZdoCommand(uint16_t panId, uint16_t macDstShort,
                      uint16_t macSrcShort, uint16_t nwkDstShort,
                      uint16_t nwkSrcShort, uint16_t clusterId,
                      const uint8_t* payload, uint8_t len,
                      uint8_t radius = ZigbeeNwk::kDefaultRadius,
                      bool ackRequest = false);

  /** Transmit a simple ZCL command inside APS/NWK/MAC data frames. */
  bool sendZclCommand(uint16_t panId, uint16_t macDstShort,
                      uint16_t macSrcShort, uint16_t nwkDstShort,
                      uint16_t nwkSrcShort, uint8_t dstEndpoint,
                      uint16_t clusterId, uint16_t profileId,
                      uint8_t srcEndpoint, uint8_t commandId,
                      const uint8_t* payload = nullptr, uint8_t len = 0,
                      uint8_t zclFrameType = ZCL_FRAME_CLUSTER_SPECIFIC,
                      uint8_t zclDirection = ZCL_DIRECTION_CLIENT_TO_SERVER,
                      uint8_t radius = ZigbeeNwk::kDefaultRadius,
                      bool ackRequest = false);

  /** Register the received-frame callback (delivered from poll()). */
  void onReceive(CC2530RxCallback cb) { rxCb_ = cb; }

  /** Register a parsed short-address data-frame callback (delivered from poll()). */
  void onDataReceive(CC2530DataCallback cb) { dataCb_ = cb; }

  /** Register a parsed MAC command-frame callback (delivered from poll()). */
  void onMacCommandReceive(CC2530MacCommandCallback cb) { macCommandCb_ = cb; }

  /** Register a parsed 802.15.4 beacon callback (delivered from poll()). */
  void onBeaconReceive(CC2530BeaconCallback cb) { beaconCb_ = cb; }

  /** Register a parsed Zigbee NWK data-frame callback (delivered from poll()). */
  void onNwkReceive(CC2530NwkCallback cb) { nwkCb_ = cb; }

  /** Register a parsed Zigbee NWK command-frame callback (delivered from poll()). */
  void onNwkCommandReceive(CC2530NwkCommandCallback cb) { nwkCommandCb_ = cb; }

  /** Register a parsed Zigbee APS data-frame callback (delivered from poll()). */
  void onApsReceive(CC2530ApsCallback cb) { apsCb_ = cb; }

  /** Register a Zigbee Device Profile callback for endpoint 0/profile 0. */
  void onZdoReceive(CC2530ZdoCallback cb) { zdoCb_ = cb; }

  /** Register a parsed ZCL command callback (delivered from poll()). */
  void onZclReceive(CC2530ZclCallback cb) { zclCb_ = cb; }

  /** Pump the UART and deliver any received frames. Call often from loop(). */
  void poll();

 private:
  HardwareSerial* serial_;
  CC2530RxCallback rxCb_;
  CC2530DataCallback dataCb_;
  CC2530MacCommandCallback macCommandCb_;
  CC2530BeaconCallback beaconCb_;
  CC2530NwkCallback nwkCb_;
  CC2530NwkCommandCallback nwkCommandCb_;
  CC2530ApsCallback apsCb_;
  CC2530ZdoCallback zdoCb_;
  CC2530ZclCallback zclCb_;
  uint16_t version_;
  uint8_t channel_;
  uint8_t macSequence_;
  uint8_t nwkSequence_;
  uint8_t apsCounter_;
  uint8_t zclSequence_;
  uint8_t lastTxAttempts_;

  // incoming-frame parser
  uint8_t state_, len_, idx_, fcs_;
  uint8_t buf_[140];
  // captured command response (non-0x84 frames), consumed by waitResp()
  uint8_t respCmd_, respData_[16], respLen_;
  bool respReady_;

  void sendFrame(uint8_t cmd, const uint8_t* data, uint8_t n);
  void feed(uint8_t b);                 // parse one byte; dispatch on full frame
  bool waitResp(uint8_t cmd, uint32_t timeoutMs);
};

}  // namespace nzb

#endif  // ARDUINONRF_ZIGBEE_CC2530RADIO_H
