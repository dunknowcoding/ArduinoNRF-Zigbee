/*
  ZigbeeGpProxy.h - Green Power proxy: tunnel an overheard GPDF into the Zigbee
  network as a GP cluster command for the sink.

  A GP proxy is a mains-powered Zigbee node that hears a Green Power Data Frame
  (GPDF) on the raw 802.15.4 air and forwards it to a (possibly distant) GP sink
  over normal APS using the GP cluster (0x0021). This is what lets a battery-less
  switch control a light that is several hops away.

  This class turns a received raw GPDF into the right GP cluster payload:
    * a commissioning GPDF (command 0xE0)        -> GP Commissioning Notification
    * an operational GPDF (secured or unsecured) -> GP Notification
  For a secured operational frame the proxy does NOT need the GPD key: it forwards
  the ciphertext command + payload + the 4-byte MIC, and the sink verifies and
  decrypts (see ZigbeeGreenPowerCluster::handleNotification).

  A small proxy table gives per-GPD duplicate suppression (the same GPDF is often
  heard by several proxies / repeated by the GPD), keyed on the GPD frame counter.
*/
#ifndef NIUS_ZIGBEE_GP_PROXY_H
#define NIUS_ZIGBEE_GP_PROXY_H

#include <Arduino.h>

#include "ZigbeeGreenPowerCluster.h"

namespace nzb {

struct GpProxyEntry {
  bool used;
  uint32_t srcId;
  uint32_t lastFrameCounter;
};

class ZigbeeGpProxy {
 public:
  ZigbeeGpProxy() : entries_(nullptr), capacity_(0) {}

  void begin(GpProxyEntry* storage, uint8_t capacity) {
    entries_ = storage;
    capacity_ = capacity;
    for (uint8_t i = 0; i < capacity_; ++i) entries_[i] = GpProxyEntry();
  }

  /** Convert a received raw GPDF into a GP cluster payload to forward to the
      sink.
      @param gpdf/gpdfLen the raw GPDF bytes (as received off the air).
      @param gppShortAddr/gppLink this proxy's own short address + link quality,
             attached as proxy info.
      @param outClusterCmd out: which GP cluster command to send
             (GP_CMD_NOTIFICATION or GP_CMD_COMMISSIONING_NOTIFICATION).
      @param out/outMax the payload buffer.
      @return payload length, or 0 if dropped (duplicate / malformed / off-spec).
      A commissioning GPDF is always forwarded (its frame counter may be 0). */
  uint8_t forward(const uint8_t* gpdf, uint8_t gpdfLen, uint16_t gppShortAddr,
                  uint8_t gppLink, uint8_t& outClusterCmd, uint8_t* out,
                  uint8_t outMax) {
    GpdfFrame f;
    if (!ZigbeeGreenPower::parse(gpdf, gpdfLen, f)) return 0;
    if (f.applicationId != 0) return 0;  // only GPD source id supported

    bool commissioning =
        (f.securityLevel == GP_SEC_NONE && f.commandId == GPD_CMD_COMMISSIONING);

    // Duplicate suppression on the frame counter (skip for commissioning, whose
    // counter is often 0 and which must always reach the sink).
    if (!commissioning && f.securityLevel >= GP_SEC_FC_MIC) {
      if (isDuplicate(f.srcId, f.frameCounter)) return 0;
    }

    GpNotification n = GpNotification();
    n.applicationId = 0;
    n.securityLevel = f.securityLevel;
    n.rxAfterTx = f.rxAfterTx;
    n.srcId = f.srcId;
    n.frameCounter = f.frameCounter;
    n.proxyInfoPresent = true;
    n.gppShortAddr = gppShortAddr;
    n.gppLink = gppLink;

    if (commissioning) {
      // The commissioning command body is in the clear; parse() already exposed
      // command id + payload.
      n.secured = false;
      n.commandId = f.commandId;
      n.payloadLen = f.payloadLen;
      memcpy(n.payload, f.payload, f.payloadLen);
      outClusterCmd = GP_CMD_COMMISSIONING_NOTIFICATION;
      return ZigbeeGreenPowerCluster::buildNotification(out, outMax, n);
    }

    if (f.securityLevel == GP_SEC_ENC_FC_MIC) {
      // Secured operational frame: forward ciphertext + MIC; the sink decrypts.
      // GPDF layout: header(10) | cipher[commandId|payload] | MIC(4).
      if (gpdfLen < 10 + 1 + 4) return 0;
      uint8_t encLen = (uint8_t)(gpdfLen - 10 - 4);
      if (encLen < 1 || encLen > 48) return 0;
      n.secured = true;
      n.commandId = gpdf[10];                       // ciphertext command byte
      n.payloadLen = (uint8_t)(encLen - 1);
      memcpy(n.payload, &gpdf[11], n.payloadLen);
      memcpy(n.mic, &gpdf[gpdfLen - 4], 4);
      outClusterCmd = GP_CMD_NOTIFICATION;
      return ZigbeeGreenPowerCluster::buildNotification(out, outMax, n);
    }

    // Unsecured operational frame: forward command + payload in the clear.
    n.secured = false;
    n.commandId = f.commandId;
    n.payloadLen = f.payloadLen;
    memcpy(n.payload, f.payload, f.payloadLen);
    outClusterCmd = GP_CMD_NOTIFICATION;
    return ZigbeeGreenPowerCluster::buildNotification(out, outMax, n);
  }

 private:
  // Returns true (and updates the table) if (srcId, frameCounter) is a fresh
  // frame; returns true-as-"duplicate" if we have already forwarded it.
  bool isDuplicate(uint32_t srcId, uint32_t frameCounter) {
    GpProxyEntry* e = find(srcId);
    if (e) {
      if (frameCounter <= e->lastFrameCounter) return true;  // already seen
      e->lastFrameCounter = frameCounter;
      return false;
    }
    e = firstFree();
    if (!e) e = oldest();  // recycle when full
    if (!e) return false;
    e->used = true;
    e->srcId = srcId;
    e->lastFrameCounter = frameCounter;
    return false;
  }

  GpProxyEntry* find(uint32_t srcId) {
    for (uint8_t i = 0; i < capacity_; ++i)
      if (entries_[i].used && entries_[i].srcId == srcId) return &entries_[i];
    return nullptr;
  }
  GpProxyEntry* firstFree() {
    for (uint8_t i = 0; i < capacity_; ++i)
      if (!entries_[i].used) return &entries_[i];
    return nullptr;
  }
  GpProxyEntry* oldest() {
    GpProxyEntry* o = nullptr;
    for (uint8_t i = 0; i < capacity_; ++i) {
      if (!entries_[i].used) continue;
      if (!o || entries_[i].lastFrameCounter < o->lastFrameCounter)
        o = &entries_[i];
    }
    return o;
  }

  GpProxyEntry* entries_;
  uint8_t capacity_;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_GP_PROXY_H
