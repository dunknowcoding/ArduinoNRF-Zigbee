/*
  CC2530_SourceRouteSecurity - self-test for SECURED source-route relaying.

  Source-routed NWK frames carry a relay-index byte that every hop rewrites to
  point at the next relay. For such a frame to be NWK-secured on air, that
  mutable byte must be excluded from the CCM* additional authenticated data
  (AAD) - otherwise bumping it at a relay would invalidate the MIC. NiusZigbee
  does this in secureNpdu()/openNpdu() (srRelayIndexOffset), and the radio
  exposes it through sendNwkDataSourceRouted().

  This test proves the mechanism on silicon, without radio traffic:
    1. round-trip: secure a source-routed frame, open it, recover the payload;
    2. AAD exclusion: bump the relay-index byte in the *already secured* frame
       and confirm it STILL opens (a relay can update the index without
       re-encrypting) - the heart of secured source-route OTA;
    3. per-hop re-encrypt: rebuild with the advanced index and re-secure under a
       different node's aux header, then open - the path a real relay takes;
    4. integrity: tampering a PROTECTED header byte (relay count) or the
       ciphertext is rejected (MIC fail);
    5. replay: re-opening the same (sender, counter) is rejected.

  Runs on board1 via J-Link.
*/

#include <ZigbeeSecurity.h>
#include <ZigbeeNwk.h>

using namespace nzb;

uint32_t passes = 0, fails = 0;
void check(bool ok, const char* what) {
  Serial.print(ok ? "  PASS " : "  FAIL ");
  Serial.println(what);
  if (ok) ++passes; else ++fails;
}

static ZigbeeSecurity sec;
static const uint8_t kNwkKey[16] = {0x41, 0x6c, 0x6c, 0x69, 0x61, 0x6e, 0x63, 0x65,
                                    0x30, 0x39, 0x4b, 0x65, 0x79, 0x21, 0x21, 0x21};
static const uint64_t kIeeeA = 0x00124B0001020304ULL;  // originator
static const uint64_t kIeeeB = 0x00124B00050607F0ULL;  // a relay

// Basic source-routed frame: 2 relays, index 0. Header = base(8) + SR(2 + 2*2).
static const uint16_t kRelays[2] = {0x0002, 0x0003};
static const uint8_t kRelayCount = 2;
static const uint8_t kHeaderLen = 8 + 2 + 2 * kRelayCount;  // 14
static const uint8_t kRelayIndexOff = 9;                    // base(8) + relayCount(1)

static uint8_t buildSr(uint8_t* out, uint8_t outMax, uint8_t relayIndex,
                       const char* msg, uint8_t msgLen) {
  return ZigbeeNwk::buildDataFrameSourceRouted(
      out, outMax, /*dst*/ 0x0004, /*src*/ 0x0000, ZigbeeNwk::kDefaultRadius,
      /*seq*/ 0x42, kRelays, kRelayCount, relayIndex, (const uint8_t*)msg, msgLen);
}

