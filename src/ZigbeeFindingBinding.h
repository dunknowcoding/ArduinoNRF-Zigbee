/*
  ZigbeeFindingBinding.h - Zigbee 3.0 BDB Finding & Binding.

  Finding & Binding is how a controller (the initiator - e.g. a wall switch)
  is paired to a target (e.g. a light) without a hub picking endpoints by hand.
  The target is put into Identify mode; the initiator finds it (Identify Query),
  reads its Simple Descriptor (its server/input clusters), and for each of the
  initiator's CLIENT (output) clusters that the target offers as a SERVER
  (input) cluster, creates a binding from the initiator's endpoint+cluster to
  the target's IEEE address + endpoint. After that the switch's commands reach
  the light directly.

  This header is the matching + bind-creation core: it pairs the initiator's
  output clusters against the target's input clusters and writes the resulting
  bindings into a ZigbeeBindingTable. The Identify trigger, Simple Descriptor
  exchange (ZigbeeZdo), and the binding store (ZigbeeBindingTable) already exist.
*/
#ifndef NIUS_ZIGBEE_FINDING_BINDING_H
#define NIUS_ZIGBEE_FINDING_BINDING_H

#include <Arduino.h>

#include "ZigbeeBindingTable.h"

namespace nzb {

class ZigbeeFindingBinding {
 public:
  /** Compute the clusters to bind: the initiator's output (client) clusters
      that the target also offers as input (server) clusters. Writes the
      matching cluster ids to @p matched. @return the number matched. */
  static uint8_t matchClusters(const uint16_t* myOutputClusters,
                               uint8_t myOutputCount,
                               const uint16_t* targetInputClusters,
                               uint8_t targetInputCount, uint16_t* matched,
                               uint8_t maxMatched) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < myOutputCount && n < maxMatched; ++i) {
      for (uint8_t j = 0; j < targetInputCount; ++j) {
        if (myOutputClusters[i] == targetInputClusters[j]) {
          // Skip a duplicate match for the same cluster.
          bool already = false;
          for (uint8_t k = 0; k < n; ++k)
            if (matched[k] == myOutputClusters[i]) already = true;
          if (!already) matched[n++] = myOutputClusters[i];
          break;
        }
      }
    }
    return n;
  }

  /** Create the bindings for a finished match: for each cluster in @p matched,
      add an IEEE binding from (srcIeee, srcEndpoint, cluster) to (targetIeee,
      targetEndpoint) in @p table. @return how many bindings were added (an
      existing identical binding is idempotent and still counts as present).
      Returns the count actually stored; stops if the table fills. */
  static uint8_t createBindings(ZigbeeBindingTable& table, uint64_t srcIeee,
                                uint8_t srcEndpoint, const uint16_t* matched,
                                uint8_t matchedCount, uint64_t targetIeee,
                                uint8_t targetEndpoint) {
    uint8_t added = 0;
    for (uint8_t i = 0; i < matchedCount; ++i) {
      ZigbeeBinding b;
      b.used = true;
      b.srcIeee = srcIeee;
      b.srcEndpoint = srcEndpoint;
      b.clusterId = matched[i];
      b.dstAddrMode = ZB_BIND_ADDR_IEEE;
      b.dstGroup = 0;
      b.dstIeee = targetIeee;
      b.dstEndpoint = targetEndpoint;
      if (table.add(b)) ++added;
    }
    return added;
  }

  /** One-shot: match the initiator's output clusters against the target's input
      clusters and bind them. @return the number of bindings created. */
  static uint8_t bindMatching(ZigbeeBindingTable& table, uint64_t srcIeee,
                              uint8_t srcEndpoint,
                              const uint16_t* myOutputClusters,
                              uint8_t myOutputCount, uint64_t targetIeee,
                              uint8_t targetEndpoint,
                              const uint16_t* targetInputClusters,
                              uint8_t targetInputCount) {
    uint16_t matched[16];
    uint8_t n = matchClusters(myOutputClusters, myOutputCount,
                              targetInputClusters, targetInputCount, matched, 16);
    return createBindings(table, srcIeee, srcEndpoint, matched, n, targetIeee,
                          targetEndpoint);
  }
};

}  // namespace nzb

#endif  // NIUS_ZIGBEE_FINDING_BINDING_H
