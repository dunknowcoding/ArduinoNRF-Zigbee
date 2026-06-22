/* CC2530 802.15.4 radio co-processor (SDCC) for ArduinoNRF.
 *
 * Turns a CC2530 module into a UART-controlled 802.15.4 MAC/PHY helper. Runs on
 * the 32 MHz XOSC started via CLKCONCMD (the path that works on clone modules
 * where the stock Z-Stack SLEEPCMD sequence hangs). Host protocol over UART0
 * @115200:
 *
 *   Host -> CC2530:  FE  LEN  CMD  [DATA..]  FCS      (FCS = XOR of LEN..DATA)
 *     0x01 PING                      -> 0x81 PONG [ver_hi ver_lo]
 *     0x02 SET_CHANNEL [ch 11..26]   -> 0x82 OK
 *     0x03 TX [psdu..]               -> 0x83 TXSTAT [status attempts]
 *     0x04 SET_PROMISC [filter]      -> 0x82 OK
 *          filter: 0=promiscuous/frame filter disabled, 1=filtered
 *     0x05 SET_ADDR [pan short ieee] -> 0x82 OK
 *          pan/short little-endian, ieee = 8 bytes little-endian
 *     0x06 SET_MAC [flags retries]   -> 0x82 OK
 *          flags bit0=filter, bit1=auto ACK, bit2=CCA TX
 *     0x07 GET_MAC                   -> 0x85 MAC_INFO [flags retries pan short ieee]
 *     0x08 TX_ADV [retries psdu..]   -> 0x83 TXSTAT [status attempts]
 *     0x09 SET_TX_POWER [raw]        -> 0x82 OK
 *     0x0A SET_PENDING [flag]        -> 0x82 OK
 *          flag: 1 = set the frame-pending bit in outgoing auto-ACKs (a parent
 *          tells a polling sleepy child "I have buffered data for you"),
 *          0 = clear it. Maps to FRMCTRL1.PENDING_OR. Opt-in: default 0, so
 *          behavior is unchanged unless the host buffers indirect frames.
 *   CC2530 -> Host (async):
 *     0x80 RESET_IND [ver_hi ver_lo]
 *     0x84 RX_FRAME [rssi lqi psdu..]
 */
#define FW_VER_HI 0
#define FW_VER_LO 8

/* ---- SFRs ---- */
__sfr __at (0xF1) PERCFG;
__sfr __at (0xF3) P0SEL;
__sfr __at (0x86) U0CSR;
__sfr __at (0xC4) U0UCR;
__sfr __at (0xC5) U0GCR;
__sfr __at (0xC2) U0BAUD;
__sfr __at (0xC1) U0DBUF;
__sfr __at (0xC6) CLKCONCMD;
__sfr __at (0x9E) CLKCONSTA;
__sfr __at (0x9D) SLEEPSTA;
__sfr __at (0xE1) RFST;
__sfr __at (0xD9) RFD;
__sfr __at (0x91) RFIRQF1;
__sfr __at (0xE9) RFIRQF0;

/* ---- RF XREGs ---- */
__xdata __at (0x616A) volatile unsigned char EXT_ADDR0;
__xdata __at (0x616B) volatile unsigned char EXT_ADDR1;
__xdata __at (0x616C) volatile unsigned char EXT_ADDR2;
__xdata __at (0x616D) volatile unsigned char EXT_ADDR3;
__xdata __at (0x616E) volatile unsigned char EXT_ADDR4;
__xdata __at (0x616F) volatile unsigned char EXT_ADDR5;
__xdata __at (0x6170) volatile unsigned char EXT_ADDR6;
__xdata __at (0x6171) volatile unsigned char EXT_ADDR7;
__xdata __at (0x6172) volatile unsigned char PAN_ID0;
__xdata __at (0x6173) volatile unsigned char PAN_ID1;
__xdata __at (0x6174) volatile unsigned char SHORT_ADDR0;
__xdata __at (0x6175) volatile unsigned char SHORT_ADDR1;
__xdata __at (0x6180) volatile unsigned char FRMFILT0;
__xdata __at (0x6189) volatile unsigned char FRMCTRL0;
__xdata __at (0x618A) volatile unsigned char FRMCTRL1;
__xdata __at (0x618F) volatile unsigned char FREQCTRL;
__xdata __at (0x6190) volatile unsigned char TXPOWER;
__xdata __at (0x6193) volatile unsigned char FSMSTAT1;
__xdata __at (0x6198) volatile unsigned char RSSI;
__xdata __at (0x6199) volatile unsigned char RSSISTAT;
__xdata __at (0x619B) volatile unsigned char RXFIFOCNT;
__xdata __at (0x61A7) volatile unsigned char RFRND;
__xdata __at (0x61AB) volatile unsigned char RXCTRL;
__xdata __at (0x61AC) volatile unsigned char FSCTRL;
__xdata __at (0x61AE) volatile unsigned char FSCAL1;
__xdata __at (0x61B2) volatile unsigned char AGCCTRL1;
__xdata __at (0x61B5) volatile unsigned char ADCTEST0;
__xdata __at (0x61B6) volatile unsigned char ADCTEST1;
__xdata __at (0x61B7) volatile unsigned char ADCTEST2;
__xdata __at (0x61FA) volatile unsigned char TXFILTCFG;

