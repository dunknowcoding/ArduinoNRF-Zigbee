/*
  CC2530Znp_Info - bring up a CC2530 running TI Z-Stack ZNP and read its info.

  This is the counterpart to CC2530_Info, but for the ZNP backend: instead of the
  raw 802.15.4 SDCC firmware, the CC2530 must be flashed with Z-Stack ZNP
  firmware (built with IAR EW 8051, see extras/firmware/cc2530znp/BUILD.md). The
  nRF host drives it over the MT API.

  It resets the ZNP, pings it, prints the firmware version and device info, then
  starts the stack as a coordinator on a fixed PAN/channel and reports the state
  changes as the network forms. Wiring is the same UART as CC2530Radio (Serial1).
*/
#include <CC2530ZnpRadio.h>

using nzb::CC2530ZnpRadio;
using nzb::ZnpVersion;
using nzb::ZnpDeviceInfo;
using nzb::ZnpIncomingMsg;
using nzb::ZNP_COORDINATOR;

CC2530ZnpRadio znp(Serial1);

static const uint16_t PAN_ID = 0x1A62;
static const uint8_t CHANNEL = 15;
static const uint8_t HA_ENDPOINT = 1;
static const uint16_t HA_PROFILE = 0x0104;        // Home Automation
static const uint16_t CLUSTER_ON_OFF = 0x0006;

void onState(uint8_t state) {
  Serial.print("ZDO state change -> ");
  Serial.println(state);  // 9 = started as ZB coordinator
}

void onIncoming(const ZnpIncomingMsg& m) {
  Serial.print("AF rx cluster=0x"); Serial.print(m.clusterId, HEX);
  Serial.print(" from 0x"); Serial.print(m.srcAddr, HEX);
  Serial.print(" len="); Serial.println(m.len);
}

void onConfirm(uint8_t status, uint8_t endpoint, uint8_t transId) {
  Serial.print("AF data confirm ep="); Serial.print(endpoint);
  Serial.print(" trans="); Serial.print(transId);
  Serial.print(" status="); Serial.println(status);  // 0 = delivered
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("CC2530Znp_Info: bringing up Z-Stack ZNP...");

  znp.onStateChange(onState);
  znp.onIncoming(onIncoming);
  znp.onDataConfirm(onConfirm);

  if (!znp.begin()) {
    Serial.println("ERROR: ZNP did not respond (is ZNP firmware flashed + wired?)");
    return;
  }
  Serial.print("ping OK, capabilities=0x");
  Serial.println(znp.capabilities(), HEX);

  ZnpVersion v;
  if (znp.getVersion(v)) {
    Serial.print("ZNP version: product="); Serial.print(v.product);
    Serial.print(" rev "); Serial.print(v.major); Serial.print(".");
    Serial.print(v.minor); Serial.print("."); Serial.println(v.maint);
  }

  // Fresh form-up: clear state, set type/PAN/channel, then start.
  znp.setStartupOption(0x03);   // clear network + config state
  znp.reset();                  // apply the cleared state
  znp.begin();                  // re-ping after the reset
  znp.setLogicalType(ZNP_COORDINATOR);
  znp.setPanId(PAN_ID);
  znp.setChannel(CHANNEL);

  uint8_t status = 0xFF;
  if (znp.startupFromApp(0, status)) {
    Serial.print("ZDO startup requested, status="); Serial.println(status);
  } else {
    Serial.println("ZDO startup request FAILED");
  }

  ZnpDeviceInfo info;
  if (znp.getDeviceInfo(info)) {
    Serial.print("device: short=0x"); Serial.print(info.shortAddr, HEX);
    Serial.print(" type="); Serial.print(info.deviceType);
    Serial.print(" state="); Serial.println(info.deviceState);
  }

  // Register a Home Automation On/Off endpoint and open the network so other
  // devices can join this ZNP coordinator.
  const uint16_t clusters[] = {CLUSTER_ON_OFF};
  if (znp.registerEndpoint(HA_ENDPOINT, HA_PROFILE, 0x0050, 1, clusters, 1,
                           clusters, 1)) {
    Serial.println("AF endpoint 1 registered (HA On/Off)");
  }
  if (znp.permitJoin(60)) {
    Serial.println("network open for joining (60 s)");
  }
}

void loop() {
  znp.poll();  // surface ZDO state-change indications as the network forms
  delay(10);
}
