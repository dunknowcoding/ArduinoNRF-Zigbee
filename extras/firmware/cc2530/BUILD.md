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