#define IRQ_FIFOP   0x04   /* RFIRQF0: frame available in RXFIFO */
#define IRQ_TXDONE  0x02   /* RFIRQF1: TX complete */

#define CMD_PING          0x01
#define CMD_SET_CHANNEL   0x02
#define CMD_TX            0x03
#define CMD_SET_PROMISC   0x04
#define CMD_SET_ADDR      0x05
#define CMD_SET_MAC       0x06
#define CMD_GET_MAC       0x07
#define CMD_TX_ADV        0x08
#define CMD_SET_TX_POWER  0x09
#define CMD_SET_PENDING   0x0A
#define CMD_GET_STATS     0x0B

#define RSP_RESET_IND     0x80
#define RSP_PONG          0x81
#define RSP_OK            0x82
#define RSP_TXSTAT        0x83
#define RSP_RX_FRAME      0x84
#define RSP_MAC_INFO      0x85
#define RSP_STATS         0x86

#define MAC_FLAG_FILTER   0x01
#define MAC_FLAG_AUTOACK  0x02
#define MAC_FLAG_CCA_TX   0x04

#define FRMFILT0_FRAME_FILTER       0x01
#define FRMFILT0_MAX_FRAME_VERSION  0x0C

#define FRMCTRL0_AUTOACK  0x20
#define FRMCTRL0_AUTOCRC  0x40

/* FRMCTRL1.PENDING_OR (bit 2): when 1, the frame-pending bit in ALL outgoing
   auto-ACKs is forced to 1, so a parent answers a sleepy child's MAC Data
   Request poll with "I have data buffered for you". The host sets this while it
   has indirect frames queued (ZigbeeIndirectQueue::hasPending) and clears it
   when the queue drains. */
#define FRMCTRL1_PENDING_OR  0x04

#define STROBE_SRXON      0xE3
#define STROBE_STXON      0xE9
#define STROBE_STXONCCA   0xEA
#define STROBE_SFLUSHRX   0xED
#define STROBE_SFLUSHTX   0xEE

/* FSMSTAT1 (0x6193) bits, RSSISTAT (0x6199) bit, used for CSMA-CA. */
#define FSMSTAT1_CCA      0x10   /* current clear-channel assessment: 1 = clear */
#define RSSISTAT_VALID    0x01   /* RSSI sample valid (CCA undefined until set) */

/* Unslotted CSMA-CA parameters (IEEE 802.15.4 defaults). On a busy channel the
   transmitter waits a random number of backoff periods, re-samples CCA, and
   raises the backoff exponent, instead of hammering the air with immediate
   retries. This is what the official Z-Stack MAC does and what our previous
   single-shot STXONCCA did NOT - the main on-air difference under congestion. */
#define CSMA_MIN_BE       3
#define CSMA_MAX_BE       5
#define CSMA_MAX_BACKOFFS 4
/* ~320 us (one aUnitBackoffPeriod, 20 symbols) of busy-wait at 32 MHz. */
#define CSMA_BACKOFF_LOOPS 2000U

/* MAC-level acknowledgement + retransmit - the other half of the official MAC's
   reliability that our firmware was missing. When a transmitted frame requests an
   ACK (FCF.AR), wait for the matching ACK (same DSN, CRC-valid) and, if it does
   not arrive, retransmit the whole frame (re-running CSMA-CA) up to
   MAC_MAX_FRAME_RETRIES times - exactly what TI's MAC does for unicast. A frame
   that arrives during the ACK wait but is NOT our ACK is forwarded to the host, so
   no inbound traffic is dropped while we wait. Only ack-requested frames are
   affected; broadcasts and ack-not-requested frames keep the old behavior. */
