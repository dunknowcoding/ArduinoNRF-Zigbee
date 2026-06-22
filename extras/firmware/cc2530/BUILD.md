# CC2530 transceiver firmware

`cc2530_radio.c` is a small, non-banked IEEE 802.15.4 MAC/PHY helper for the
CC2530, built with the free SDCC compiler. It deliberately avoids TI's Z-Stack
runtime and clone-incompatible startup path: it runs the CPU on the 32 MHz XOSC
started via `CLKCONCMD`, brings up UART0 at 115200, and drives the radio
directly.

The nRF52840 host owns Zigbee NWK/APS/ZCL behavior. The CC2530 firmware exposes
the radio features needed underneath a future Zigbee PRO stack: PAN/short/IEEE
address registers, hardware frame filtering, Auto ACK, CCA transmit, TX retry
count, raw TX/RX, and promiscuous sniffing.

Firmware v0.3 keeps `FRMFILT0.MAX_FRAME_VERSION` at the CC2530 reset value while
enabling frame filtering. This is required for Zigbee data frames, which use
IEEE 802.15.4-2006 frame version 1.

Firmware v0.4 fixes a large-frame RX corruption: `radio_rx()` used to read the
RXFIFO in a tight loop the moment `FIFOP` asserted, but `FIFOP` fires at the
default ~64-byte threshold (i.e. mid-reception) for a large frame, so the read
underran the FIFO and copied repeated stale bytes for the tail. Frames longer
than ~70 bytes (e.g. an APS key-transport) arrived with a garbled cipher/MIC.
The read now paces to reception (`while(RXFIFOCNT==0)` per byte, bounded so an
aborted frame cannot hang it). Reflash every module after rebuilding.

Firmware v0.5 adds `SET_PENDING` (0x0A): the host sets/clears
`FRMCTRL1.PENDING_OR`, which forces the frame-pending bit in outgoing auto-ACKs
to 1. A parent sets it while it has frames buffered for a sleepy child
(`ZigbeeIndirectQueue::hasPending`) so the child's MAC Data Request poll is
answered with "data pending", then clears it when the queue drains. It is opt-in
(default 0), so behavior is unchanged for non-sleepy use. The new command is
build-verified with SDCC; the on-air frame-pending effect on the auto-ACK should
be confirmed on the bench with a real sleepy child.

Firmware v0.6 adds **unslotted CSMA-CA** to the transmit path. When CCA TX is
enabled (`MAC_FLAG_CCA_TX`), instead of the old single-shot `STXONCCA` +
immediate-retry-on-busy, the radio now waits a random number of backoff periods
(seeded from the `RFRND` true-random register), re-samples `FSMSTAT1.CCA`, and
raises the backoff exponent (BE 3..5, up to 4 backoffs) before transmitting -
the IEEE 802.15.4 channel-access procedure the official Z-Stack MAC uses. This
is the main on-air behavior our SDCC firmware was missing under congestion; it
spreads contending transmitters instead of colliding on immediate retries.
Non-CCA TX keeps the legacy immediate-retry loop.

Firmware v0.7 adds **MAC-level ACK + retransmit** - the other half of the official
MAC's reliability. When a transmitted frame requests an acknowledgement (FCF.AR),
the firmware waits for the matching ACK (same DSN, CRC-valid) and, if it does not
arrive, retransmits the whole frame (re-running CSMA-CA) up to `macMaxFrameRetries`
(3) times, exactly like TI's MAC for unicast. `TXSTAT` now reflects the *acked*
result, so the host learns of a delivered-and-acknowledged frame rather than just
"transmitted". A frame that arrives during the ACK wait but is **not** our ACK is
forwarded to the host as a normal `RX_FRAME`, so no inbound traffic is dropped
while waiting. Only ack-requested frames are affected; broadcasts and
ack-not-requested frames keep the previous behavior. On-air verified: a v0.7 node
joins and exchanges traffic cleanly (`mic=0 rpl=0`).

Firmware v0.8 adds `GET_STATS` (0x0B): two MAC reliability counters, `mac_retx`
(unicasts delivered only after one or more MAC retransmits) and `mac_noack`
(unicasts that exhausted `macMaxFrameRetries` without an ACK). They make the v0.7
retransmit path observable from the host (`CC2530Radio::getMacStats`,
`mac[retx=.. noack=..]` in the `CC2530_BeaconJoin` status line) - `retx > 0` is
direct evidence that the MAC recovered a frame the channel dropped, which is the
reliability ZNP provides and our pre-v0.7 firmware did not. Pure instrumentation;
no change to the TX/RX behavior.

