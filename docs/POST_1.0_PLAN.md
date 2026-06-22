# NiusZigbee — post-1.0 improvement / optimization plan

NiusZigbee 1.0.0 is the first stable release: a complete host-side Zigbee 3.0 /
Zigbee PRO stack on the nRF52840 driving a CC2530, with ZNP-grade MAC reliability
(CSMA-CA, MAC-ACK + retransmit, energy-detect scan, tunable MAC PIB), a second
TI Z-Stack ZNP backend, and R23 Dynamic Link Key on NiusZigbee's own elliptic-curve
crypto — all hardware-verified.

## Release discipline after 1.0

The 1.0 line was built by shipping features as they were verified on hardware. After
1.0 that bar **tightens**: a new feature is released **only when verified on real
hardware**, never build-verified-only. Each item below therefore names its
**verification requirement** up front; an item is not started toward release until
its rig or spec is available, so we never ship an unverifiable guess.

Every feature continues to follow the repo convention: a `CC2530_*` self-test
example with an `N/N PASS` count, on-air evidence recorded in
[VERIFIED_BEHAVIOR.md](VERIFIED_BEHAVIOR.md), gated behind a `-DNIUS_ZIGBEE_*` build
flag where it touches the mesh demo, and a version bump + tag per verified feature.

## 1. ZNP MAC refinements — need a sleepy-device rig

- **Per-child frame-pending (CC2530 SRCMATCH).** Replace the global
  `FRMCTRL1.PENDING_OR` with the hardware source-match table so only the polling
  child that actually has buffered data sees frame-pending. *Verify:* a real sleepy
  child polls; confirm pending is set for it and not for a second idle child.
- **RxOnWhenIdle (sleepy radio-off).** Let an end device power its receiver down
  between MAC data polls. *Verify:* a current meter shows the radio draw drop between
  polls with no missed buffered frames.

## 2. R23 completion — need a commissioning peer and/or the R23 spec constants

- **Wire DLK into secure join.** Use the verified `ZigbeeSpeke` handshake in
  `CC2530_BeaconJoin` to establish a per-joiner link key, replacing the
  `ZigBeeAlliance09` default in the transport-key step. *Verify:* a two-board
  DLK-authenticated join; a wrong install code is rejected at confirmation.
- **Byte-exact R23 interop.** Match the spec's generator mapping, KDF labels, and
  transcript hashing so the DLK interoperates with a certified R23 device. *Verify:*
  key agreement against a Silicon Labs / TI R23 node. (Needs the R23 spec constants.)
- **Device Interview, Trust Center Swap-Out.** R23 TLV-based join-time device query/
  filter, and re-keying the Trust Center without recommissioning. *Verify:* the
  join-filter decision and the TC hand-off on a multi-node bench. (Needs the R23 TLV
  constants.)

## 3. Reliability quantification — need a controlled congestion rig

- **beat-ZNP delivery benchmark.** A forced-loss / multi-hop-with-data
  congestion-and-delivery comparison against the pre-v0.7 firmware and a ZNP node,
  to put numbers on the reliability gain. The mechanisms are verified (MAC-ACK
  recovered 93 frames at 100% delivery under a jammer); the `mac[retx noack]` +
  `aps[...]` counters are the instrument. *Verify:* a clean, isolated 3-board rig
  (coordinator + end + congestor) with repeatable loss.

## 4. Zigbee Direct (BLE) — parked on the board package

R23 Zigbee Direct onboarding over BLE is feasible on the nice!nano boards (they have
the radio) and would reuse the X25519/SPEKE crypto, but it waits on a mature
**NimBLE** controller in the ArduinoNRF board package (currently an early slice).

## 5. Optimization (no new protocol surface)

- **X25519 performance.** The current field arithmetic uses the portable 16×16-bit
  limb representation; a 32-bit-limb (or Cortex-M4 assembly) version would cut DLK
  latency if commissioning time matters.
- **Firmware footprint / throughput.** Trim the CC2530 image; tune APS/NWK
  throughput using the now-runtime-tunable MAC PIB; quantify before/after.
- **Test breadth.** A periodic full compile-baseline of the example set to catch
  bit-rot, and consolidating the bench scripts as local (git-ignored) tooling.

## Out of scope

- **Sub-GHz** — the CC2530 is a 2.4 GHz-only transceiver (hardware limit). A
  2.4 GHz-only device is still a conformant R23 device.
- **Formal certification** — requires the CSA test harness.
