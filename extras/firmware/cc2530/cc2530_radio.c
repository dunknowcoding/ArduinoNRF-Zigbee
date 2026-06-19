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
#define FW_VER_LO 5

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
__xdata __at (0x6198) volatile unsigned char RSSI;
__xdata __at (0x619B) volatile unsigned char RXFIFOCNT;
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

#define RSP_RESET_IND     0x80
#define RSP_PONG          0x81
#define RSP_OK            0x82
#define RSP_TXSTAT        0x83
#define RSP_RX_FRAME      0x84
#define RSP_MAC_INFO      0x85

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

static __xdata unsigned char rxbuf[140];
static unsigned char mac_flags = 0;
static unsigned char tx_retries = 0;

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
static unsigned char radio_tx(__xdata unsigned char* psdu, unsigned char len,
                              unsigned char retries, unsigned char* attempts){
  unsigned char r=1, i, max_attempts;
  if(len>125){ *attempts=0; return 2; }
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
    if(rlen >= 2){
      /* report: 0x84 [rssi][crc|lqi][psdu...]  (psdu = rxbuf[0..rlen-3]) */
      utx(0xFE); utx((unsigned char)(rlen+1)); utx(RSP_RX_FRAME);
      { unsigned char i, fcs=(unsigned char)((rlen+1)^RSP_RX_FRAME);
        utx(rxbuf[rlen-2]); fcs^=rxbuf[rlen-2];     /* RSSI */
        utx(rxbuf[rlen-1]); fcs^=rxbuf[rlen-1];     /* CRC|LQI */
        for(i=0;i+2<rlen;i++){ utx(rxbuf[i]); fcs^=rxbuf[i]; }
        utx(fcs);
      }
    }
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
            }
            st=0;
          }
          break;
      }
    }
  }
}
