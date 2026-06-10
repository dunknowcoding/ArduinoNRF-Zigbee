/*
  CC2530Radio.h - public entry point for the CC2530 driver.

  Forwards to the driver under src/modules/CC2530/ and lifts the class into the
  global namespace so sketches can simply write:

      #include <CC2530Radio.h>
      CC2530Radio radio;            // uses Serial1 (D0/D1) by default
*/
#ifndef ARDUINONRF_ZIGBEE_PUBLIC_CC2530RADIO_H
#define ARDUINONRF_ZIGBEE_PUBLIC_CC2530RADIO_H

#include "modules/CC2530/CC2530Radio.h"
#include "ZigbeeClusters.h"

using nzb::CC2530Radio;
using nzb::CC2530RxCallback;
using nzb::CC2530DataCallback;
using nzb::CC2530NwkCallback;
using nzb::CC2530ApsCallback;
using nzb::CC2530ZclCallback;
using nzb::MacDataFrame;
using nzb::NwkDataFrame;
using nzb::ApsDataFrame;
using nzb::ZclFrame;
using nzb::ZigbeeMac;
using nzb::ZigbeeNwk;
using nzb::ZigbeeAps;
using nzb::ZigbeeZcl;
using nzb::ZigbeeOnOffCluster;
using nzb::ZigbeeBasicCluster;

#endif  // ARDUINONRF_ZIGBEE_PUBLIC_CC2530RADIO_H