static bool payloadMatches(const uint8_t* plain, uint8_t plainLen,
                           const char* msg, uint8_t msgLen) {
  if (plainLen != kHeaderLen + msgLen) return false;
  for (uint8_t i = 0; i < msgLen; ++i)
    if (plain[kHeaderLen + i] != (uint8_t)msg[i]) return false;
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 4000) {
  }
  sec.setNetworkKey(kNwkKey, 0);

  Serial.println();
  Serial.println("=== NiusZigbee secured source-route self-test ===");

  const char msg[] = "SR-secured";
  const uint8_t msgLen = 10;

  // Build + secure the originator frame (relay index 0).
  uint8_t plain[64];
  uint8_t plainLen = buildSr(plain, sizeof(plain), 0, msg, msgLen);
  check(plainLen == kHeaderLen + msgLen, "build source-routed frame");
  check((plain[1] & 0x04) == 0x04, "source-route FCF bit set");  // FCF bit 10

  uint8_t secured[96];
  uint8_t securedLen =
      sec.secureNpdu(plain, plainLen, kHeaderLen, kIeeeA, 1, secured, sizeof(secured));
  check(securedLen > plainLen, "secure (aux + MIC added)");

  // 1. Round trip.
  uint8_t opened[96];
  sec.resetReplayTable();
  uint8_t openedLen = sec.openNpdu(secured, securedLen, kHeaderLen, opened, sizeof(opened));
  check(openedLen > 0, "open secured frame (MIC ok)");
  check(payloadMatches(opened, openedLen, msg, msgLen), "payload recovered");

  // 2. AAD exclusion: bump the relay-index byte in the SECURED frame, no
  //    re-encryption, and confirm it still opens. This is what lets a relay
  //    forward a secured source-routed frame.
  check(secured[kRelayIndexOff] == 0, "secured relay index starts at 0");
  secured[kRelayIndexOff] = 1;  // a relay advances the index
  sec.resetReplayTable();
  openedLen = sec.openNpdu(secured, securedLen, kHeaderLen, opened, sizeof(opened));
  check(openedLen > 0 && payloadMatches(opened, openedLen, msg, msgLen),
        "relay-index bump still opens (AAD-excluded)");
  check(opened[kRelayIndexOff] == 1, "opened frame carries the bumped index");

  // 3. Per-hop re-encrypt: a relay rebuilds with the advanced index and
  //    re-secures under its own aux header; the next node opens it.
  uint8_t relayPlain[64];
  uint8_t relayPlainLen = buildSr(relayPlain, sizeof(relayPlain), 1, msg, msgLen);
  uint8_t relaySecured[96];
  uint8_t relaySecuredLen = sec.secureNpdu(relayPlain, relayPlainLen, kHeaderLen,
                                           kIeeeB, 1, relaySecured, sizeof(relaySecured));
  check(relaySecuredLen > 0, "relay re-secures (own aux)");
  sec.resetReplayTable();
  openedLen = sec.openNpdu(relaySecured, relaySecuredLen, kHeaderLen, opened, sizeof(opened));
  check(openedLen > 0 && payloadMatches(opened, openedLen, msg, msgLen),
        "re-secured frame opens at next hop");

  // 4a. Tamper a PROTECTED header byte (the relay count at offset 8) -> reject.
  uint8_t tampered[96];
  memcpy(tampered, secured, securedLen);
  tampered[kRelayIndexOff] = 0;  // restore index
  tampered[8] ^= 0x01;           // corrupt the relay COUNT (in the AAD)
  sec.resetReplayTable();
  check(sec.openNpdu(tampered, securedLen, kHeaderLen, opened, sizeof(opened)) == 0,
        "tampered relay-count rejected (MIC fail)");

  // 4b. Tamper a ciphertext byte -> reject.
  memcpy(tampered, secured, securedLen);
  tampered[kRelayIndexOff] = 0;
  tampered[securedLen - 1] ^= 0x80;  // corrupt the last MIC/ciphertext byte
  sec.resetReplayTable();
  check(sec.openNpdu(tampered, securedLen, kHeaderLen, opened, sizeof(opened)) == 0,
        "tampered ciphertext/MIC rejected");

  // 5. Replay: re-open the same (sender, counter) without reset -> rejected.
  memcpy(tampered, secured, securedLen);
  tampered[kRelayIndexOff] = 0;
  sec.resetReplayTable();
  check(sec.openNpdu(tampered, securedLen, kHeaderLen, opened, sizeof(opened)) > 0,
        "first open accepted");
  check(sec.openNpdu(tampered, securedLen, kHeaderLen, opened, sizeof(opened)) == 0,
        "replay of same counter rejected");

  Serial.println("-------------------------------------------------");
  Serial.print("RESULT: ");
  Serial.print(passes);
  Serial.print(" passed, ");
  Serial.print(fails);
  Serial.print(" failed -> ");
  Serial.println(fails == 0 ? "ALL PASS" : "FAILURES");
}

void loop() {
  delay(1000);
}