#define FCF_ACK_REQUEST   0x20   /* FCF byte0 bit5: acknowledgement request */
#define FCF_TYPE_MASK     0x07   /* FCF byte0 frame-type field */
#define FCF_TYPE_ACK      0x02   /* ...== ACK */
#define RXSTAT_CRC_OK     0x80   /* trailing status byte bit7: CRC passed */
#define MAC_MAX_FRAME_RETRIES 3  /* macMaxFrameRetries (4 transmissions total) */
#define ACK_WAIT_LOOPS    6000U  /* > ~1 ms: covers aTurnaround + ACK reception */

static __xdata unsigned char rxbuf[140];
static unsigned char mac_flags = 0;
static unsigned char tx_retries = 0;
static unsigned int rng_state = 0xACE1u;   /* CSMA backoff PRNG (software, no radio reads) */
/* MAC reliability counters (read via CMD_GET_STATS) - make the v0.7 retransmit
   path observable: mac_retx = unicasts delivered only after >=1 MAC retransmit,
   mac_noack = unicasts that exhausted macMaxFrameRetries with no ACK. */
static unsigned int mac_retx = 0;
static unsigned int mac_noack = 0;
static unsigned char wait_for_ack(unsigned char dsn);  /* defined after radio_rx */

static void clock_init(void){
  CLKCONCMD = 0x80;                 /* 32 MHz XOSC, 32 kHz RC */
  { unsigned int g=0; while(!(SLEEPSTA & 0x40) && ++g){} }  /* wait XOSC stable */
}
static void uart_init(void){
  PERCFG=0x00; P0SEL|=0x0C;         /* UART0 alt1, P0.2/P0.3 */
  U0CSR=0xC0;                       /* UART mode + RX enable */
  U0UCR=0x02;                       /* 8N1, no flow control */
  U0GCR=0x0B; U0BAUD=216;           /* 115200 @ 32 MHz */
}
static void utx(unsigned char c){ U0DBUF=c; while(!(U0CSR & 0x02)){} U0CSR &= ~0x02; }
static unsigned char urx_avail(void){ return U0CSR & 0x04; }
static unsigned char urx(void){ while(!(U0CSR & 0x04)){} return U0DBUF; }

static void apply_mac(void){
  /* Keep MAX_FRAME_VERSION at 3. Writing only bit0 rejects Zigbee's
     IEEE 802.15.4-2006 data frames (frame version 1) when filtering is on. */
  FRMFILT0 = (unsigned char)(FRMFILT0_MAX_FRAME_VERSION |
             ((mac_flags & MAC_FLAG_FILTER) ? FRMFILT0_FRAME_FILTER : 0x00));
  FRMCTRL0 = (unsigned char)(FRMCTRL0_AUTOCRC |
             ((mac_flags & MAC_FLAG_AUTOACK) ? FRMCTRL0_AUTOACK : 0x00));
}

static void set_address(__xdata unsigned char* d){
  PAN_ID0=d[0]; PAN_ID1=d[1];
  SHORT_ADDR0=d[2]; SHORT_ADDR1=d[3];
  EXT_ADDR0=d[4]; EXT_ADDR1=d[5]; EXT_ADDR2=d[6]; EXT_ADDR3=d[7];
  EXT_ADDR4=d[8]; EXT_ADDR5=d[9]; EXT_ADDR6=d[10]; EXT_ADDR7=d[11];
  /* Seed the CSMA backoff PRNG per node from the IEEE LSBs, so different nodes
     pick different backoff slots (decorrelates contending transmitters). */
  rng_state = (unsigned int)d[4] | ((unsigned int)d[5] << 8);
  if(rng_state == 0u) rng_state = 0xACE1u;
}

