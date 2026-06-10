/*
  ArduinoNRF_Zigbee.h - umbrella header for the ArduinoNRF-Zigbee library.

  A host-side driver collection for external Zigbee/802.15.4 radio modules driven
  by an ArduinoNRF (nRF52840) board over a hardware UART. Include the specific
  module you have - e.g. #include <CC2530Radio.h> - or include this header to pull
  in the framework plus every bundled module driver.

  Modules:
    * CC2530  - AliExpress CC2530 module running this library's SDCC 802.15.4
                transceiver firmware (extras/firmware/cc2530/). Raw 802.15.4.

  Flash the module's firmware with the ArduinoNRF board package's built-in
  CC-Debugger - see docs/FLASHING.md. Wiring: docs/WIRING.md.
*/
#ifndef ARDUINONRF_ZIGBEE_H
#define ARDUINONRF_ZIGBEE_H

#define ARDUINONRF_ZIGBEE_VERSION "0.1.7"

#include "ZigbeeModule.h"
#include "ZigbeeMac.h"
#include "ZigbeeNwk.h"
#include "ZigbeeAps.h"
#include "ZigbeeZcl.h"
#include "ZigbeeClusters.h"
#include "CC2530Radio.h"

#endif  // ARDUINONRF_ZIGBEE_H
