/*
  ZigbeeGreenPowerCluster.h - the Green Power cluster (0x0021): the network-side
  commands a GP proxy and a GP sink exchange over a normal Zigbee APS frame.

  Green Power devices (GPDs) talk on raw 802.15.4 (see ZigbeeGreenPower.h). A GP
  PROXY is a mains node that overhears a GPDF and tunnels it into the Zigbee
  network as a GP cluster command so a (possibly distant) GP SINK can act on it.
  This is what makes battery-less switches work across a whole mesh, not just
  next to the sink.

  This header builds/parses the core GP cluster (0x0021) commands on the GP
  endpoint (242) / ZGP profile (0xA1E0):

    proxy -> sink (client to server):
      * GP Notification              (0x00) - an operational GPDF was heard
      * GP Commissioning Notification(0x04) - a commissioning GPDF was heard

    sink -> proxy (server to client):
      * GP Pairing                   (0x01) - the sink announces a GPD<->sink
                                              pairing (key, counter, sink addr)

  It also provides server-side handlers that drive a ZigbeeGpSinkTable: commission
  a GPD from a Commissioning Notification, and decrypt/replay-check an operational
  Notification using the stored key.

  Encoding note: like the rest of NiusZigbee's GP/inter-PAN tooling, the option
  bitfields below follow the spec's common layout but are this library's own
  compact encoding; confirm against a certified reference vector before
  interoperating with third-party GP devices. For a SECURED notification the
  proxy forwards the GPD command id + payload as ciphertext plus the 4-byte MIC,
  and the sink reconstructs the original secured GPDF (the deterministic header
  from ZigbeeGreenPower::secure) to verify+decrypt it - so the AAD/MIC integrity
  is preserved end to end.
*/
#ifndef NIUS_ZIGBEE_GREEN_POWER_CLUSTER_H
#define NIUS_ZIGBEE_GREEN_POWER_CLUSTER_H

#include <Arduino.h>

#include "ZigbeeGreenPower.h"

namespace nzb {

// GP cluster command ids (direction-specific; duplicate values across directions
// are intentional and match the spec).
enum GpClusterClientCommand : uint8_t {  // proxy -> sink
  GP_CMD_NOTIFICATION = 0x00,
  GP_CMD_PAIRING_SEARCH = 0x01,
  GP_CMD_COMMISSIONING_NOTIFICATION = 0x04,
};
enum GpClusterServerCommand : uint8_t {  // sink -> proxy
  GP_CMD_PAIRING = 0x01,
  GP_CMD_PROXY_COMMISSIONING_MODE = 0x02,
  GP_CMD_RESPONSE = 0x06,
};

// GP communication modes (how the sink wants the GPD delivered).
enum GpCommunicationMode : uint8_t {
  GP_COMM_FULL_UNICAST = 0,
  GP_COMM_GROUP_DERIVED = 1,
  GP_COMM_GROUP_PRECOMMISSIONED = 2,
  GP_COMM_LIGHTWEIGHT_UNICAST = 3,
};

// A GP Notification / Commissioning Notification, decoded.
struct GpNotification {
  uint8_t applicationId;   // 0 = GPD source id
  uint8_t securityLevel;   // GpSecurityLevel
  uint8_t securityKeyType;
  bool rxAfterTx;
  uint32_t srcId;
  uint32_t frameCounter;
  uint8_t commandId;       // GPD command (ciphertext byte if `secured`)
  uint8_t payload[48];
  uint8_t payloadLen;
  bool secured;            // commandId+payload are ciphertext; `mic` is valid
  uint8_t mic[4];
  bool proxyInfoPresent;
  uint16_t gppShortAddr;
  uint8_t gppLink;
};

// A GP Pairing, decoded.
struct GpPairing {
  uint8_t applicationId;
  bool addSink;
  bool removeGpd;
  uint8_t communicationMode;
  uint8_t securityLevel;
  uint8_t securityKeyType;
  uint32_t srcId;
  uint16_t sinkNwkAddr;
  uint8_t deviceId;
  bool frameCounterPresent;
  uint32_t frameCounter;
  bool keyPresent;
  uint8_t key[16];
};

class ZigbeeGreenPowerCluster {
 public:
  static const uint16_t kCluster = 0x0021;   // Green Power cluster id
  static const uint16_t kProfileZgp = 0xA1E0;  // ZGP profile id
  static const uint8_t kEndpoint = 242;      // GP endpoint

