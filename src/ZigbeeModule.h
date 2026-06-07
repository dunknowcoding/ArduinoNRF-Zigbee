/*
  ZigbeeModule.h - the extension contract for ArduinoNRF-Zigbee.

  This library is a HOST-side driver collection: an ArduinoNRF (nRF52840) board is
  the host, and an external radio module (CC2530 today; CC2530+Z-Stack, CC2350,
  ... in future) does the actual 2.4 GHz work, reached over a hardware UART.

  Two layers of capability are anticipated, so drivers declare what they offer:

    * RAW 802.15.4 (PHY/MAC frames, sniffing) - implemented by the SDCC
      transceiver firmware. See IEEE802154Radio below and CC2530Radio.
    * ZIGBEE PRO (network join, endpoints, ZCL) - to be implemented by a future
      Z-Stack/ZNP-backed driver. It will add its own interface alongside this one.

  HOW TO ADD A NEW MODULE
  -----------------------
    1. Drop the host driver in   src/modules/<NAME>/<NAME>Radio.{h,cpp}
    2. Put its device firmware in extras/firmware/<name>/  (source + prebuilt .hex)
    3. Add a one-line forwarder  src/<NAME>Radio.h  ->  the module header
    4. Add example(s) under      examples/<NAME>_*/
    5. If it speaks raw 802.15.4, implement IEEE802154Radio so sketches written
       against the interface work unchanged across modules.

  Flashing the module's firmware uses the ArduinoNRF board package's built-in
  CC-Debugger (no external TI CC-Debugger needed) - see docs/FLASHING.md.
*/
#ifndef ARDUINONRF_ZIGBEE_ZIGBEEMODULE_H
#define ARDUINONRF_ZIGBEE_ZIGBEEMODULE_H

#include <Arduino.h>

namespace nzb {

/**
 * Common abstract interface for raw IEEE 802.15.4 radio modules. Optional: a
 * driver may implement it so application code can target any conforming module.
 * (CC2530Radio offers the same methods; it can be used directly or via this.)
 */
class IEEE802154Radio {
 public:
  virtual ~IEEE802154Radio() {}
  virtual bool begin(uint8_t channel = 11) = 0;
  virtual bool setChannel(uint8_t channel) = 0;   // 11..26
  virtual bool send(const uint8_t* payload, uint8_t len) = 0;
  virtual void poll() = 0;
};

}  // namespace nzb

#endif  // ARDUINONRF_ZIGBEE_ZIGBEEMODULE_H
