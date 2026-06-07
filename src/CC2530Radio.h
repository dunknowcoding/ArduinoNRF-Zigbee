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

using nzb::CC2530Radio;
using nzb::CC2530RxCallback;

#endif  // ARDUINONRF_ZIGBEE_PUBLIC_CC2530RADIO_H