  // ----------------------------------------- GP (Commissioning) Notification
  /** Build a GP Notification / Commissioning Notification payload (the same wire
      shape for both; the ZCL command id chooses which). Layout (LE):
        options(2) srcId(4) frameCounter(4) commandId(1) payloadLen(1)
        payload(n) [mic(4) if secured] [gppShort(2) gppLink(1) if proxyInfo].
      @return payload length, or 0 on error. */
  static uint8_t buildNotification(uint8_t* out, uint8_t outMax,
                                   const GpNotification& n) {
    if (n.payloadLen > sizeof(n.payload)) return 0;
    uint8_t need = (uint8_t)(2 + 4 + 4 + 1 + 1 + n.payloadLen +
                             (n.secured ? 4 : 0) +
                             (n.proxyInfoPresent ? 3 : 0));
    if (!out || outMax < need) return 0;

    uint16_t opt = (uint16_t)((n.applicationId & 0x07) |
                              ((n.securityLevel & 0x03) << 3) |
                              ((n.securityKeyType & 0x03) << 5) |
                              (n.rxAfterTx ? 0x80 : 0) |
                              (n.secured ? 0x100 : 0) |
                              (n.proxyInfoPresent ? 0x200 : 0));
    writeLe16(&out[0], opt);
    writeLe32(&out[2], n.srcId);
    writeLe32(&out[6], n.frameCounter);
    out[10] = n.commandId;
    out[11] = n.payloadLen;
    uint8_t idx = 12;
    for (uint8_t i = 0; i < n.payloadLen; ++i) out[idx++] = n.payload[i];
    if (n.secured) { memcpy(&out[idx], n.mic, 4); idx += 4; }
    if (n.proxyInfoPresent) {
      writeLe16(&out[idx], n.gppShortAddr); idx += 2;
      out[idx++] = n.gppLink;
    }
    return idx;
  }

  static bool parseNotification(const uint8_t* p, uint8_t len,
                                GpNotification& n) {
    n = GpNotification();
    if (!p || len < 12) return false;
    uint16_t opt = readLe16(&p[0]);
    n.applicationId = (uint8_t)(opt & 0x07);
    n.securityLevel = (uint8_t)((opt >> 3) & 0x03);
    n.securityKeyType = (uint8_t)((opt >> 5) & 0x03);
    n.rxAfterTx = (opt & 0x80) != 0;
    n.secured = (opt & 0x100) != 0;
    n.proxyInfoPresent = (opt & 0x200) != 0;
    n.srcId = readLe32(&p[2]);
    n.frameCounter = readLe32(&p[6]);
    n.commandId = p[10];
    n.payloadLen = p[11];
    if (n.payloadLen > sizeof(n.payload)) return false;
    uint8_t idx = 12;
    if ((uint8_t)(idx + n.payloadLen) > len) return false;
    memcpy(n.payload, &p[idx], n.payloadLen);
    idx += n.payloadLen;
    if (n.secured) {
      if ((uint8_t)(idx + 4) > len) return false;
      memcpy(n.mic, &p[idx], 4); idx += 4;
    }
    if (n.proxyInfoPresent) {
      if ((uint8_t)(idx + 3) > len) return false;
      n.gppShortAddr = readLe16(&p[idx]); idx += 2;
      n.gppLink = p[idx++];
    }
    return true;
  }