static void radio_init(unsigned char ch){
  apply_mac();
  TXFILTCFG=0x09; AGCCTRL1=0x15; FSCAL1=0x00; RXCTRL=0x3F; FSCTRL=0x55;
  ADCTEST0=0x10; ADCTEST1=0x0E; ADCTEST2=0x03;
  if(ch<11) ch=11; if(ch>26) ch=26;
  FREQCTRL=(unsigned char)(11 + 5*(ch-11));
  TXPOWER=0xF5;
  RFST=STROBE_SFLUSHRX;             /* flush RX */
  RFST=STROBE_SRXON;                /* enter RX */
}

/* transmit psdu[len]; radio appends FCS. returns 0 ok / 1 fail / 2 bad len */
static unsigned char radio_tx_once(__xdata unsigned char* psdu, unsigned char len){
  unsigned int t; unsigned char i, done=0;
  if(len>125) return 2;
  RFST=STROBE_SFLUSHTX;             /* flush TX FIFO */
  RFD=(unsigned char)(len+2);       /* PHR = psdu + 2 FCS */
  for(i=0;i<len;i++) RFD=psdu[i];
  RFIRQF1=0;
  RFST=(mac_flags & MAC_FLAG_CCA_TX) ? STROBE_STXONCCA : STROBE_STXON;
  for(t=0;t<60000;t++){ if(RFIRQF1 & IRQ_TXDONE){ done=1; break; } }
  RFST=STROBE_SRXON;                /* back to RX */
  return done?0:1;
}
/* Software xorshift LFSR for the CSMA backoff. Deliberately does NOT read the
   RF random register (RFRND): RFRND samples the receiver ADC, and reading it in
   the TX/backoff path disturbed the radio enough that a just-channel-changed
   transmitter (the active scan setChannel()s before every beacon request) could
   never get a frame out (tx=0). A plain PRNG is more than enough to de-correlate
   backoff slots and never touches the radio. Seeded per node from the IEEE LSBs
   (set in set_address) so different nodes pick different slots. */
static unsigned char rng_byte(void){
  unsigned int x = rng_state;
  x ^= (unsigned int)(x << 7);
  x ^= (unsigned int)(x >> 9);
  x ^= (unsigned int)(x << 8);
  rng_state = x ? x : 0xACE1u;
  return (unsigned char)(x & 0xFFu);
}

/* Busy-wait n aUnitBackoffPeriods (~320 us each). */
static void backoff_delay(unsigned char periods){
  unsigned char p; unsigned int t;
  for(p=0;p<periods;p++){ for(t=0;t<CSMA_BACKOFF_LOOPS;t++){ } }
}

static unsigned char radio_tx(__xdata unsigned char* psdu, unsigned char len,
                              unsigned char retries, unsigned char* attempts){
  unsigned char r=1, i, be=CSMA_MIN_BE, max_attempts;
  if(len>125){ *attempts=0; return 2; }
  /* CSMA-CA: radio_tx_once() already does the hardware CCA+TX (STXONCCA, the
     proven v0.5 path) and returns nonzero when the channel was busy. So just add
     the 802.15.4 backoff between attempts: on a busy channel, wait a random
     number of backoff periods and retry with a widening window, instead of
     hammering immediate retries. Building on radio_tx_once keeps the exact TX
     sequence that the scan/join path is known to work with - an earlier rewrite
     that re-implemented the TX inline regressed end-device scanning (tx=0). */
  if(mac_flags & MAC_FLAG_CCA_TX){
    unsigned char fr, want_ack=(unsigned char)(psdu[0] & FCF_ACK_REQUEST), total=0;
    /* macMaxFrameRetries: each pass runs the full CSMA-CA, transmits, then (if the
       frame requested an ACK) waits for it; on no-ACK the whole frame is retried. */
    for(fr=0; fr<=MAC_MAX_FRAME_RETRIES; fr++){
      be=CSMA_MIN_BE;
      for(i=0; i<=CSMA_MAX_BACKOFFS; i++){
        r=radio_tx_once(psdu,len);
        if(++total==0) total=255;
        if(r==0) break;            /* got the channel + TXDONE */
        backoff_delay((unsigned char)(rng_byte() & (unsigned char)((1u<<be)-1u)));
        if(be<CSMA_MAX_BE) be++;
      }
      if(r!=0){ *attempts=total; return r; }   /* channel-access failure */
      if(!want_ack){ *attempts=total; return 0; }       /* no ACK expected */
      if(wait_for_ack(psdu[2])){
        if(fr>0) mac_retx++;       /* delivered only after >=1 retransmit */
        *attempts=total; return 0;
      }
      /* no ACK -> retransmit the whole frame (outer loop) */
    }
    mac_noack++;                   /* exhausted macMaxFrameRetries, never acked */
    *attempts=total; return 1;
  }
  max_attempts=(unsigned char)(retries+1);
  for(i=0;i<max_attempts;i++){
    r=radio_tx_once(psdu,len);
    *attempts=(unsigned char)(i+1);
    if(r==0) return 0;
  }
  return r;
}

