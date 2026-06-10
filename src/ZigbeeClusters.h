/*
  ZigbeeClusters.h - tiny reusable cluster behaviors for NiusZigbee.

  These classes are deliberately small. They help sketches answer common ZCL
  frames, but they are not a complete Zigbee Cluster Library runtime.
*/
#ifndef NIUS_ZIGBEE_CLUSTERS_H
#define NIUS_ZIGBEE_CLUSTERS_H

#include <Arduino.h>
#include "ZigbeeZcl.h"

namespace nzb {

class ZigbeeOnOffCluster {
 public:
  explicit ZigbeeOnOffCluster(bool initialState = false);

  bool isOn() const { return on_; }
  void setOn(bool on) { on_ = on; }
  bool applyCommand(uint8_t commandId) {
    return ZigbeeZcl::applyOnOffCommand(commandId, on_);
  }

  uint8_t buildReadAttributesResponse(const ZclFrame& request,
                                      uint8_t* out, uint8_t outMax) const;
  uint8_t handleFrame(const ZclFrame& request, uint8_t* out,
                      uint8_t outMax);

 private:
  bool on_;

  uint8_t appendReadAttributeRecord(uint16_t attrId, uint8_t* out,
                                    uint8_t outMax) const;
};

class ZigbeeBasicCluster {
 public:
  ZigbeeBasicCluster();

  void setVersions(uint8_t zclVersion, uint8_t applicationVersion,
                   uint8_t stackVersion, uint8_t hardwareVersion);
  void setIdentity(const char* manufacturerName, const char* modelIdentifier,
                   const char* dateCode = "");
  void setPowerSource(uint8_t powerSource) { powerSource_ = powerSource; }

  uint8_t buildReadAttributesResponse(const ZclFrame& request,
                                      uint8_t* out, uint8_t outMax) const;
  uint8_t handleFrame(const ZclFrame& request, uint8_t* out,
                      uint8_t outMax) const;

 private:
  uint8_t zclVersion_;
  uint8_t applicationVersion_;
  uint8_t stackVersion_;
  uint8_t hardwareVersion_;
  uint8_t powerSource_;
  const char* manufacturerName_;
  const char* modelIdentifier_;
  const char* dateCode_;

  uint8_t appendReadAttributeRecord(uint16_t attrId, uint8_t* out,
                                    uint8_t outMax) const;
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_CLUSTERS_H