  // ----------------------------------------------------------- GP Pairing
  /** Build a GP Pairing payload (LE): options(3) srcId(4) sinkNwk(2) deviceId(1)
      [frameCounter(4)] [key(16)]. */
  static uint8_t buildPairing(uint8_t* out, uint8_t outMax, const GpPairing& g) {
    uint8_t need = (uint8_t)(3 + 4 + 2 + 1 + (g.frameCounterPresent ? 4 : 0) +
                             (g.keyPresent ? 16 : 0));
    if (!out || outMax < need) return 0;
    uint32_t opt = (uint32_t)((g.applicationId & 0x07) |
                              (g.addSink ? 0x08 : 0) |
                              (g.removeGpd ? 0x10 : 0) |
                              ((g.communicationMode & 0x03) << 5) |
                              ((uint32_t)(g.securityLevel & 0x03) << 7) |
                              ((uint32_t)(g.securityKeyType & 0x07) << 9) |
                              (g.frameCounterPresent ? 0x1000UL : 0) |
                              (g.keyPresent ? 0x2000UL : 0));
    out[0] = (uint8_t)(opt & 0xFF);
    out[1] = (uint8_t)((opt >> 8) & 0xFF);
    out[2] = (uint8_t)((opt >> 16) & 0xFF);
    writeLe32(&out[3], g.srcId);
    writeLe16(&out[7], g.sinkNwkAddr);
    out[9] = g.deviceId;
    uint8_t idx = 10;
    if (g.frameCounterPresent) { writeLe32(&out[idx], g.frameCounter); idx += 4; }
    if (g.keyPresent) { memcpy(&out[idx], g.key, 16); idx += 16; }
    return idx;
  }

  static bool parsePairing(const uint8_t* p, uint8_t len, GpPairing& g) {
    g = GpPairing();
    if (!p || len < 10) return false;
    uint32_t opt = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
    g.applicationId = (uint8_t)(opt & 0x07);
    g.addSink = (opt & 0x08) != 0;
    g.removeGpd = (opt & 0x10) != 0;
    g.communicationMode = (uint8_t)((opt >> 5) & 0x03);
    g.securityLevel = (uint8_t)((opt >> 7) & 0x03);
    g.securityKeyType = (uint8_t)((opt >> 9) & 0x07);
    g.frameCounterPresent = (opt & 0x1000UL) != 0;
    g.keyPresent = (opt & 0x2000UL) != 0;
    g.srcId = readLe32(&p[3]);
    g.sinkNwkAddr = readLe16(&p[7]);
    g.deviceId = p[9];
    uint8_t idx = 10;
    if (g.frameCounterPresent) {
      if ((uint8_t)(idx + 4) > len) return false;
      g.frameCounter = readLe32(&p[idx]); idx += 4;
    }
    if (g.keyPresent) {
      if ((uint8_t)(idx + 16) > len) return false;
      memcpy(g.key, &p[idx], 16); idx += 16;
    }
    return true;
  }

  // -------------------------------------------------------- server handlers
  /** Commission a GPD from a received GP Commissioning Notification: parse the
      embedded GP commissioning command (0xE0) for the device id + key and store
      it in @p sink. Optionally fills @p outPairing so the sink can announce the
      pairing to proxies. @return true on success. */
  static bool handleCommissioningNotification(ZigbeeGpSinkTable& sink,
                                              const uint8_t* payload,
                                              uint8_t len,
                                              GpSinkEntry** outEntry = nullptr,
                                              GpPairing* outPairing = nullptr,
                                              uint16_t sinkNwkAddr = 0) {
    GpNotification n;
    if (!parseNotification(payload, len, n)) return false;
    if (n.commandId != GPD_CMD_COMMISSIONING) return false;
    GpCommissioningCommand c;
    if (!ZigbeeGreenPower::parseCommissioning(n.payload, n.payloadLen, c))
      return false;
    if (!c.keyPresent) return false;
    GpSinkEntry* e = sink.commission(n.srcId, c.deviceId, c.key);
    if (!e) return false;
    if (outEntry) *outEntry = e;
    if (outPairing) {
      GpPairing g = GpPairing();
      g.applicationId = 0;
      g.addSink = true;
      g.communicationMode = GP_COMM_LIGHTWEIGHT_UNICAST;
      g.securityLevel = GP_SEC_ENC_FC_MIC;
      g.srcId = n.srcId;
      g.sinkNwkAddr = sinkNwkAddr;
      g.deviceId = c.deviceId;
      g.frameCounterPresent = c.outgoingCounterPresent;
      g.frameCounter = c.outgoingCounter;
      g.keyPresent = true;
      memcpy(g.key, c.key, 16);
      *outPairing = g;
    }
    return true;
  }