Firmware v0.9 adds `ED_SCAN` (0x0C): an IEEE 802.15.4 **Energy-Detect scan**. The
host names a channel; the firmware tunes the receiver, samples the RSSI across a
short dwell, and returns the **peak** energy (signed; dBm = value - 73). This is the
MLME-SCAN energy-detect primitive the official MAC uses to (a) pick the quietest
channel when a coordinator forms a network and (b) detect a jammed/busy channel for
frequency agility - it lets the host *sense* interference rather than only fail on
it. Exposed by `CC2530Radio::energyScan()` and demonstrated by `CC2530_EnergyScan`.
On-air verified: against a co-located CCA-off jammer, the jammed channel reads
~-23 dBm while quiet channels sit near the -78 dBm noise floor.

Firmware v0.10 adds `SET_MAC_PIB` (0x0D): the host can tune the MAC PIB at runtime -
`macMinBE` / `macMaxBE` (CSMA backoff-exponent window), `macMaxCSMABackoffs`, and
`macMaxFrameRetries` - exactly like ZNP's settable MAC attributes, so the host can
trade reliability against latency or widen the backoff window under congestion
(defaults remain the 802.15.4 values 3/5/4/3). `macMaxFrameRetries = 0` disables
the v0.7 retransmit at runtime (the runtime equivalent of the `MAC_NO_RETRANSMIT`
build flag). Exposed by `CC2530Radio::setMacPib()`.

A compile-time flag `MAC_NO_RETRANSMIT` (off by default) builds the CCA TX path
without the v0.7 ACK-wait/retransmit (pre-v0.7 behaviour). It is a bench comparison
aid for measuring the MAC-ACK contribution; the shipped binary never defines it.

## UART protocol

```
Host -> CC2530:  FE  LEN  CMD  [DATA..]  FCS        (FCS = XOR of LEN..DATA)
  0x01 PING                    -> 0x81 PONG [ver_hi ver_lo]
  0x02 SET_CHANNEL [11..26]    -> 0x82 OK
  0x03 TX [psdu..]             -> 0x83 TXSTAT [status attempts]
  0x04 SET_PROMISC [filter]    -> 0x82 OK
       filter: 0 = promiscuous / frame filter disabled, 1 = filtered
  0x05 SET_ADDR [pan short ieee] -> 0x82 OK
       pan/short little-endian, ieee = 8 bytes little-endian
  0x06 SET_MAC [flags retries] -> 0x82 OK
       flags bit0 = filter, bit1 = Auto ACK, bit2 = CCA TX
  0x07 GET_MAC                 -> 0x85 MAC_INFO [flags retries pan short ieee]
  0x08 TX_ADV [retries psdu..] -> 0x83 TXSTAT [status attempts]
  0x09 SET_TX_POWER [raw]      -> 0x82 OK
  0x0A SET_PENDING [0|1]       -> 0x82 OK   (force frame-pending in auto-ACKs)
  0x0B GET_STATS               -> 0x86 STATS [retx_lo retx_hi noack_lo noack_hi]
  0x0C ED_SCAN [channel]       -> 0x87 ED_RESULT [channel peak_rssi]
       peak_rssi signed; dBm = peak_rssi - 73. Leaves the radio on [channel].
  0x0D SET_MAC_PIB [minBE maxBE maxBackoffs maxFrameRetries] -> 0x82 OK
       runtime CSMA/retransmit tuning; maxFrameRetries 0 disables MAC-ACK retransmit

CC2530 -> Host (asynchronous):
  0x80 RESET_IND [ver_hi ver_lo]      (sent once at boot)
  0x84 RX_FRAME  [rssi lqi psdu..]
```

TX status is `0 = ok`, `1 = no TXDONE before timeout`, `2 = bad frame length`.
The matching host driver is `src/modules/CC2530/CC2530Radio.cpp`.

## Build

```sh
sdcc -mmcs51 cc2530_radio.c
objcopy -I ihex -O binary cc2530_radio.ihx cc2530_radio.bin
objcopy -I ihex -O ihex cc2530_radio.ihx cc2530_radio.hex
```

`cc2530_radio.bin` is the image to flash at address 0. The prebuilt `.bin` /
`.hex` files here are checked in so users do not need SDCC unless they modify
the firmware source.

## Flash it

Use the `CC2530_FlashFirmware` example. It embeds `cc2530_radio.bin` and writes
it with ArduinoNRF's built-in `CCDebugger`.

After rebuilding the binary, regenerate
`examples/CC2530_FlashFirmware/cc2530_radio_fw.h` from `cc2530_radio.bin` before
testing or releasing.
