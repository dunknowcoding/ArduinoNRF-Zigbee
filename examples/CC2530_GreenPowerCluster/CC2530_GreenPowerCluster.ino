/*
  CC2530_GreenPowerCluster - Zigbee Green Power proxy + GP cluster (0x0021)
  self-test.

  A GP proxy overhears a battery-less Green Power device (GPD) on the raw
  802.15.4 air and tunnels it into the Zigbee network as a GP cluster command so
  a (possibly distant) GP sink can act on it. This sketch self-tests:
    * GP Notification / Commissioning Notification build + parse round trips
    * GP Pairing build + parse round trip
    * the proxy turning a commissioning GPDF into a Commissioning Notification,
      and the sink commissioning the GPD from it
    * the proxy forwarding a SECURED operational GPDF (ciphertext + MIC), and the
      sink decrypting it with the stored key + enforcing the replay counter
    * proxy-side duplicate suppression

  Runs on board1 via J-Link, no radio traffic (all on-chip crypto).
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

static const uint8_t GPD_KEY[16] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                                    0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};
static const uint32_t SRC_ID = 0x01234567;
static const uint16_t PROXY_ADDR = 0x1234;

void testNotificationRoundTrip() {
  Serial.println("GP Notification round trip:");
  GpNotification n = GpNotification();
  n.applicationId = 0;
  n.securityLevel = GP_SEC_ENC_FC_MIC;
  n.srcId = SRC_ID;
  n.frameCounter = 4242;
  n.commandId = 0x9A;        // ciphertext byte
  n.payload[0] = 0xDE; n.payload[1] = 0xAD; n.payloadLen = 2;
  n.secured = true;
  n.mic[0]=1; n.mic[1]=2; n.mic[2]=3; n.mic[3]=4;
  n.proxyInfoPresent = true;
  n.gppShortAddr = PROXY_ADDR;
  n.gppLink = 0xE0;

  uint8_t buf[48];
  uint8_t len = ZigbeeGreenPowerCluster::buildNotification(buf, sizeof(buf), n);
  check(len > 0, "build notification");

  GpNotification p;
  check(ZigbeeGreenPowerCluster::parseNotification(buf, len, p), "parse notification");
  check(p.srcId == SRC_ID && p.frameCounter == 4242 && p.secured &&
            p.commandId == 0x9A && p.payloadLen == 2 && p.payload[0] == 0xDE,
        "fields round-trip");
  check(p.proxyInfoPresent && p.gppShortAddr == PROXY_ADDR && p.gppLink == 0xE0,
        "proxy info round-trip");
  check(p.mic[0]==1 && p.mic[3]==4, "MIC round-trip");
}

void testPairingRoundTrip() {
  Serial.println("GP Pairing round trip:");
  GpPairing g = GpPairing();
  g.applicationId = 0;
  g.addSink = true;
  g.communicationMode = GP_COMM_LIGHTWEIGHT_UNICAST;
  g.securityLevel = GP_SEC_ENC_FC_MIC;
  g.srcId = SRC_ID;
  g.sinkNwkAddr = 0x0000;
  g.deviceId = 0x02;
  g.frameCounterPresent = true;
  g.frameCounter = 100;
  g.keyPresent = true;
  memcpy(g.key, GPD_KEY, 16);

  uint8_t buf[40];
  uint8_t len = ZigbeeGreenPowerCluster::buildPairing(buf, sizeof(buf), g);
  check(len > 0, "build pairing");

  GpPairing p;
  check(ZigbeeGreenPowerCluster::parsePairing(buf, len, p), "parse pairing");
  bool keyOk = true;
  for (uint8_t i = 0; i < 16; ++i) if (p.key[i] != GPD_KEY[i]) keyOk = false;
  check(p.srcId == SRC_ID && p.addSink && p.deviceId == 0x02 &&
            p.frameCounterPresent && p.frameCounter == 100 && p.keyPresent && keyOk,
        "fields + key round-trip");
}

void testCommissioningThroughProxy() {
  Serial.println("Commissioning GPDF -> proxy -> sink:");
  // GPD emits an unsecured commissioning GPDF carrying its key.
  GpCommissioningCommand c;
  c.deviceId = 0x02;
  c.options = 0x20;  // GPD key present
  c.keyPresent = true;
  memcpy(c.key, GPD_KEY, 16);
  c.outgoingCounterPresent = false;
  uint8_t cmd[24];
  uint8_t cn = ZigbeeGreenPower::buildCommissioning(cmd, sizeof(cmd), c);
  uint8_t gpdf[40];
  uint8_t gn = ZigbeeGreenPower::buildUnsecured(gpdf, sizeof(gpdf), SRC_ID,
                                                GPD_CMD_COMMISSIONING, cmd, cn);

  // Proxy tunnels it.
  GpProxyEntry pstore[4];
  ZigbeeGpProxy proxy;
  proxy.begin(pstore, 4);
  uint8_t clusterCmd = 0xFF;
  uint8_t apdu[48];
  uint8_t an = proxy.forward(gpdf, gn, PROXY_ADDR, 0xE0, clusterCmd, apdu,
                             sizeof(apdu));
  check(an > 0 && clusterCmd == GP_CMD_COMMISSIONING_NOTIFICATION,
        "proxy emits Commissioning Notification");

  // Sink commissions the GPD from the notification.
  GpSinkEntry sstore[4];
  ZigbeeGpSinkTable sink;
  sink.begin(sstore, 4);
  GpSinkEntry* e = nullptr;
  GpPairing pairing;
  check(ZigbeeGreenPowerCluster::handleCommissioningNotification(
            sink, apdu, an, &e, &pairing, 0x0000),
        "sink handles Commissioning Notification");
  check(e && e->srcId == SRC_ID && e->deviceId == 0x02, "GPD now in sink table");
  bool keyOk = true;
  for (uint8_t i = 0; i < 16; ++i) if (e->key[i] != GPD_KEY[i]) keyOk = false;
  check(keyOk, "GPD key stored");
  check(pairing.addSink && pairing.srcId == SRC_ID && pairing.keyPresent,
        "sink produced a GP Pairing to announce");
}

void testSecuredOperationalThroughProxy() {
  Serial.println("Secured operational GPDF -> proxy -> sink decrypt:");
  // Sink already knows the GPD.
  GpSinkEntry sstore[4];
  ZigbeeGpSinkTable sink;
  sink.begin(sstore, 4);
  sink.commission(SRC_ID, 0x02, GPD_KEY);

  GpProxyEntry pstore[4];
  ZigbeeGpProxy proxy;
  proxy.begin(pstore, 4);

  // GPD emits a secured Toggle (frame counter 200).
  uint8_t gpdf[24];
  uint8_t gn = ZigbeeGreenPower::secure(gpdf, sizeof(gpdf), GPD_KEY, SRC_ID,
                                        /*frameCounter=*/200, GPD_CMD_TOGGLE,
                                        nullptr, 0);

  uint8_t clusterCmd = 0xFF, apdu[48];
  uint8_t an = proxy.forward(gpdf, gn, PROXY_ADDR, 0xC8, clusterCmd, apdu,
                             sizeof(apdu));
  check(an > 0 && clusterCmd == GP_CMD_NOTIFICATION,
        "proxy emits GP Notification (secured passthrough)");

  uint8_t outCmd = 0, outPayload[48], outLen = 0;
  bool ok = ZigbeeGreenPowerCluster::handleNotification(
      sink, apdu, an, outCmd, outPayload, sizeof(outPayload), outLen);
  check(ok && outCmd == GPD_CMD_TOGGLE, "sink decrypts Toggle from notification");

  // Replay of the SAME secured frame must be rejected (counter not advanced).
  uint8_t clusterCmd2 = 0xFF, apdu2[48];
  uint8_t an2 = proxy.forward(gpdf, gn, PROXY_ADDR, 0xC8, clusterCmd2, apdu2,
                              sizeof(apdu2));
  check(an2 == 0, "proxy drops a duplicate (same counter)");

  // A forged notification (advance counter past the proxy, but wrong MIC) is
  // rejected by the sink.
  GpNotification bad;
  ZigbeeGreenPowerCluster::parseNotification(apdu, an, bad);
  bad.frameCounter = 201;
  bad.mic[0] ^= 0xFF;
  uint8_t badApdu[48];
  uint8_t bn = ZigbeeGreenPowerCluster::buildNotification(badApdu, sizeof(badApdu), bad);
  uint8_t oc=0, op[48], ol=0;
  check(!ZigbeeGreenPowerCluster::handleNotification(sink, badApdu, bn, oc, op,
                                                     sizeof(op), ol),
        "sink rejects a tampered notification (MIC fail)");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee Green Power cluster (0x0021) + proxy self-test ===");

  testNotificationRoundTrip();
  testPairingRoundTrip();
  testCommissioningThroughProxy();
  testSecuredOperationalThroughProxy();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
