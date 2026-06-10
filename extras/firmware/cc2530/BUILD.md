# CC2530 transceiver firmware

`cc2530_radio.c` is a small, **non-banked** IEEE 802.15.4 transceiver for the
CC2530, built with the free **SDCC** compiler. It deliberately avoids TI's
Z-Stack runtime (and its clone-incompatible clock/banking startup): it runs the
CPU on the 32 MHz XOSC started via `CLKCONCMD`, brings up UART0 at 115200, and
drives the radio directly — which is why it works on clone modules where stock
Z-Stack hangs.

It exposes a framed UART protocol (host = the nRF52840):

```
Host -> CC2530:  FE  LEN  CMD  [DATA..]  FCS        (FCS = XOR of LEN..DATA)
  0x01 PING                    -> 0x81 PONG [ver_hi ver_lo]
  0x02 SET_CHANNEL [11..26]    -> 0x82 OK
  0x03 TX [psdu..]             -> 0x83 TXSTAT [0=ok / 1=fail]
  0x04 SET_PROMISC [filter]    -> 0x82 OK
       filter: 0 = promiscuous / frame filter disabled, 1 = filtered
CC2530 -> Host (asynchronous):
  0x80 RESET_IND [ver_hi ver_lo]      (sent once at boot)
  0x84 RX_FRAME  [rssi lqi psdu..]
```

The matching host driver is `src/modules/CC2530/CC2530Radio.cpp`.

## Build

```sh
sdcc -mmcs51 cc2530_radio.c          # produces cc2530_radio.ihx
# flat binary for flashing:
objcopy -I ihex -O binary cc2530_radio.ihx cc2530_radio.bin
```

`cc2530_radio.bin` (≈1.2 KB) is the image to flash at address 0. The prebuilt
`cc2530_radio.bin` / `.hex` here are checked in so you don't need SDCC unless you
modify the source.

## Flash it

Use the **CC2530_FlashFirmware** example (it embeds `cc2530_radio.bin` and writes
it with ArduinoNRF's built-in `CCDebugger`). To regenerate the embedded header:

```sh
xxd -i cc2530_radio.bin | sed 's/unsigned char .*\[\]/const uint8_t FW[]/; s/unsigned int .*_len/const unsigned int FW_LEN/' \
  > ../../../examples/CC2530_FlashFirmware/cc2530_radio_fw.h
```
