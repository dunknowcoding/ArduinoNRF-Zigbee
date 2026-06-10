/*
  CC2530Radio.h - host driver for the CC2530 802.15.4 radio co-processor.

  Runs on an ArduinoNRF (nRF52840) board and drives a CC2530 module that has been
  flashed with this library's SDCC transceiver firmware
  (extras/firmware/cc2530/cc2530_radio.c). The nRF talks to the CC2530 over a
  hardware UART (Serial1 by default) at 115200 baud using a small framed protocol;
  the CC2530 does the actual 2.4 GHz IEEE 802.15.4 PHY/MAC.

  This is raw 802.15.4 (send/receive PHY frames, promiscuous sniffing), not a full
  Zigbee PRO stack - perfect for custom links, sniffers, and exercising the radio.
  A future Z-Stack-backed driver can sit beside this one (see ZigbeeModule.h).

  Wiring and how to flash the CC2530 firmware (using ArduinoNRF's built-in
  CC-Debugger) are documented in docs/WIRING.md and docs/FLASHING.md.
*/
#ifndef ARDUINONRF_ZIGBEE_CC2530RADIO_H
#define ARDUINONRF_ZIGBEE_CC2530RADIO_H

#include <Arduino.h>
#include "../../ZigbeeMac.h"

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

class CC2530Radio {
 public:
  /** Largest 802.15.4 payload we accept (127-byte PHY frame minus 2-byte FCS). */
  static const uint8_t kMaxPayload = 125;

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

  /** Transmit a raw 802.15.4 frame (the radio appends the FCS). @return true on TXDONE. */
  bool send(const uint8_t* payload, uint8_t len);

  /** Transmit a short-address IEEE 802.15.4 data frame. */
  bool sendData(uint16_t panId, uint16_t dstShort, uint16_t srcShort,
                const uint8_t* payload, uint8_t len,
                bool ackRequest = false);

  /** Register the received-frame callback (delivered from poll()). */
  void onReceive(CC2530RxCallback cb) { rxCb_ = cb; }

  /** Register a parsed short-address data-frame callback (delivered from poll()). */
  void onDataReceive(CC2530DataCallback cb) { dataCb_ = cb; }

  /** Pump the UART and deliver any received frames. Call often from loop(). */
  void poll();

 private:
  HardwareSerial* serial_;
  CC2530RxCallback rxCb_;
  CC2530DataCallback dataCb_;
  uint16_t version_;
  uint8_t channel_;
  uint8_t macSequence_;

  // incoming-frame parser
  uint8_t state_, len_, idx_, fcs_;
  uint8_t buf_[140];
  // captured command response (non-0x84 frames), consumed by waitResp()
  uint8_t respCmd_, respData_[8], respLen_;
  bool respReady_;

  void sendFrame(uint8_t cmd, const uint8_t* data, uint8_t n);
  void feed(uint8_t b);                 // parse one byte; dispatch on full frame
  bool waitResp(uint8_t cmd, uint32_t timeoutMs);
};

}  // namespace nzb

#endif  // ARDUINONRF_ZIGBEE_CC2530RADIO_H