  /** Process an operational GP Notification at the sink: look up the GPD, decrypt
      it if secured (using the stored key), enforce the frame-counter anti-replay,
      and output the recovered GPD command. @return true if the frame is accepted
      (known GPD, MIC ok, counter advanced). */
  static bool handleNotification(ZigbeeGpSinkTable& sink, const uint8_t* payload,
                                 uint8_t len, uint8_t& outCommandId,
                                 uint8_t* outPayload, uint8_t outMax,
                                 uint8_t& outPayloadLen) {
    outPayloadLen = 0;
    GpNotification n;
    if (!parseNotification(payload, len, n)) return false;
    GpSinkEntry* e = sink.find(n.srcId);
    if (!e) return false;

    if (n.secured) {
      // Rebuild the original secured GPDF (the deterministic layout produced by
      // ZigbeeGreenPower::secure) so the MIC/AAD verify exactly, then open it.
      uint8_t encLen = (uint8_t)(1 + n.payloadLen);  // commandId + payload
      if (encLen < 1 || encLen > 48) return false;
      uint8_t buf[2 + 4 + 4 + 48 + 4];
      buf[0] = (uint8_t)((3 << 2) | 0x80);  // proto v3 + nwk FC extension
      buf[1] = (uint8_t)((3 << 3) | (n.rxAfterTx ? 0x40 : 0));  // appId 0, secL 3
      writeLe32(&buf[2], n.srcId);
      writeLe32(&buf[6], n.frameCounter);
      buf[10] = n.commandId;                 // ciphertext command byte
      memcpy(&buf[11], n.payload, n.payloadLen);
      memcpy(&buf[10 + encLen], n.mic, 4);
      uint8_t total = (uint8_t)(10 + encLen + 4);
      GpdfFrame f;
      if (!ZigbeeGreenPower::open(buf, total, e->key, f)) return false;
      if (!sink.checkAndUpdateCounter(n.srcId, f.frameCounter)) return false;
      outCommandId = f.commandId;
      if (f.payloadLen > outMax) return false;
      memcpy(outPayload, f.payload, f.payloadLen);
      outPayloadLen = f.payloadLen;
      return true;
    }

    // Unsecured operational frame: counter check (if any) then pass through.
    if (n.frameCounter != 0 &&
        !sink.checkAndUpdateCounter(n.srcId, n.frameCounter))
      return false;
    outCommandId = n.commandId;
    if (n.payloadLen > outMax) return false;
    memcpy(outPayload, n.payload, n.payloadLen);
    outPayloadLen = n.payloadLen;
    return true;
  }

 private:
  static void writeLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8);
  }
  static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  }
  static void writeLe32(uint8_t* p, uint32_t v) {
    for (uint8_t i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
  }
  static uint32_t readLe32(const uint8_t* p) {
    uint32_t v = 0;
    for (uint8_t i = 0; i < 4; ++i) v |= (uint32_t)p[i] << (8 * i);
    return v;
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_GREEN_POWER_CLUSTER_H