/* poll RXFIFO; if a frame is present read it into rxbuf, return its length (incl
   2 trailing status bytes RSSI,CRC|LQI), else 0 */
static unsigned char radio_rx(void){
  unsigned char len, i;
  if(!(RFIRQF0 & IRQ_FIFOP)) return 0;
  len = RFD;                        /* first FIFO byte = frame length */
  if(len==0 || len>127){ RFST=STROBE_SFLUSHRX; RFIRQF0=0; return 0; }
  /* FIFOP asserts at the default threshold (~64 bytes), i.e. mid-reception for
     a large frame, so reading RFD in a tight loop underruns the RXFIFO and
     copies repeated stale bytes for the tail (a >~70 B frame arrives with its
     cipher/MIC garbled). Pace each read to reception: wait for the byte to be
     present (RXFIFOCNT > 0) before reading it, bounded so an aborted frame
     (CRC discard mid-read) can't hang the loop forever. */
  for(i=0;i<len;i++){
    unsigned int guard=0;
    while(RXFIFOCNT==0){ if(++guard==0){ RFST=STROBE_SFLUSHRX; RFIRQF0=0; return 0; } }
    rxbuf[i]=RFD;                   /* psdu + RSSI + (CRC|LQI) */
  }
  RFIRQF0=0;
  if(RXFIFOCNT==0){} else { RFST=STROBE_SFLUSHRX; }   /* drain leftovers */
  return len;
}
static void send_frame(unsigned char resp, __xdata unsigned char* d, unsigned char n){
  unsigned char i, fcs;
  utx(0xFE); utx((unsigned char)(n+1)); utx(resp);
  fcs=(unsigned char)((n+1) ^ resp);
  for(i=0;i<n;i++){ utx(d[i]); fcs^=d[i]; }
  utx(fcs);
}

/* forward a frame already read into rxbuf[] (length rlen, incl 2 status bytes) to
   the host as RSP_RX_FRAME: [rssi][crc|lqi][psdu...]. */
static void forward_rx(unsigned char rlen){
  unsigned char i, fcs=(unsigned char)((rlen+1)^RSP_RX_FRAME);
  utx(0xFE); utx((unsigned char)(rlen+1)); utx(RSP_RX_FRAME);
  utx(rxbuf[rlen-2]); fcs^=rxbuf[rlen-2];       /* RSSI */
  utx(rxbuf[rlen-1]); fcs^=rxbuf[rlen-1];       /* CRC|LQI */
  for(i=0;i+2<rlen;i++){ utx(rxbuf[i]); fcs^=rxbuf[i]; }
  utx(fcs);
}

/* After an ack-requested TX, poll RX for the matching ACK (frame-type ACK, CRC OK,
   DSN == dsn). Any other frame received in the window is real inbound traffic, so
   forward it to the host rather than dropping it. Returns 1 if acked, 0 on timeout. */
static unsigned char wait_for_ack(unsigned char dsn){
  unsigned int t; unsigned char rlen;
  for(t=0;t<ACK_WAIT_LOOPS;t++){
    rlen = radio_rx();
    if(rlen>=2){
      if(rlen>=5 && (rxbuf[0] & FCF_TYPE_MASK)==FCF_TYPE_ACK &&
         (rxbuf[rlen-1] & RXSTAT_CRC_OK) && rxbuf[2]==dsn) return 1;
      forward_rx(rlen);             /* not our ACK: don't lose inbound data */
    }
  }
  return 0;
}

