/*
  CC2530_KeyTransport - Zigbee Trust Center key-transport command tooling.

  A joining device gets the network key from the Trust Center inside an APS
  Transport-Key command, protected at the APS layer under a link key the
  joiner already holds (the default "ZigBeeAlliance09", or an install-code
  key). This sketch self-tests the key-transport command frames: Transport
  Key (network key), Request Key, and Switch Key, plus the default TC link
  key constant.

  The APS-layer AES-CCM* that encrypts a Transport-Key on air reuses the same
  CCM* primitive as ZigbeeSecurity (with an APS nonce/AAD); wiring that
  envelope into the join handshake is the integration step. The command
  frames here are what the envelope carries.
*/

#include <CC2530Radio.h>

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

void testTransportKey() {
  Serial.println("Transport-Key (network key):");
  ApsTransportKey t;
  t.keyType = APS_KEY_STANDARD_NETWORK;
  for (uint8_t i = 0; i < 16; ++i) t.key[i] = (uint8_t)(0xA0 + i);
  t.keySeqNumber = 3;
  t.destAddress = 0x1A62195E00000031ULL;  // the joiner
  t.srcAddress = 0x1A62195E00000000ULL;   // the Trust Center

  uint8_t buf[40];
  uint8_t n = ZigbeeApsKey::buildTransportNetworkKey(buf, sizeof(buf), t);
  check(n == 35, "Transport-Key is 35 bytes");
  check(buf[0] == APS_CMD_TRANSPORT_KEY && buf[1] == APS_KEY_STANDARD_NETWORK,
        "command id + key type");

  ApsTransportKey parsed;
  bool ok = ZigbeeApsKey::parseTransportNetworkKey(buf, n, parsed);
  check(ok, "parse Transport-Key");
  bool keyMatch = true;
  for (uint8_t i = 0; i < 16; ++i)
    if (parsed.key[i] != t.key[i]) keyMatch = false;
  check(keyMatch && parsed.keySeqNumber == t.keySeqNumber &&
            parsed.destAddress == t.destAddress &&
            parsed.srcAddress == t.srcAddress,
        "round-trip: key, seq, dst, src");
}

void testRequestAndSwitch() {
  Serial.println("Request-Key / Switch-Key:");

  ApsRequestKey r;
  r.keyType = APS_KEY_STANDARD_NETWORK;
  uint8_t buf[16];
  uint8_t n = ZigbeeApsKey::buildRequestKey(buf, sizeof(buf), r);
  check(n == 2, "Request network key is 2 bytes");
  ApsRequestKey pr;
  check(ZigbeeApsKey::parseRequestKey(buf, n, pr) &&
            pr.keyType == APS_KEY_STANDARD_NETWORK,
        "parse Request-Key (network)");

  r.keyType = APS_KEY_APP_LINK;
  r.partnerAddress = 0x1A62195E000000ABULL;
  n = ZigbeeApsKey::buildRequestKey(buf, sizeof(buf), r);
  check(n == 10, "Request app link key is 10 bytes (with partner)");
  check(ZigbeeApsKey::parseRequestKey(buf, n, pr) &&
            pr.keyType == APS_KEY_APP_LINK &&
            pr.partnerAddress == r.partnerAddress,
        "parse Request-Key (app link + partner)");

  uint8_t sw[4];
  uint8_t sn = ZigbeeApsKey::buildSwitchKey(sw, sizeof(sw), 7);
  uint8_t seq = 0;
  check(sn == 2 && ZigbeeApsKey::parseSwitchKey(sw, sn, seq) && seq == 7,
        "Switch-Key round-trip");
}

void testDefaultLinkKey() {
  Serial.println("Default TC link key:");
  const uint8_t* k = ZigbeeApsKey::defaultTcLinkKey();
  // "ZigBeeAlliance09"
  static const uint8_t expect[16] = {0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41,
                                     0x6C, 0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65,
                                     0x30, 0x39};
  bool match = true;
  for (uint8_t i = 0; i < 16; ++i) if (k[i] != expect[i]) match = false;
  check(match, "default key == \"ZigBeeAlliance09\"");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== NiusZigbee key-transport self-test ===");

  testTransportKey();
  testRequestAndSwitch();
  testDefaultLinkKey();

  Serial.print("RESULT: "); Serial.print(passes); Serial.print(" passed, ");
  Serial.print(fails); Serial.println(" failed");
}

void loop() { delay(1000); }