void main(void){
  __xdata unsigned char cmd[140];
  unsigned char st=0, ln=0, idx=0, c, rlen;
  __xdata unsigned char tmp[16];
  clock_init();
  uart_init();
  { __xdata unsigned char a[12] =
      {0xFF,0xFF,0xFF,0xFF,0,0,0,0,0,0,0,0};
    set_address(a);
  }
  radio_init(11);
  tmp[0]=FW_VER_HI; tmp[1]=FW_VER_LO;
  send_frame(RSP_RESET_IND, tmp, 2);
  for(;;){
    /* radio RX -> host */
    rlen = radio_rx();
    if(rlen >= 2){ forward_rx(rlen); }   /* 0x84 [rssi][crc|lqi][psdu...] */
    /* UART command parser (non-blocking, one byte per loop) */
    if(urx_avail()){
      c=urx();
      switch(st){
        case 0: if(c==0xFE){ st=1; } break;
        case 1: ln=c; idx=0; st=2; if(ln==0||ln>137){ st=0; } break;
        case 2:
          cmd[idx++]=c;
          if(idx>=ln+1){                 /* got LEN payload bytes + FCS */
            unsigned char cc=cmd[0], i, good=ln;
            for(i=0;i<ln;i++) good^=cmd[i];
            if(good==cmd[ln]){
              if(cc==CMD_PING){
                tmp[0]=FW_VER_HI; tmp[1]=FW_VER_LO; send_frame(RSP_PONG,tmp,2);
              }
              else if(cc==CMD_SET_CHANNEL && ln>=2){
                radio_init(cmd[1]); send_frame(RSP_OK,tmp,0);
              }
              else if(cc==CMD_TX){
                unsigned char attempts=0;
                unsigned char r=radio_tx(&cmd[1],(unsigned char)(ln-1),tx_retries,&attempts);
                tmp[0]=r; tmp[1]=attempts; send_frame(RSP_TXSTAT,tmp,2);
              }
              else if(cc==CMD_SET_PROMISC && ln>=2){
                if(cmd[1]) mac_flags |= MAC_FLAG_FILTER;
                else mac_flags &= (unsigned char)~MAC_FLAG_FILTER;
                apply_mac(); send_frame(RSP_OK,tmp,0);
              }
              else if(cc==CMD_SET_ADDR && ln>=13){
                set_address(&cmd[1]); send_frame(RSP_OK,tmp,0);
              }
              else if(cc==CMD_SET_MAC && ln>=3){
                mac_flags=(unsigned char)(cmd[1] & (MAC_FLAG_FILTER|MAC_FLAG_AUTOACK|MAC_FLAG_CCA_TX));
                tx_retries=cmd[2];
                apply_mac(); send_frame(RSP_OK,tmp,0);
              }
              else if(cc==CMD_GET_MAC){
                tmp[0]=mac_flags; tmp[1]=tx_retries;
                tmp[2]=PAN_ID0; tmp[3]=PAN_ID1;
                tmp[4]=SHORT_ADDR0; tmp[5]=SHORT_ADDR1;
                tmp[6]=EXT_ADDR0; tmp[7]=EXT_ADDR1; tmp[8]=EXT_ADDR2; tmp[9]=EXT_ADDR3;
                tmp[10]=EXT_ADDR4; tmp[11]=EXT_ADDR5; tmp[12]=EXT_ADDR6; tmp[13]=EXT_ADDR7;
                send_frame(RSP_MAC_INFO,tmp,14);
              }
              else if(cc==CMD_TX_ADV && ln>=2){
                unsigned char attempts=0;
                unsigned char r=radio_tx(&cmd[2],(unsigned char)(ln-2),cmd[1],&attempts);
                tmp[0]=r; tmp[1]=attempts; send_frame(RSP_TXSTAT,tmp,2);
              }
              else if(cc==CMD_SET_TX_POWER && ln>=2){
                TXPOWER=cmd[1]; send_frame(RSP_OK,tmp,0);
              }
              else if(cc==CMD_SET_PENDING && ln>=2){
                if(cmd[1]) FRMCTRL1 |= FRMCTRL1_PENDING_OR;
                else FRMCTRL1 &= (unsigned char)~FRMCTRL1_PENDING_OR;
                send_frame(RSP_OK,tmp,0);
              }
              else if(cc==CMD_GET_STATS){
                tmp[0]=(unsigned char)(mac_retx & 0xFF);
                tmp[1]=(unsigned char)(mac_retx >> 8);
                tmp[2]=(unsigned char)(mac_noack & 0xFF);
                tmp[3]=(unsigned char)(mac_noack >> 8);
                send_frame(RSP_STATS,tmp,4);
              }
            }
            st=0;
          }
          break;
      }
    }
  }
}
