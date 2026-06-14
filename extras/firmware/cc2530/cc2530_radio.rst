                                      1 ;--------------------------------------------------------
                                      2 ; File Created by SDCC : free open source ISO C Compiler
                                      3 ; Version 4.5.0 #15242 (MINGW64)
                                      4 ;--------------------------------------------------------
                                      5 	.module cc2530_radio
                                      6 	
                                      7 	.optsdcc -mmcs51 --model-small
                                      8 ;--------------------------------------------------------
                                      9 ; Public variables in this module
                                     10 ;--------------------------------------------------------
                                     11 	.globl _main
                                     12 	.globl _RFIRQF0
                                     13 	.globl _RFIRQF1
                                     14 	.globl _RFD
                                     15 	.globl _RFST
                                     16 	.globl _SLEEPSTA
                                     17 	.globl _CLKCONSTA
                                     18 	.globl _CLKCONCMD
                                     19 	.globl _U0DBUF
                                     20 	.globl _U0BAUD
                                     21 	.globl _U0GCR
                                     22 	.globl _U0UCR
                                     23 	.globl _U0CSR
                                     24 	.globl _P0SEL
                                     25 	.globl _PERCFG
                                     26 	.globl _TXFILTCFG
                                     27 	.globl _ADCTEST2
                                     28 	.globl _ADCTEST1
                                     29 	.globl _ADCTEST0
                                     30 	.globl _AGCCTRL1
                                     31 	.globl _FSCAL1
                                     32 	.globl _FSCTRL
                                     33 	.globl _RXCTRL
                                     34 	.globl _RXFIFOCNT
                                     35 	.globl _RSSI
                                     36 	.globl _TXPOWER
                                     37 	.globl _FREQCTRL
                                     38 	.globl _FRMCTRL0
                                     39 	.globl _FRMFILT0
                                     40 	.globl _SHORT_ADDR1
                                     41 	.globl _SHORT_ADDR0
                                     42 	.globl _PAN_ID1
                                     43 	.globl _PAN_ID0
                                     44 	.globl _EXT_ADDR7
                                     45 	.globl _EXT_ADDR6
                                     46 	.globl _EXT_ADDR5
                                     47 	.globl _EXT_ADDR4
                                     48 	.globl _EXT_ADDR3
                                     49 	.globl _EXT_ADDR2
                                     50 	.globl _EXT_ADDR1
                                     51 	.globl _EXT_ADDR0
                                     52 ;--------------------------------------------------------
                                     53 ; special function registers
                                     54 ;--------------------------------------------------------
                                     55 	.area RSEG    (ABS,DATA)
      000000                         56 	.org 0x0000
                           0000F1    57 _PERCFG	=	0x00f1
                           0000F3    58 _P0SEL	=	0x00f3
                           000086    59 _U0CSR	=	0x0086
                           0000C4    60 _U0UCR	=	0x00c4
                           0000C5    61 _U0GCR	=	0x00c5
                           0000C2    62 _U0BAUD	=	0x00c2
                           0000C1    63 _U0DBUF	=	0x00c1
                           0000C6    64 _CLKCONCMD	=	0x00c6
                           00009E    65 _CLKCONSTA	=	0x009e
                           00009D    66 _SLEEPSTA	=	0x009d
                           0000E1    67 _RFST	=	0x00e1
                           0000D9    68 _RFD	=	0x00d9
                           000091    69 _RFIRQF1	=	0x0091
                           0000E9    70 _RFIRQF0	=	0x00e9
                                     71 ;--------------------------------------------------------
                                     72 ; special function bits
                                     73 ;--------------------------------------------------------
                                     74 	.area RSEG    (ABS,DATA)
      000000                         75 	.org 0x0000
                                     76 ;--------------------------------------------------------
                                     77 ; overlayable register banks
                                     78 ;--------------------------------------------------------
                                     79 	.area REG_BANK_0	(REL,OVR,DATA)
      000000                         80 	.ds 8
                                     81 ;--------------------------------------------------------
                                     82 ; internal ram data
                                     83 ;--------------------------------------------------------
                                     84 	.area DSEG    (DATA)
      000008                         85 _mac_flags:
      000008                         86 	.ds 1
      000009                         87 _tx_retries:
      000009                         88 	.ds 1
      00000A                         89 _radio_tx_PARM_2:
      00000A                         90 	.ds 1
      00000B                         91 _radio_tx_PARM_3:
      00000B                         92 	.ds 1
      00000C                         93 _radio_tx_PARM_4:
      00000C                         94 	.ds 3
      00000F                         95 _radio_tx_max_attempts_10000_28:
      00000F                         96 	.ds 1
      000010                         97 _send_frame_PARM_2:
      000010                         98 	.ds 2
      000012                         99 _send_frame_PARM_3:
      000012                        100 	.ds 1
      000013                        101 _main_st_10000_46:
      000013                        102 	.ds 1
      000014                        103 _main_idx_10000_46:
      000014                        104 	.ds 1
      000015                        105 _main_attempts_80000_63:
      000015                        106 	.ds 1
      000016                        107 _main_attempts_80000_68:
      000016                        108 	.ds 1
                                    109 ;--------------------------------------------------------
                                    110 ; overlayable items in internal ram
                                    111 ;--------------------------------------------------------
                                    112 	.area	OSEG    (OVR,DATA)
                                    113 	.area	OSEG    (OVR,DATA)
                                    114 	.area	OSEG    (OVR,DATA)
                                    115 	.area	OSEG    (OVR,DATA)
      000017                        116 _radio_tx_once_PARM_2:
      000017                        117 	.ds 1
                                    118 	.area	OSEG    (OVR,DATA)
                                    119 ;--------------------------------------------------------
                                    120 ; Stack segment in internal ram
                                    121 ;--------------------------------------------------------
                                    122 	.area SSEG
      000018                        123 __start__stack:
      000018                        124 	.ds	1
                                    125 
                                    126 ;--------------------------------------------------------
                                    127 ; indirectly addressable internal ram data
                                    128 ;--------------------------------------------------------
                                    129 	.area ISEG    (DATA)
                                    130 ;--------------------------------------------------------
                                    131 ; absolute internal ram data
                                    132 ;--------------------------------------------------------
                                    133 	.area IABS    (ABS,DATA)
                                    134 	.area IABS    (ABS,DATA)
                                    135 ;--------------------------------------------------------
                                    136 ; bit data
                                    137 ;--------------------------------------------------------
                                    138 	.area BSEG    (BIT)
                                    139 ;--------------------------------------------------------
                                    140 ; paged external ram data
                                    141 ;--------------------------------------------------------
                                    142 	.area PSEG    (PAG,XDATA)
                                    143 ;--------------------------------------------------------
                                    144 ; uninitialized external ram data
                                    145 ;--------------------------------------------------------
                                    146 	.area XSEG    (XDATA)
                           00616A   147 _EXT_ADDR0	=	0x616a
                           00616B   148 _EXT_ADDR1	=	0x616b
                           00616C   149 _EXT_ADDR2	=	0x616c
                           00616D   150 _EXT_ADDR3	=	0x616d
                           00616E   151 _EXT_ADDR4	=	0x616e
                           00616F   152 _EXT_ADDR5	=	0x616f
                           006170   153 _EXT_ADDR6	=	0x6170
                           006171   154 _EXT_ADDR7	=	0x6171
                           006172   155 _PAN_ID0	=	0x6172
                           006173   156 _PAN_ID1	=	0x6173
                           006174   157 _SHORT_ADDR0	=	0x6174
                           006175   158 _SHORT_ADDR1	=	0x6175
                           006180   159 _FRMFILT0	=	0x6180
                           006189   160 _FRMCTRL0	=	0x6189
                           00618F   161 _FREQCTRL	=	0x618f
                           006190   162 _TXPOWER	=	0x6190
                           006198   163 _RSSI	=	0x6198
                           00619B   164 _RXFIFOCNT	=	0x619b
                           0061AB   165 _RXCTRL	=	0x61ab
                           0061AC   166 _FSCTRL	=	0x61ac
                           0061AE   167 _FSCAL1	=	0x61ae
                           0061B2   168 _AGCCTRL1	=	0x61b2
                           0061B5   169 _ADCTEST0	=	0x61b5
                           0061B6   170 _ADCTEST1	=	0x61b6
                           0061B7   171 _ADCTEST2	=	0x61b7
                           0061FA   172 _TXFILTCFG	=	0x61fa
      000001                        173 _rxbuf:
      000001                        174 	.ds 140
      00008D                        175 _main_cmd_10000_46:
      00008D                        176 	.ds 140
      000119                        177 _main_tmp_10000_46:
      000119                        178 	.ds 16
      000129                        179 _main_a_20000_47:
      000129                        180 	.ds 12
                                    181 ;--------------------------------------------------------
                                    182 ; absolute external ram data
                                    183 ;--------------------------------------------------------
                                    184 	.area XABS    (ABS,XDATA)
                                    185 ;--------------------------------------------------------
                                    186 ; initialized external ram data
                                    187 ;--------------------------------------------------------
                                    188 	.area XISEG   (XDATA)
                                    189 	.area HOME    (CODE)
                                    190 	.area GSINIT0 (CODE)
                                    191 	.area GSINIT1 (CODE)
                                    192 	.area GSINIT2 (CODE)
                                    193 	.area GSINIT3 (CODE)
                                    194 	.area GSINIT4 (CODE)
                                    195 	.area GSINIT5 (CODE)
                                    196 	.area GSINIT  (CODE)
                                    197 	.area GSFINAL (CODE)
                                    198 	.area CSEG    (CODE)
                                    199 ;--------------------------------------------------------
                                    200 ; interrupt vector
                                    201 ;--------------------------------------------------------
                                    202 	.area HOME    (CODE)
      000000                        203 __interrupt_vect:
      000000 02 00 4C         [24]  204 	ljmp	__sdcc_gsinit_startup
                                    205 ; restartable atomic support routines
      000003                        206 	.ds	5
      000008                        207 sdcc_atomic_exchange_rollback_start::
      000008 00               [12]  208 	nop
      000009 00               [12]  209 	nop
      00000A                        210 sdcc_atomic_exchange_pdata_impl:
      00000A E2               [24]  211 	movx	a, @r0
      00000B FB               [12]  212 	mov	r3, a
      00000C EA               [12]  213 	mov	a, r2
      00000D F2               [24]  214 	movx	@r0, a
      00000E 80 2C            [24]  215 	sjmp	sdcc_atomic_exchange_exit
      000010 00               [12]  216 	nop
      000011 00               [12]  217 	nop
      000012                        218 sdcc_atomic_exchange_xdata_impl:
      000012 E0               [24]  219 	movx	a, @dptr
      000013 FB               [12]  220 	mov	r3, a
      000014 EA               [12]  221 	mov	a, r2
      000015 F0               [24]  222 	movx	@dptr, a
      000016 80 24            [24]  223 	sjmp	sdcc_atomic_exchange_exit
      000018                        224 sdcc_atomic_compare_exchange_idata_impl:
      000018 E6               [12]  225 	mov	a, @r0
      000019 B5 02 02         [24]  226 	cjne	a, ar2, .+#5
      00001C EB               [12]  227 	mov	a, r3
      00001D F6               [12]  228 	mov	@r0, a
      00001E 22               [24]  229 	ret
      00001F 00               [12]  230 	nop
      000020                        231 sdcc_atomic_compare_exchange_pdata_impl:
      000020 E2               [24]  232 	movx	a, @r0
      000021 B5 02 02         [24]  233 	cjne	a, ar2, .+#5
      000024 EB               [12]  234 	mov	a, r3
      000025 F2               [24]  235 	movx	@r0, a
      000026 22               [24]  236 	ret
      000027 00               [12]  237 	nop
      000028                        238 sdcc_atomic_compare_exchange_xdata_impl:
      000028 E0               [24]  239 	movx	a, @dptr
      000029 B5 02 02         [24]  240 	cjne	a, ar2, .+#5
      00002C EB               [12]  241 	mov	a, r3
      00002D F0               [24]  242 	movx	@dptr, a
      00002E 22               [24]  243 	ret
      00002F                        244 sdcc_atomic_exchange_rollback_end::
                                    245 
      00002F                        246 sdcc_atomic_exchange_gptr_impl::
      00002F 30 F6 E0         [24]  247 	jnb	b.6, sdcc_atomic_exchange_xdata_impl
      000032 A8 82            [24]  248 	mov	r0, dpl
      000034 20 F5 D3         [24]  249 	jb	b.5, sdcc_atomic_exchange_pdata_impl
      000037                        250 sdcc_atomic_exchange_idata_impl:
      000037 EA               [12]  251 	mov	a, r2
      000038 C6               [12]  252 	xch	a, @r0
      000039 F5 82            [12]  253 	mov	dpl, a
      00003B 22               [24]  254 	ret
      00003C                        255 sdcc_atomic_exchange_exit:
      00003C 8B 82            [24]  256 	mov	dpl, r3
      00003E 22               [24]  257 	ret
      00003F                        258 sdcc_atomic_compare_exchange_gptr_impl::
      00003F 30 F6 E6         [24]  259 	jnb	b.6, sdcc_atomic_compare_exchange_xdata_impl
      000042 A8 82            [24]  260 	mov	r0, dpl
      000044 20 F5 D9         [24]  261 	jb	b.5, sdcc_atomic_compare_exchange_pdata_impl
      000047 80 CF            [24]  262 	sjmp	sdcc_atomic_compare_exchange_idata_impl
                                    263 ;--------------------------------------------------------
                                    264 ; global & static initialisations
                                    265 ;--------------------------------------------------------
                                    266 	.area HOME    (CODE)
                                    267 	.area GSINIT  (CODE)
                                    268 	.area GSFINAL (CODE)
                                    269 	.area GSINIT  (CODE)
                                    270 	.globl __sdcc_gsinit_startup
                                    271 	.globl __sdcc_program_startup
                                    272 	.globl __start__stack
                                    273 	.globl __mcs51_genXINIT
                                    274 	.globl __mcs51_genXRAMCLEAR
                                    275 	.globl __mcs51_genRAMCLEAR
                                    276 ;	cc2530_radio.c:109: static unsigned char mac_flags = 0;
      0000A5 75 08 00         [24]  277 	mov	_mac_flags,#0x00
                                    278 ;	cc2530_radio.c:110: static unsigned char tx_retries = 0;
      0000A8 75 09 00         [24]  279 	mov	_tx_retries,#0x00
                                    280 	.area GSFINAL (CODE)
      0000AB 02 00 49         [24]  281 	ljmp	__sdcc_program_startup
                                    282 ;--------------------------------------------------------
                                    283 ; Home
                                    284 ;--------------------------------------------------------
                                    285 	.area HOME    (CODE)
                                    286 	.area HOME    (CODE)
      000049                        287 __sdcc_program_startup:
      000049 02 03 AA         [24]  288 	ljmp	_main
                                    289 ;	return from main will return to caller
                                    290 ;--------------------------------------------------------
                                    291 ; code
                                    292 ;--------------------------------------------------------
                                    293 	.area CSEG    (CODE)
                                    294 ;------------------------------------------------------------
                                    295 ;Allocation info for local variables in function 'clock_init'
                                    296 ;------------------------------------------------------------
                                    297 ;g             Allocated to registers r6 r7 
                                    298 ;------------------------------------------------------------
                                    299 ;	cc2530_radio.c:112: static void clock_init(void){
                                    300 ;	-----------------------------------------
                                    301 ;	 function clock_init
                                    302 ;	-----------------------------------------
      0000AE                        303 _clock_init:
                           000007   304 	ar7 = 0x07
                           000006   305 	ar6 = 0x06
                           000005   306 	ar5 = 0x05
                           000004   307 	ar4 = 0x04
                           000003   308 	ar3 = 0x03
                           000002   309 	ar2 = 0x02
                           000001   310 	ar1 = 0x01
                           000000   311 	ar0 = 0x00
                                    312 ;	cc2530_radio.c:113: CLKCONCMD = 0x80;                 /* 32 MHz XOSC, 32 kHz RC */
      0000AE 75 C6 80         [24]  313 	mov	_CLKCONCMD,#0x80
                                    314 ;	cc2530_radio.c:114: { unsigned int g=0; while(!(SLEEPSTA & 0x40) && ++g){} }  /* wait XOSC stable */
      0000B1 7E 00            [12]  315 	mov	r6,#0x00
      0000B3 7F 00            [12]  316 	mov	r7,#0x00
      0000B5                        317 00102$:
      0000B5 E5 9D            [12]  318 	mov	a,_SLEEPSTA
      0000B7 20 E6 0F         [24]  319 	jb	acc.6,00105$
      0000BA 74 01            [12]  320 	mov	a,#0x01
      0000BC 2E               [12]  321 	add	a, r6
      0000BD FC               [12]  322 	mov	r4,a
      0000BE E4               [12]  323 	clr	a
      0000BF 3F               [12]  324 	addc	a, r7
      0000C0 FD               [12]  325 	mov	r5,a
      0000C1 8C 06            [24]  326 	mov	ar6,r4
      0000C3 8D 07            [24]  327 	mov	ar7,r5
      0000C5 EC               [12]  328 	mov	a,r4
      0000C6 4D               [12]  329 	orl	a,r5
      0000C7 70 EC            [24]  330 	jnz	00102$
      0000C9                        331 00105$:
                                    332 ;	cc2530_radio.c:115: }
      0000C9 22               [24]  333 	ret
                                    334 ;------------------------------------------------------------
                                    335 ;Allocation info for local variables in function 'uart_init'
                                    336 ;------------------------------------------------------------
                                    337 ;	cc2530_radio.c:116: static void uart_init(void){
                                    338 ;	-----------------------------------------
                                    339 ;	 function uart_init
                                    340 ;	-----------------------------------------
      0000CA                        341 _uart_init:
                                    342 ;	cc2530_radio.c:117: PERCFG=0x00; P0SEL|=0x0C;         /* UART0 alt1, P0.2/P0.3 */
      0000CA 75 F1 00         [24]  343 	mov	_PERCFG,#0x00
      0000CD 43 F3 0C         [24]  344 	orl	_P0SEL,#0x0c
                                    345 ;	cc2530_radio.c:118: U0CSR=0xC0;                       /* UART mode + RX enable */
      0000D0 75 86 C0         [24]  346 	mov	_U0CSR,#0xc0
                                    347 ;	cc2530_radio.c:119: U0UCR=0x02;                       /* 8N1, no flow control */
      0000D3 75 C4 02         [24]  348 	mov	_U0UCR,#0x02
                                    349 ;	cc2530_radio.c:120: U0GCR=0x0B; U0BAUD=216;           /* 115200 @ 32 MHz */
      0000D6 75 C5 0B         [24]  350 	mov	_U0GCR,#0x0b
      0000D9 75 C2 D8         [24]  351 	mov	_U0BAUD,#0xd8
                                    352 ;	cc2530_radio.c:121: }
      0000DC 22               [24]  353 	ret
                                    354 ;------------------------------------------------------------
                                    355 ;Allocation info for local variables in function 'utx'
                                    356 ;------------------------------------------------------------
                                    357 ;c             Allocated to registers 
                                    358 ;------------------------------------------------------------
                                    359 ;	cc2530_radio.c:122: static void utx(unsigned char c){ U0DBUF=c; while(!(U0CSR & 0x02)){} U0CSR &= ~0x02; }
                                    360 ;	-----------------------------------------
                                    361 ;	 function utx
                                    362 ;	-----------------------------------------
      0000DD                        363 _utx:
      0000DD 85 82 C1         [24]  364 	mov	_U0DBUF,dpl
      0000E0                        365 00101$:
      0000E0 E5 86            [12]  366 	mov	a,_U0CSR
      0000E2 30 E1 FB         [24]  367 	jnb	acc.1,00101$
      0000E5 53 86 FD         [24]  368 	anl	_U0CSR,#0xfd
      0000E8 22               [24]  369 	ret
                                    370 ;------------------------------------------------------------
                                    371 ;Allocation info for local variables in function 'urx_avail'
                                    372 ;------------------------------------------------------------
                                    373 ;	cc2530_radio.c:123: static unsigned char urx_avail(void){ return U0CSR & 0x04; }
                                    374 ;	-----------------------------------------
                                    375 ;	 function urx_avail
                                    376 ;	-----------------------------------------
      0000E9                        377 _urx_avail:
      0000E9 E5 86            [12]  378 	mov	a,_U0CSR
      0000EB 54 04            [12]  379 	anl	a,#0x04
      0000ED F5 82            [12]  380 	mov	dpl,a
      0000EF 22               [24]  381 	ret
                                    382 ;------------------------------------------------------------
                                    383 ;Allocation info for local variables in function 'urx'
                                    384 ;------------------------------------------------------------
                                    385 ;	cc2530_radio.c:124: static unsigned char urx(void){ while(!(U0CSR & 0x04)){} return U0DBUF; }
                                    386 ;	-----------------------------------------
                                    387 ;	 function urx
                                    388 ;	-----------------------------------------
      0000F0                        389 _urx:
      0000F0                        390 00101$:
      0000F0 E5 86            [12]  391 	mov	a,_U0CSR
      0000F2 30 E2 FB         [24]  392 	jnb	acc.2,00101$
      0000F5 85 C1 82         [24]  393 	mov	dpl, _U0DBUF
      0000F8 22               [24]  394 	ret
                                    395 ;------------------------------------------------------------
                                    396 ;Allocation info for local variables in function 'apply_mac'
                                    397 ;------------------------------------------------------------
                                    398 ;	cc2530_radio.c:126: static void apply_mac(void){
                                    399 ;	-----------------------------------------
                                    400 ;	 function apply_mac
                                    401 ;	-----------------------------------------
      0000F9                        402 _apply_mac:
                                    403 ;	cc2530_radio.c:130: ((mac_flags & MAC_FLAG_FILTER) ? FRMFILT0_FRAME_FILTER : 0x00));
      0000F9 E5 08            [12]  404 	mov	a,_mac_flags
      0000FB 30 E0 04         [24]  405 	jnb	acc.0,00103$
      0000FE 7F 01            [12]  406 	mov	r7,#0x01
      000100 80 02            [24]  407 	sjmp	00104$
      000102                        408 00103$:
      000102 7F 00            [12]  409 	mov	r7,#0x00
      000104                        410 00104$:
      000104 90 61 80         [24]  411 	mov	dptr,#_FRMFILT0
      000107 74 0C            [12]  412 	mov	a,#0x0c
      000109 4F               [12]  413 	orl	a,r7
      00010A F0               [24]  414 	movx	@dptr,a
                                    415 ;	cc2530_radio.c:132: ((mac_flags & MAC_FLAG_AUTOACK) ? FRMCTRL0_AUTOACK : 0x00));
      00010B E5 08            [12]  416 	mov	a,_mac_flags
      00010D 30 E1 04         [24]  417 	jnb	acc.1,00105$
      000110 7F 20            [12]  418 	mov	r7,#0x20
      000112 80 02            [24]  419 	sjmp	00106$
      000114                        420 00105$:
      000114 7F 00            [12]  421 	mov	r7,#0x00
      000116                        422 00106$:
      000116 90 61 89         [24]  423 	mov	dptr,#_FRMCTRL0
      000119 74 40            [12]  424 	mov	a,#0x40
      00011B 4F               [12]  425 	orl	a,r7
      00011C F0               [24]  426 	movx	@dptr,a
                                    427 ;	cc2530_radio.c:133: }
      00011D 22               [24]  428 	ret
                                    429 ;------------------------------------------------------------
                                    430 ;Allocation info for local variables in function 'set_address'
                                    431 ;------------------------------------------------------------
                                    432 ;d             Allocated to registers r6 r7 
                                    433 ;------------------------------------------------------------
                                    434 ;	cc2530_radio.c:135: static void set_address(__xdata unsigned char* d){
                                    435 ;	-----------------------------------------
                                    436 ;	 function set_address
                                    437 ;	-----------------------------------------
      00011E                        438 _set_address:
                                    439 ;	cc2530_radio.c:136: PAN_ID0=d[0]; PAN_ID1=d[1];
      00011E AE 82            [24]  440 	mov	r6,dpl
      000120 AF 83            [24]  441 	mov  r7,dph
      000122 E0               [24]  442 	movx	a,@dptr
      000123 90 61 72         [24]  443 	mov	dptr,#_PAN_ID0
      000126 F0               [24]  444 	movx	@dptr,a
      000127 8E 82            [24]  445 	mov	dpl,r6
      000129 8F 83            [24]  446 	mov	dph,r7
      00012B A3               [24]  447 	inc	dptr
      00012C E0               [24]  448 	movx	a,@dptr
      00012D 90 61 73         [24]  449 	mov	dptr,#_PAN_ID1
      000130 F0               [24]  450 	movx	@dptr,a
                                    451 ;	cc2530_radio.c:137: SHORT_ADDR0=d[2]; SHORT_ADDR1=d[3];
      000131 8E 82            [24]  452 	mov	dpl,r6
      000133 8F 83            [24]  453 	mov	dph,r7
      000135 A3               [24]  454 	inc	dptr
      000136 A3               [24]  455 	inc	dptr
      000137 E0               [24]  456 	movx	a,@dptr
      000138 90 61 74         [24]  457 	mov	dptr,#_SHORT_ADDR0
      00013B F0               [24]  458 	movx	@dptr,a
      00013C 8E 82            [24]  459 	mov	dpl,r6
      00013E 8F 83            [24]  460 	mov	dph,r7
      000140 A3               [24]  461 	inc	dptr
      000141 A3               [24]  462 	inc	dptr
      000142 A3               [24]  463 	inc	dptr
      000143 E0               [24]  464 	movx	a,@dptr
      000144 90 61 75         [24]  465 	mov	dptr,#_SHORT_ADDR1
      000147 F0               [24]  466 	movx	@dptr,a
                                    467 ;	cc2530_radio.c:138: EXT_ADDR0=d[4]; EXT_ADDR1=d[5]; EXT_ADDR2=d[6]; EXT_ADDR3=d[7];
      000148 8E 82            [24]  468 	mov	dpl,r6
      00014A 8F 83            [24]  469 	mov	dph,r7
      00014C A3               [24]  470 	inc	dptr
      00014D A3               [24]  471 	inc	dptr
      00014E A3               [24]  472 	inc	dptr
      00014F A3               [24]  473 	inc	dptr
      000150 E0               [24]  474 	movx	a,@dptr
      000151 90 61 6A         [24]  475 	mov	dptr,#_EXT_ADDR0
      000154 F0               [24]  476 	movx	@dptr,a
      000155 8E 82            [24]  477 	mov	dpl,r6
      000157 8F 83            [24]  478 	mov	dph,r7
      000159 A3               [24]  479 	inc	dptr
      00015A A3               [24]  480 	inc	dptr
      00015B A3               [24]  481 	inc	dptr
      00015C A3               [24]  482 	inc	dptr
      00015D A3               [24]  483 	inc	dptr
      00015E E0               [24]  484 	movx	a,@dptr
      00015F 90 61 6B         [24]  485 	mov	dptr,#_EXT_ADDR1
      000162 F0               [24]  486 	movx	@dptr,a
      000163 74 06            [12]  487 	mov	a,#0x06
      000165 2E               [12]  488 	add	a, r6
      000166 F5 82            [12]  489 	mov	dpl,a
      000168 E4               [12]  490 	clr	a
      000169 3F               [12]  491 	addc	a, r7
      00016A F5 83            [12]  492 	mov	dph,a
      00016C E0               [24]  493 	movx	a,@dptr
      00016D 90 61 6C         [24]  494 	mov	dptr,#_EXT_ADDR2
      000170 F0               [24]  495 	movx	@dptr,a
      000171 74 07            [12]  496 	mov	a,#0x07
      000173 2E               [12]  497 	add	a, r6
      000174 F5 82            [12]  498 	mov	dpl,a
      000176 E4               [12]  499 	clr	a
      000177 3F               [12]  500 	addc	a, r7
      000178 F5 83            [12]  501 	mov	dph,a
      00017A E0               [24]  502 	movx	a,@dptr
      00017B 90 61 6D         [24]  503 	mov	dptr,#_EXT_ADDR3
      00017E F0               [24]  504 	movx	@dptr,a
                                    505 ;	cc2530_radio.c:139: EXT_ADDR4=d[8]; EXT_ADDR5=d[9]; EXT_ADDR6=d[10]; EXT_ADDR7=d[11];
      00017F 74 08            [12]  506 	mov	a,#0x08
      000181 2E               [12]  507 	add	a, r6
      000182 F5 82            [12]  508 	mov	dpl,a
      000184 E4               [12]  509 	clr	a
      000185 3F               [12]  510 	addc	a, r7
      000186 F5 83            [12]  511 	mov	dph,a
      000188 E0               [24]  512 	movx	a,@dptr
      000189 90 61 6E         [24]  513 	mov	dptr,#_EXT_ADDR4
      00018C F0               [24]  514 	movx	@dptr,a
      00018D 74 09            [12]  515 	mov	a,#0x09
      00018F 2E               [12]  516 	add	a, r6
      000190 F5 82            [12]  517 	mov	dpl,a
      000192 E4               [12]  518 	clr	a
      000193 3F               [12]  519 	addc	a, r7
      000194 F5 83            [12]  520 	mov	dph,a
      000196 E0               [24]  521 	movx	a,@dptr
      000197 90 61 6F         [24]  522 	mov	dptr,#_EXT_ADDR5
      00019A F0               [24]  523 	movx	@dptr,a
      00019B 74 0A            [12]  524 	mov	a,#0x0a
      00019D 2E               [12]  525 	add	a, r6
      00019E F5 82            [12]  526 	mov	dpl,a
      0001A0 E4               [12]  527 	clr	a
      0001A1 3F               [12]  528 	addc	a, r7
      0001A2 F5 83            [12]  529 	mov	dph,a
      0001A4 E0               [24]  530 	movx	a,@dptr
      0001A5 90 61 70         [24]  531 	mov	dptr,#_EXT_ADDR6
      0001A8 F0               [24]  532 	movx	@dptr,a
      0001A9 74 0B            [12]  533 	mov	a,#0x0b
      0001AB 2E               [12]  534 	add	a, r6
      0001AC F5 82            [12]  535 	mov	dpl,a
      0001AE E4               [12]  536 	clr	a
      0001AF 3F               [12]  537 	addc	a, r7
      0001B0 F5 83            [12]  538 	mov	dph,a
      0001B2 E0               [24]  539 	movx	a,@dptr
      0001B3 90 61 71         [24]  540 	mov	dptr,#_EXT_ADDR7
      0001B6 F0               [24]  541 	movx	@dptr,a
                                    542 ;	cc2530_radio.c:140: }
      0001B7 22               [24]  543 	ret
                                    544 ;------------------------------------------------------------
                                    545 ;Allocation info for local variables in function 'radio_init'
                                    546 ;------------------------------------------------------------
                                    547 ;ch            Allocated to registers r7 
                                    548 ;------------------------------------------------------------
                                    549 ;	cc2530_radio.c:142: static void radio_init(unsigned char ch){
                                    550 ;	-----------------------------------------
                                    551 ;	 function radio_init
                                    552 ;	-----------------------------------------
      0001B8                        553 _radio_init:
      0001B8 AF 82            [24]  554 	mov	r7, dpl
                                    555 ;	cc2530_radio.c:143: apply_mac();
      0001BA C0 07            [24]  556 	push	ar7
      0001BC 12 00 F9         [24]  557 	lcall	_apply_mac
      0001BF D0 07            [24]  558 	pop	ar7
                                    559 ;	cc2530_radio.c:144: TXFILTCFG=0x09; AGCCTRL1=0x15; FSCAL1=0x00; RXCTRL=0x3F; FSCTRL=0x55;
      0001C1 90 61 FA         [24]  560 	mov	dptr,#_TXFILTCFG
      0001C4 74 09            [12]  561 	mov	a,#0x09
      0001C6 F0               [24]  562 	movx	@dptr,a
      0001C7 90 61 B2         [24]  563 	mov	dptr,#_AGCCTRL1
      0001CA 74 15            [12]  564 	mov	a,#0x15
      0001CC F0               [24]  565 	movx	@dptr,a
      0001CD 90 61 AE         [24]  566 	mov	dptr,#_FSCAL1
      0001D0 E4               [12]  567 	clr	a
      0001D1 F0               [24]  568 	movx	@dptr,a
      0001D2 90 61 AB         [24]  569 	mov	dptr,#_RXCTRL
      0001D5 74 3F            [12]  570 	mov	a,#0x3f
      0001D7 F0               [24]  571 	movx	@dptr,a
      0001D8 90 61 AC         [24]  572 	mov	dptr,#_FSCTRL
      0001DB 74 55            [12]  573 	mov	a,#0x55
      0001DD F0               [24]  574 	movx	@dptr,a
                                    575 ;	cc2530_radio.c:145: ADCTEST0=0x10; ADCTEST1=0x0E; ADCTEST2=0x03;
      0001DE 90 61 B5         [24]  576 	mov	dptr,#_ADCTEST0
      0001E1 74 10            [12]  577 	mov	a,#0x10
      0001E3 F0               [24]  578 	movx	@dptr,a
      0001E4 90 61 B6         [24]  579 	mov	dptr,#_ADCTEST1
      0001E7 74 0E            [12]  580 	mov	a,#0x0e
      0001E9 F0               [24]  581 	movx	@dptr,a
      0001EA 90 61 B7         [24]  582 	mov	dptr,#_ADCTEST2
      0001ED 74 03            [12]  583 	mov	a,#0x03
      0001EF F0               [24]  584 	movx	@dptr,a
                                    585 ;	cc2530_radio.c:146: if(ch<11) ch=11; if(ch>26) ch=26;
      0001F0 BF 0B 00         [24]  586 	cjne	r7,#0x0b,00119$
      0001F3                        587 00119$:
      0001F3 50 02            [24]  588 	jnc	00102$
      0001F5 7F 0B            [12]  589 	mov	r7,#0x0b
      0001F7                        590 00102$:
      0001F7 EF               [12]  591 	mov	a,r7
      0001F8 24 E5            [12]  592 	add	a,#0xff - 0x1a
      0001FA 50 02            [24]  593 	jnc	00104$
      0001FC 7F 1A            [12]  594 	mov	r7,#0x1a
      0001FE                        595 00104$:
                                    596 ;	cc2530_radio.c:147: FREQCTRL=(unsigned char)(11 + 5*(ch-11));
      0001FE EF               [12]  597 	mov	a,r7
      0001FF 24 F5            [12]  598 	add	a,#0xf5
      000201 75 F0 05         [24]  599 	mov	b,#0x05
      000204 A4               [48]  600 	mul	ab
      000205 24 0B            [12]  601 	add	a, #0x0b
      000207 90 61 8F         [24]  602 	mov	dptr,#_FREQCTRL
      00020A F0               [24]  603 	movx	@dptr,a
                                    604 ;	cc2530_radio.c:148: TXPOWER=0xF5;
      00020B 90 61 90         [24]  605 	mov	dptr,#_TXPOWER
      00020E 74 F5            [12]  606 	mov	a,#0xf5
      000210 F0               [24]  607 	movx	@dptr,a
                                    608 ;	cc2530_radio.c:149: RFST=STROBE_SFLUSHRX;             /* flush RX */
      000211 75 E1 ED         [24]  609 	mov	_RFST,#0xed
                                    610 ;	cc2530_radio.c:150: RFST=STROBE_SRXON;                /* enter RX */
      000214 75 E1 E3         [24]  611 	mov	_RFST,#0xe3
                                    612 ;	cc2530_radio.c:151: }
      000217 22               [24]  613 	ret
                                    614 ;------------------------------------------------------------
                                    615 ;Allocation info for local variables in function 'radio_tx_once'
                                    616 ;------------------------------------------------------------
                                    617 ;len           Allocated with name '_radio_tx_once_PARM_2'
                                    618 ;psdu          Allocated to registers r6 r7 
                                    619 ;t             Allocated to registers r6 r7 
                                    620 ;i             Allocated to registers r4 
                                    621 ;done          Allocated to registers r5 
                                    622 ;------------------------------------------------------------
                                    623 ;	cc2530_radio.c:154: static unsigned char radio_tx_once(__xdata unsigned char* psdu, unsigned char len){
                                    624 ;	-----------------------------------------
                                    625 ;	 function radio_tx_once
                                    626 ;	-----------------------------------------
      000218                        627 _radio_tx_once:
      000218 AE 82            [24]  628 	mov	r6, dpl
      00021A AF 83            [24]  629 	mov	r7, dph
                                    630 ;	cc2530_radio.c:155: unsigned int t; unsigned char i, done=0;
      00021C 7D 00            [12]  631 	mov	r5,#0x00
                                    632 ;	cc2530_radio.c:156: if(len>125) return 2;
      00021E E5 17            [12]  633 	mov	a,_radio_tx_once_PARM_2
      000220 24 82            [12]  634 	add	a,#0xff - 0x7d
      000222 50 04            [24]  635 	jnc	00102$
      000224 75 82 02         [24]  636 	mov	dpl, #0x02
      000227 22               [24]  637 	ret
      000228                        638 00102$:
                                    639 ;	cc2530_radio.c:157: RFST=STROBE_SFLUSHTX;             /* flush TX FIFO */
      000228 75 E1 EE         [24]  640 	mov	_RFST,#0xee
                                    641 ;	cc2530_radio.c:158: RFD=(unsigned char)(len+2);       /* PHR = psdu + 2 FCS */
      00022B AC 17            [24]  642 	mov	r4,_radio_tx_once_PARM_2
      00022D 0C               [12]  643 	inc	r4
      00022E 0C               [12]  644 	inc	r4
      00022F 8C D9            [24]  645 	mov	_RFD,r4
                                    646 ;	cc2530_radio.c:159: for(i=0;i<len;i++) RFD=psdu[i];
      000231 7C 00            [12]  647 	mov	r4,#0x00
      000233                        648 00108$:
      000233 C3               [12]  649 	clr	c
      000234 EC               [12]  650 	mov	a,r4
      000235 95 17            [12]  651 	subb	a,_radio_tx_once_PARM_2
      000237 50 0E            [24]  652 	jnc	00103$
      000239 EC               [12]  653 	mov	a,r4
      00023A 2E               [12]  654 	add	a, r6
      00023B F5 82            [12]  655 	mov	dpl,a
      00023D E4               [12]  656 	clr	a
      00023E 3F               [12]  657 	addc	a, r7
      00023F F5 83            [12]  658 	mov	dph,a
      000241 E0               [24]  659 	movx	a,@dptr
      000242 F5 D9            [12]  660 	mov	_RFD,a
      000244 0C               [12]  661 	inc	r4
      000245 80 EC            [24]  662 	sjmp	00108$
      000247                        663 00103$:
                                    664 ;	cc2530_radio.c:160: RFIRQF1=0;
      000247 75 91 00         [24]  665 	mov	_RFIRQF1,#0x00
                                    666 ;	cc2530_radio.c:161: RFST=(mac_flags & MAC_FLAG_CCA_TX) ? STROBE_STXONCCA : STROBE_STXON;
      00024A E5 08            [12]  667 	mov	a,_mac_flags
      00024C 30 E2 04         [24]  668 	jnb	acc.2,00114$
      00024F 7F EA            [12]  669 	mov	r7,#0xea
      000251 80 02            [24]  670 	sjmp	00115$
      000253                        671 00114$:
      000253 7F E9            [12]  672 	mov	r7,#0xe9
      000255                        673 00115$:
      000255 8F E1            [24]  674 	mov	_RFST,r7
                                    675 ;	cc2530_radio.c:162: for(t=0;t<60000;t++){ if(RFIRQF1 & IRQ_TXDONE){ done=1; break; } }
      000257 7E 00            [12]  676 	mov	r6,#0x00
      000259 7F 00            [12]  677 	mov	r7,#0x00
      00025B                        678 00110$:
      00025B E5 91            [12]  679 	mov	a,_RFIRQF1
      00025D 30 E1 04         [24]  680 	jnb	acc.1,00111$
      000260 7D 01            [12]  681 	mov	r5,#0x01
      000262 80 0E            [24]  682 	sjmp	00106$
      000264                        683 00111$:
      000264 0E               [12]  684 	inc	r6
      000265 BE 00 01         [24]  685 	cjne	r6,#0x00,00165$
      000268 0F               [12]  686 	inc	r7
      000269                        687 00165$:
      000269 C3               [12]  688 	clr	c
      00026A EE               [12]  689 	mov	a,r6
      00026B 94 60            [12]  690 	subb	a,#0x60
      00026D EF               [12]  691 	mov	a,r7
      00026E 94 EA            [12]  692 	subb	a,#0xea
      000270 40 E9            [24]  693 	jc	00110$
      000272                        694 00106$:
                                    695 ;	cc2530_radio.c:163: RFST=STROBE_SRXON;                /* back to RX */
      000272 75 E1 E3         [24]  696 	mov	_RFST,#0xe3
                                    697 ;	cc2530_radio.c:164: return done?0:1;
      000275 ED               [12]  698 	mov	a,r5
      000276 60 04            [24]  699 	jz	00116$
      000278 7F 00            [12]  700 	mov	r7,#0x00
      00027A 80 02            [24]  701 	sjmp	00117$
      00027C                        702 00116$:
      00027C 7F 01            [12]  703 	mov	r7,#0x01
      00027E                        704 00117$:
      00027E 8F 82            [24]  705 	mov	dpl,r7
                                    706 ;	cc2530_radio.c:165: }
      000280 22               [24]  707 	ret
                                    708 ;------------------------------------------------------------
                                    709 ;Allocation info for local variables in function 'radio_tx'
                                    710 ;------------------------------------------------------------
                                    711 ;len           Allocated with name '_radio_tx_PARM_2'
                                    712 ;retries       Allocated with name '_radio_tx_PARM_3'
                                    713 ;attempts      Allocated with name '_radio_tx_PARM_4'
                                    714 ;psdu          Allocated to registers r6 r7 
                                    715 ;r             Allocated to registers r5 
                                    716 ;i             Allocated to registers r3 
                                    717 ;max_attempts  Allocated with name '_radio_tx_max_attempts_10000_28'
                                    718 ;------------------------------------------------------------
                                    719 ;	cc2530_radio.c:166: static unsigned char radio_tx(__xdata unsigned char* psdu, unsigned char len,
                                    720 ;	-----------------------------------------
                                    721 ;	 function radio_tx
                                    722 ;	-----------------------------------------
      000281                        723 _radio_tx:
      000281 AE 82            [24]  724 	mov	r6, dpl
      000283 AF 83            [24]  725 	mov	r7, dph
                                    726 ;	cc2530_radio.c:168: unsigned char r=1, i, max_attempts;
      000285 7D 01            [12]  727 	mov	r5,#0x01
                                    728 ;	cc2530_radio.c:169: if(len>125){ *attempts=0; return 2; }
      000287 E5 0A            [12]  729 	mov	a,_radio_tx_PARM_2
      000289 24 82            [12]  730 	add	a,#0xff - 0x7d
      00028B 50 14            [24]  731 	jnc	00102$
      00028D AA 0C            [24]  732 	mov	r2,_radio_tx_PARM_4
      00028F AB 0D            [24]  733 	mov	r3,(_radio_tx_PARM_4 + 1)
      000291 AC 0E            [24]  734 	mov	r4,(_radio_tx_PARM_4 + 2)
      000293 8A 82            [24]  735 	mov	dpl,r2
      000295 8B 83            [24]  736 	mov	dph,r3
      000297 8C F0            [24]  737 	mov	b,r4
      000299 E4               [12]  738 	clr	a
      00029A 12 07 D9         [24]  739 	lcall	__gptrput
      00029D 75 82 02         [24]  740 	mov	dpl, #0x02
      0002A0 22               [24]  741 	ret
      0002A1                        742 00102$:
                                    743 ;	cc2530_radio.c:170: max_attempts=(unsigned char)(retries+1);
      0002A1 AC 0B            [24]  744 	mov	r4,_radio_tx_PARM_3
      0002A3 0C               [12]  745 	inc	r4
      0002A4 8C 0F            [24]  746 	mov	_radio_tx_max_attempts_10000_28,r4
                                    747 ;	cc2530_radio.c:171: for(i=0;i<max_attempts;i++){
      0002A6 7B 00            [12]  748 	mov	r3,#0x00
      0002A8                        749 00107$:
      0002A8 C3               [12]  750 	clr	c
      0002A9 EB               [12]  751 	mov	a,r3
      0002AA 95 0F            [12]  752 	subb	a,_radio_tx_max_attempts_10000_28
      0002AC 50 34            [24]  753 	jnc	00105$
                                    754 ;	cc2530_radio.c:172: r=radio_tx_once(psdu,len);
      0002AE 85 0A 17         [24]  755 	mov	_radio_tx_once_PARM_2,_radio_tx_PARM_2
      0002B1 8E 82            [24]  756 	mov	dpl, r6
      0002B3 8F 83            [24]  757 	mov	dph, r7
      0002B5 C0 07            [24]  758 	push	ar7
      0002B7 C0 06            [24]  759 	push	ar6
      0002B9 C0 03            [24]  760 	push	ar3
      0002BB 12 02 18         [24]  761 	lcall	_radio_tx_once
      0002BE AD 82            [24]  762 	mov	r5, dpl
      0002C0 D0 03            [24]  763 	pop	ar3
      0002C2 D0 06            [24]  764 	pop	ar6
      0002C4 D0 07            [24]  765 	pop	ar7
                                    766 ;	cc2530_radio.c:173: *attempts=(unsigned char)(i+1);
      0002C6 A8 0C            [24]  767 	mov	r0,_radio_tx_PARM_4
      0002C8 A9 0D            [24]  768 	mov	r1,(_radio_tx_PARM_4 + 1)
      0002CA AA 0E            [24]  769 	mov	r2,(_radio_tx_PARM_4 + 2)
      0002CC 8B 04            [24]  770 	mov	ar4,r3
      0002CE 0C               [12]  771 	inc	r4
      0002CF 88 82            [24]  772 	mov	dpl,r0
      0002D1 89 83            [24]  773 	mov	dph,r1
      0002D3 8A F0            [24]  774 	mov	b,r2
      0002D5 EC               [12]  775 	mov	a,r4
      0002D6 12 07 D9         [24]  776 	lcall	__gptrput
                                    777 ;	cc2530_radio.c:174: if(r==0) return 0;
      0002D9 ED               [12]  778 	mov	a,r5
      0002DA 70 03            [24]  779 	jnz	00108$
      0002DC F5 82            [12]  780 	mov	dpl,a
      0002DE 22               [24]  781 	ret
      0002DF                        782 00108$:
                                    783 ;	cc2530_radio.c:171: for(i=0;i<max_attempts;i++){
      0002DF 0B               [12]  784 	inc	r3
      0002E0 80 C6            [24]  785 	sjmp	00107$
      0002E2                        786 00105$:
                                    787 ;	cc2530_radio.c:176: return r;
      0002E2 8D 82            [24]  788 	mov	dpl, r5
                                    789 ;	cc2530_radio.c:177: }
      0002E4 22               [24]  790 	ret
                                    791 ;------------------------------------------------------------
                                    792 ;Allocation info for local variables in function 'radio_rx'
                                    793 ;------------------------------------------------------------
                                    794 ;len           Allocated to registers r7 
                                    795 ;i             Allocated to registers r6 
                                    796 ;guard         Allocated to registers r4 r5 
                                    797 ;------------------------------------------------------------
                                    798 ;	cc2530_radio.c:181: static unsigned char radio_rx(void){
                                    799 ;	-----------------------------------------
                                    800 ;	 function radio_rx
                                    801 ;	-----------------------------------------
      0002E5                        802 _radio_rx:
                                    803 ;	cc2530_radio.c:183: if(!(RFIRQF0 & IRQ_FIFOP)) return 0;
      0002E5 E5 E9            [12]  804 	mov	a,_RFIRQF0
      0002E7 20 E2 04         [24]  805 	jb	acc.2,00102$
      0002EA 75 82 00         [24]  806 	mov	dpl, #0x00
      0002ED 22               [24]  807 	ret
      0002EE                        808 00102$:
                                    809 ;	cc2530_radio.c:184: len = RFD;                        /* first FIFO byte = frame length */
                                    810 ;	cc2530_radio.c:185: if(len==0 || len>127){ RFST=STROBE_SFLUSHRX; RFIRQF0=0; return 0; }
      0002EE E5 D9            [12]  811 	mov	a,_RFD
      0002F0 FF               [12]  812 	mov	r7,a
      0002F1 60 05            [24]  813 	jz	00103$
      0002F3 EF               [12]  814 	mov	a,r7
      0002F4 24 80            [12]  815 	add	a,#0xff - 0x7f
      0002F6 50 0A            [24]  816 	jnc	00126$
      0002F8                        817 00103$:
      0002F8 75 E1 ED         [24]  818 	mov	_RFST,#0xed
      0002FB 75 E9 00         [24]  819 	mov	_RFIRQF0,#0x00
      0002FE 75 82 00         [24]  820 	mov	dpl, #0x00
                                    821 ;	cc2530_radio.c:192: for(i=0;i<len;i++){
      000301 22               [24]  822 	ret
      000302                        823 00126$:
      000302 7E 00            [12]  824 	mov	r6,#0x00
      000304                        825 00116$:
      000304 C3               [12]  826 	clr	c
      000305 EE               [12]  827 	mov	a,r6
      000306 9F               [12]  828 	subb	a,r7
      000307 50 31            [24]  829 	jnc	00111$
                                    830 ;	cc2530_radio.c:193: unsigned int guard=0;
      000309 7C 00            [12]  831 	mov	r4,#0x00
      00030B 7D 00            [12]  832 	mov	r5,#0x00
                                    833 ;	cc2530_radio.c:194: while(RXFIFOCNT==0){ if(++guard==0){ RFST=STROBE_SFLUSHRX; RFIRQF0=0; return 0; } }
      00030D                        834 00108$:
      00030D 90 61 9B         [24]  835 	mov	dptr,#_RXFIFOCNT
      000310 E0               [24]  836 	movx	a,@dptr
      000311 70 17            [24]  837 	jnz	00110$
      000313 74 01            [12]  838 	mov	a,#0x01
      000315 2C               [12]  839 	add	a, r4
      000316 FA               [12]  840 	mov	r2,a
      000317 E4               [12]  841 	clr	a
      000318 3D               [12]  842 	addc	a, r5
      000319 FB               [12]  843 	mov	r3,a
      00031A 8A 04            [24]  844 	mov	ar4,r2
      00031C 8B 05            [24]  845 	mov	ar5,r3
      00031E EA               [12]  846 	mov	a,r2
      00031F 4B               [12]  847 	orl	a,r3
      000320 70 EB            [24]  848 	jnz	00108$
      000322 75 E1 ED         [24]  849 	mov	_RFST,#0xed
      000325 F5 E9            [12]  850 	mov	_RFIRQF0,a
      000327 F5 82            [12]  851 	mov	dpl,a
      000329 22               [24]  852 	ret
      00032A                        853 00110$:
                                    854 ;	cc2530_radio.c:195: rxbuf[i]=RFD;                   /* psdu + RSSI + (CRC|LQI) */
      00032A EE               [12]  855 	mov	a,r6
      00032B 24 01            [12]  856 	add	a, #_rxbuf
      00032D F5 82            [12]  857 	mov	dpl,a
      00032F E4               [12]  858 	clr	a
      000330 34 00            [12]  859 	addc	a, #(_rxbuf >> 8)
      000332 F5 83            [12]  860 	mov	dph,a
      000334 E5 D9            [12]  861 	mov	a,_RFD
      000336 F0               [24]  862 	movx	@dptr,a
                                    863 ;	cc2530_radio.c:192: for(i=0;i<len;i++){
      000337 0E               [12]  864 	inc	r6
      000338 80 CA            [24]  865 	sjmp	00116$
      00033A                        866 00111$:
                                    867 ;	cc2530_radio.c:197: RFIRQF0=0;
      00033A 75 E9 00         [24]  868 	mov	_RFIRQF0,#0x00
                                    869 ;	cc2530_radio.c:198: if(RXFIFOCNT==0){} else { RFST=STROBE_SFLUSHRX; }   /* drain leftovers */
      00033D 90 61 9B         [24]  870 	mov	dptr,#_RXFIFOCNT
      000340 E0               [24]  871 	movx	a,@dptr
      000341 60 03            [24]  872 	jz	00114$
      000343 75 E1 ED         [24]  873 	mov	_RFST,#0xed
      000346                        874 00114$:
                                    875 ;	cc2530_radio.c:199: return len;
      000346 8F 82            [24]  876 	mov	dpl, r7
                                    877 ;	cc2530_radio.c:200: }
      000348 22               [24]  878 	ret
                                    879 ;------------------------------------------------------------
                                    880 ;Allocation info for local variables in function 'send_frame'
                                    881 ;------------------------------------------------------------
                                    882 ;d             Allocated with name '_send_frame_PARM_2'
                                    883 ;n             Allocated with name '_send_frame_PARM_3'
                                    884 ;resp          Allocated to registers r7 
                                    885 ;i             Allocated to registers r6 
                                    886 ;fcs           Allocated to registers r7 
                                    887 ;------------------------------------------------------------
                                    888 ;	cc2530_radio.c:201: static void send_frame(unsigned char resp, __xdata unsigned char* d, unsigned char n){
                                    889 ;	-----------------------------------------
                                    890 ;	 function send_frame
                                    891 ;	-----------------------------------------
      000349                        892 _send_frame:
      000349 AF 82            [24]  893 	mov	r7, dpl
                                    894 ;	cc2530_radio.c:203: utx(0xFE); utx((unsigned char)(n+1)); utx(resp);
      00034B 75 82 FE         [24]  895 	mov	dpl, #0xfe
      00034E C0 07            [24]  896 	push	ar7
      000350 12 00 DD         [24]  897 	lcall	_utx
      000353 AE 12            [24]  898 	mov	r6,_send_frame_PARM_3
      000355 0E               [12]  899 	inc	r6
      000356 8E 82            [24]  900 	mov	dpl,r6
      000358 C0 06            [24]  901 	push	ar6
      00035A 12 00 DD         [24]  902 	lcall	_utx
      00035D D0 06            [24]  903 	pop	ar6
      00035F D0 07            [24]  904 	pop	ar7
      000361 8F 82            [24]  905 	mov	dpl, r7
      000363 C0 07            [24]  906 	push	ar7
      000365 C0 06            [24]  907 	push	ar6
      000367 12 00 DD         [24]  908 	lcall	_utx
      00036A D0 06            [24]  909 	pop	ar6
      00036C D0 07            [24]  910 	pop	ar7
                                    911 ;	cc2530_radio.c:204: fcs=(unsigned char)((n+1) ^ resp);
      00036E EE               [12]  912 	mov	a,r6
      00036F 62 07            [12]  913 	xrl	ar7,a
                                    914 ;	cc2530_radio.c:205: for(i=0;i<n;i++){ utx(d[i]); fcs^=d[i]; }
      000371 7E 00            [12]  915 	mov	r6,#0x00
      000373                        916 00103$:
      000373 C3               [12]  917 	clr	c
      000374 EE               [12]  918 	mov	a,r6
      000375 95 12            [12]  919 	subb	a,_send_frame_PARM_3
      000377 50 2C            [24]  920 	jnc	00101$
      000379 EE               [12]  921 	mov	a,r6
      00037A 25 10            [12]  922 	add	a, _send_frame_PARM_2
      00037C FC               [12]  923 	mov	r4,a
      00037D E4               [12]  924 	clr	a
      00037E 35 11            [12]  925 	addc	a, (_send_frame_PARM_2 + 1)
      000380 FD               [12]  926 	mov	r5,a
      000381 8C 82            [24]  927 	mov	dpl,r4
      000383 8D 83            [24]  928 	mov	dph,r5
      000385 E0               [24]  929 	movx	a,@dptr
      000386 F5 82            [12]  930 	mov	dpl,a
      000388 C0 07            [24]  931 	push	ar7
      00038A C0 06            [24]  932 	push	ar6
      00038C C0 05            [24]  933 	push	ar5
      00038E C0 04            [24]  934 	push	ar4
      000390 12 00 DD         [24]  935 	lcall	_utx
      000393 D0 04            [24]  936 	pop	ar4
      000395 D0 05            [24]  937 	pop	ar5
      000397 D0 06            [24]  938 	pop	ar6
      000399 D0 07            [24]  939 	pop	ar7
      00039B 8C 82            [24]  940 	mov	dpl,r4
      00039D 8D 83            [24]  941 	mov	dph,r5
      00039F E0               [24]  942 	movx	a,@dptr
      0003A0 62 07            [12]  943 	xrl	ar7,a
      0003A2 0E               [12]  944 	inc	r6
      0003A3 80 CE            [24]  945 	sjmp	00103$
      0003A5                        946 00101$:
                                    947 ;	cc2530_radio.c:206: utx(fcs);
      0003A5 8F 82            [24]  948 	mov	dpl, r7
                                    949 ;	cc2530_radio.c:207: }
      0003A7 02 00 DD         [24]  950 	ljmp	_utx
                                    951 ;------------------------------------------------------------
                                    952 ;Allocation info for local variables in function 'main'
                                    953 ;------------------------------------------------------------
                                    954 ;st            Allocated with name '_main_st_10000_46'
                                    955 ;ln            Allocated to registers r6 
                                    956 ;idx           Allocated with name '_main_idx_10000_46'
                                    957 ;c             Allocated to registers r7 
                                    958 ;rlen          Allocated to registers r4 
                                    959 ;i             Allocated to registers r3 
                                    960 ;fcs           Allocated to registers r0 
                                    961 ;cc            Allocated to registers r7 
                                    962 ;i             Allocated to registers r4 
                                    963 ;good          Allocated to registers r5 
                                    964 ;attempts      Allocated with name '_main_attempts_80000_63'
                                    965 ;r             Allocated to registers r5 
                                    966 ;attempts      Allocated with name '_main_attempts_80000_68'
                                    967 ;r             Allocated to registers r5 
                                    968 ;cmd           Allocated with name '_main_cmd_10000_46'
                                    969 ;tmp           Allocated with name '_main_tmp_10000_46'
                                    970 ;a             Allocated with name '_main_a_20000_47'
                                    971 ;------------------------------------------------------------
                                    972 ;	cc2530_radio.c:209: void main(void){
                                    973 ;	-----------------------------------------
                                    974 ;	 function main
                                    975 ;	-----------------------------------------
      0003AA                        976 _main:
                                    977 ;	cc2530_radio.c:211: unsigned char st=0, ln=0, idx=0, c, rlen;
      0003AA 75 13 00         [24]  978 	mov	_main_st_10000_46,#0x00
      0003AD 7E 00            [12]  979 	mov	r6,#0x00
      0003AF 8E 14            [24]  980 	mov	_main_idx_10000_46,r6
                                    981 ;	cc2530_radio.c:213: clock_init();
      0003B1 C0 06            [24]  982 	push	ar6
      0003B3 12 00 AE         [24]  983 	lcall	_clock_init
                                    984 ;	cc2530_radio.c:214: uart_init();
      0003B6 12 00 CA         [24]  985 	lcall	_uart_init
                                    986 ;	cc2530_radio.c:215: { __xdata unsigned char a[12] =
      0003B9 90 01 29         [24]  987 	mov	dptr,#_main_a_20000_47
      0003BC 74 FF            [12]  988 	mov	a,#0xff
      0003BE F0               [24]  989 	movx	@dptr,a
      0003BF 90 01 2A         [24]  990 	mov	dptr,#(_main_a_20000_47 + 0x0001)
      0003C2 F0               [24]  991 	movx	@dptr,a
      0003C3 90 01 2B         [24]  992 	mov	dptr,#(_main_a_20000_47 + 0x0002)
      0003C6 F0               [24]  993 	movx	@dptr,a
      0003C7 90 01 2C         [24]  994 	mov	dptr,#(_main_a_20000_47 + 0x0003)
      0003CA F0               [24]  995 	movx	@dptr,a
      0003CB 90 01 2D         [24]  996 	mov	dptr,#(_main_a_20000_47 + 0x0004)
      0003CE E4               [12]  997 	clr	a
      0003CF F0               [24]  998 	movx	@dptr,a
      0003D0 90 01 2E         [24]  999 	mov	dptr,#(_main_a_20000_47 + 0x0005)
      0003D3 F0               [24] 1000 	movx	@dptr,a
      0003D4 90 01 2F         [24] 1001 	mov	dptr,#(_main_a_20000_47 + 0x0006)
      0003D7 F0               [24] 1002 	movx	@dptr,a
      0003D8 90 01 30         [24] 1003 	mov	dptr,#(_main_a_20000_47 + 0x0007)
      0003DB F0               [24] 1004 	movx	@dptr,a
      0003DC 90 01 31         [24] 1005 	mov	dptr,#(_main_a_20000_47 + 0x0008)
      0003DF F0               [24] 1006 	movx	@dptr,a
      0003E0 90 01 32         [24] 1007 	mov	dptr,#(_main_a_20000_47 + 0x0009)
      0003E3 F0               [24] 1008 	movx	@dptr,a
      0003E4 90 01 33         [24] 1009 	mov	dptr,#(_main_a_20000_47 + 0x000a)
      0003E7 F0               [24] 1010 	movx	@dptr,a
      0003E8 90 01 34         [24] 1011 	mov	dptr,#(_main_a_20000_47 + 0x000b)
      0003EB F0               [24] 1012 	movx	@dptr,a
                                   1013 ;	cc2530_radio.c:217: set_address(a);
      0003EC 90 01 29         [24] 1014 	mov	dptr,#_main_a_20000_47
      0003EF 12 01 1E         [24] 1015 	lcall	_set_address
                                   1016 ;	cc2530_radio.c:219: radio_init(11);
      0003F2 75 82 0B         [24] 1017 	mov	dpl, #0x0b
      0003F5 12 01 B8         [24] 1018 	lcall	_radio_init
                                   1019 ;	cc2530_radio.c:220: tmp[0]=FW_VER_HI; tmp[1]=FW_VER_LO;
      0003F8 90 01 19         [24] 1020 	mov	dptr,#_main_tmp_10000_46
      0003FB E4               [12] 1021 	clr	a
      0003FC F0               [24] 1022 	movx	@dptr,a
      0003FD 90 01 1A         [24] 1023 	mov	dptr,#(_main_tmp_10000_46 + 0x0001)
      000400 74 04            [12] 1024 	mov	a,#0x04
      000402 F0               [24] 1025 	movx	@dptr,a
                                   1026 ;	cc2530_radio.c:221: send_frame(RSP_RESET_IND, tmp, 2);
      000403 75 10 19         [24] 1027 	mov	_send_frame_PARM_2,#_main_tmp_10000_46
      000406 75 11 01         [24] 1028 	mov	(_send_frame_PARM_2 + 1),#(_main_tmp_10000_46 >> 8)
      000409 75 12 02         [24] 1029 	mov	_send_frame_PARM_3,#0x02
      00040C 75 82 80         [24] 1030 	mov	dpl, #0x80
      00040F 12 03 49         [24] 1031 	lcall	_send_frame
      000412 D0 06            [24] 1032 	pop	ar6
      000414                       1033 00162$:
                                   1034 ;	cc2530_radio.c:224: rlen = radio_rx();
      000414 C0 06            [24] 1035 	push	ar6
      000416 12 02 E5         [24] 1036 	lcall	_radio_rx
      000419 AC 82            [24] 1037 	mov	r4, dpl
      00041B D0 06            [24] 1038 	pop	ar6
                                   1039 ;	cc2530_radio.c:225: if(rlen >= 2){
      00041D BC 02 00         [24] 1040 	cjne	r4,#0x02,00378$
      000420                       1041 00378$:
      000420 50 03            [24] 1042 	jnc	00379$
      000422 02 05 19         [24] 1043 	ljmp	00103$
      000425                       1044 00379$:
                                   1045 ;	cc2530_radio.c:227: utx(0xFE); utx((unsigned char)(rlen+1)); utx(RSP_RX_FRAME);
      000425 75 82 FE         [24] 1046 	mov	dpl, #0xfe
      000428 C0 06            [24] 1047 	push	ar6
      00042A C0 04            [24] 1048 	push	ar4
      00042C 12 00 DD         [24] 1049 	lcall	_utx
      00042F D0 04            [24] 1050 	pop	ar4
      000431 8C 03            [24] 1051 	mov	ar3,r4
      000433 0B               [12] 1052 	inc	r3
      000434 8B 82            [24] 1053 	mov	dpl,r3
      000436 C0 04            [24] 1054 	push	ar4
      000438 C0 03            [24] 1055 	push	ar3
      00043A 12 00 DD         [24] 1056 	lcall	_utx
      00043D 75 82 84         [24] 1057 	mov	dpl, #0x84
      000440 12 00 DD         [24] 1058 	lcall	_utx
      000443 D0 03            [24] 1059 	pop	ar3
      000445 D0 04            [24] 1060 	pop	ar4
                                   1061 ;	cc2530_radio.c:228: { unsigned char i, fcs=(unsigned char)((rlen+1)^RSP_RX_FRAME);
      000447 63 03 84         [24] 1062 	xrl	ar3,#0x84
                                   1063 ;	cc2530_radio.c:229: utx(rxbuf[rlen-2]); fcs^=rxbuf[rlen-2];     /* RSSI */
      00044A 8C 02            [24] 1064 	mov	ar2,r4
      00044C 7C 00            [12] 1065 	mov	r4,#0x00
      00044E EA               [12] 1066 	mov	a,r2
      00044F 24 FE            [12] 1067 	add	a,#0xfe
      000451 F8               [12] 1068 	mov	r0,a
      000452 EC               [12] 1069 	mov	a,r4
      000453 34 FF            [12] 1070 	addc	a,#0xff
      000455 F9               [12] 1071 	mov	r1,a
      000456 E8               [12] 1072 	mov	a,r0
      000457 24 01            [12] 1073 	add	a, #_rxbuf
      000459 F8               [12] 1074 	mov	r0,a
      00045A E9               [12] 1075 	mov	a,r1
      00045B 34 00            [12] 1076 	addc	a, #(_rxbuf >> 8)
      00045D F9               [12] 1077 	mov	r1,a
      00045E 88 82            [24] 1078 	mov	dpl,r0
      000460 89 83            [24] 1079 	mov	dph,r1
      000462 E0               [24] 1080 	movx	a,@dptr
      000463 F5 82            [12] 1081 	mov	dpl,a
      000465 C0 04            [24] 1082 	push	ar4
      000467 C0 03            [24] 1083 	push	ar3
      000469 C0 02            [24] 1084 	push	ar2
      00046B C0 01            [24] 1085 	push	ar1
      00046D C0 00            [24] 1086 	push	ar0
      00046F 12 00 DD         [24] 1087 	lcall	_utx
      000472 D0 00            [24] 1088 	pop	ar0
      000474 D0 01            [24] 1089 	pop	ar1
      000476 D0 02            [24] 1090 	pop	ar2
      000478 D0 03            [24] 1091 	pop	ar3
      00047A D0 04            [24] 1092 	pop	ar4
      00047C 88 82            [24] 1093 	mov	dpl,r0
      00047E 89 83            [24] 1094 	mov	dph,r1
      000480 E0               [24] 1095 	movx	a,@dptr
      000481 F8               [12] 1096 	mov	r0,a
      000482 EB               [12] 1097 	mov	a,r3
      000483 62 00            [12] 1098 	xrl	ar0,a
                                   1099 ;	cc2530_radio.c:230: utx(rxbuf[rlen-1]); fcs^=rxbuf[rlen-1];     /* CRC|LQI */
      000485 EA               [12] 1100 	mov	a,r2
      000486 24 FF            [12] 1101 	add	a,#0xff
      000488 FB               [12] 1102 	mov	r3,a
      000489 EC               [12] 1103 	mov	a,r4
      00048A 34 FF            [12] 1104 	addc	a,#0xff
      00048C FD               [12] 1105 	mov	r5,a
      00048D EB               [12] 1106 	mov	a,r3
      00048E 24 01            [12] 1107 	add	a, #_rxbuf
      000490 FB               [12] 1108 	mov	r3,a
      000491 ED               [12] 1109 	mov	a,r5
      000492 34 00            [12] 1110 	addc	a, #(_rxbuf >> 8)
      000494 FD               [12] 1111 	mov	r5,a
      000495 8B 82            [24] 1112 	mov	dpl,r3
      000497 8D 83            [24] 1113 	mov	dph,r5
      000499 E0               [24] 1114 	movx	a,@dptr
      00049A F5 82            [12] 1115 	mov	dpl,a
      00049C C0 05            [24] 1116 	push	ar5
      00049E C0 04            [24] 1117 	push	ar4
      0004A0 C0 03            [24] 1118 	push	ar3
      0004A2 C0 02            [24] 1119 	push	ar2
      0004A4 C0 00            [24] 1120 	push	ar0
      0004A6 12 00 DD         [24] 1121 	lcall	_utx
      0004A9 D0 00            [24] 1122 	pop	ar0
      0004AB D0 02            [24] 1123 	pop	ar2
      0004AD D0 03            [24] 1124 	pop	ar3
      0004AF D0 04            [24] 1125 	pop	ar4
      0004B1 D0 05            [24] 1126 	pop	ar5
      0004B3 D0 06            [24] 1127 	pop	ar6
      0004B5 8B 82            [24] 1128 	mov	dpl,r3
      0004B7 8D 83            [24] 1129 	mov	dph,r5
      0004B9 E0               [24] 1130 	movx	a,@dptr
      0004BA 68               [12] 1131 	xrl	a,r0
      0004BB FD               [12] 1132 	mov	r5,a
                                   1133 ;	cc2530_radio.c:231: for(i=0;i+2<rlen;i++){ utx(rxbuf[i]); fcs^=rxbuf[i]; }
      0004BC 7B 00            [12] 1134 	mov	r3,#0x00
      0004BE                       1135 00157$:
      0004BE 8B 00            [24] 1136 	mov	ar0,r3
      0004C0 79 00            [12] 1137 	mov	r1,#0x00
      0004C2 74 02            [12] 1138 	mov	a,#0x02
      0004C4 28               [12] 1139 	add	a, r0
      0004C5 F8               [12] 1140 	mov	r0,a
      0004C6 E4               [12] 1141 	clr	a
      0004C7 39               [12] 1142 	addc	a, r1
      0004C8 F9               [12] 1143 	mov	r1,a
      0004C9 C3               [12] 1144 	clr	c
      0004CA E8               [12] 1145 	mov	a,r0
      0004CB 9A               [12] 1146 	subb	a,r2
      0004CC E9               [12] 1147 	mov	a,r1
      0004CD 64 80            [12] 1148 	xrl	a,#0x80
      0004CF 8C F0            [24] 1149 	mov	b,r4
      0004D1 63 F0 80         [24] 1150 	xrl	b,#0x80
      0004D4 95 F0            [12] 1151 	subb	a,b
      0004D6 50 38            [24] 1152 	jnc	00101$
      0004D8 EB               [12] 1153 	mov	a,r3
      0004D9 24 01            [12] 1154 	add	a, #_rxbuf
      0004DB F8               [12] 1155 	mov	r0,a
      0004DC E4               [12] 1156 	clr	a
      0004DD 34 00            [12] 1157 	addc	a, #(_rxbuf >> 8)
      0004DF F9               [12] 1158 	mov	r1,a
      0004E0 88 82            [24] 1159 	mov	dpl,r0
      0004E2 89 83            [24] 1160 	mov	dph,r1
      0004E4 E0               [24] 1161 	movx	a,@dptr
      0004E5 F5 82            [12] 1162 	mov	dpl,a
      0004E7 C0 06            [24] 1163 	push	ar6
      0004E9 C0 05            [24] 1164 	push	ar5
      0004EB C0 04            [24] 1165 	push	ar4
      0004ED C0 03            [24] 1166 	push	ar3
      0004EF C0 02            [24] 1167 	push	ar2
      0004F1 C0 01            [24] 1168 	push	ar1
      0004F3 C0 00            [24] 1169 	push	ar0
      0004F5 12 00 DD         [24] 1170 	lcall	_utx
      0004F8 D0 00            [24] 1171 	pop	ar0
      0004FA D0 01            [24] 1172 	pop	ar1
      0004FC D0 02            [24] 1173 	pop	ar2
      0004FE D0 03            [24] 1174 	pop	ar3
      000500 D0 04            [24] 1175 	pop	ar4
      000502 D0 05            [24] 1176 	pop	ar5
      000504 D0 06            [24] 1177 	pop	ar6
      000506 88 82            [24] 1178 	mov	dpl,r0
      000508 89 83            [24] 1179 	mov	dph,r1
      00050A E0               [24] 1180 	movx	a,@dptr
      00050B 62 05            [12] 1181 	xrl	ar5,a
      00050D 0B               [12] 1182 	inc	r3
      00050E 80 AE            [24] 1183 	sjmp	00157$
      000510                       1184 00101$:
                                   1185 ;	cc2530_radio.c:232: utx(fcs);
      000510 8D 82            [24] 1186 	mov	dpl, r5
      000512 C0 06            [24] 1187 	push	ar6
      000514 12 00 DD         [24] 1188 	lcall	_utx
      000517 D0 06            [24] 1189 	pop	ar6
      000519                       1190 00103$:
                                   1191 ;	cc2530_radio.c:236: if(urx_avail()){
      000519 C0 06            [24] 1192 	push	ar6
      00051B 12 00 E9         [24] 1193 	lcall	_urx_avail
      00051E E5 82            [12] 1194 	mov	a, dpl
      000520 D0 06            [24] 1195 	pop	ar6
      000522 70 03            [24] 1196 	jnz	00381$
      000524 02 04 14         [24] 1197 	ljmp	00162$
      000527                       1198 00381$:
                                   1199 ;	cc2530_radio.c:237: c=urx();
      000527 C0 06            [24] 1200 	push	ar6
      000529 12 00 F0         [24] 1201 	lcall	_urx
      00052C AF 82            [24] 1202 	mov	r7, dpl
      00052E D0 06            [24] 1203 	pop	ar6
                                   1204 ;	cc2530_radio.c:238: switch(st){
      000530 E4               [12] 1205 	clr	a
      000531 B5 13 02         [24] 1206 	cjne	a,_main_st_10000_46,00382$
      000534 80 11            [24] 1207 	sjmp	00104$
      000536                       1208 00382$:
      000536 74 01            [12] 1209 	mov	a,#0x01
      000538 B5 13 02         [24] 1210 	cjne	a,_main_st_10000_46,00383$
      00053B 80 18            [24] 1211 	sjmp	00107$
      00053D                       1212 00383$:
      00053D 74 02            [12] 1213 	mov	a,#0x02
      00053F B5 13 02         [24] 1214 	cjne	a,_main_st_10000_46,00384$
      000542 80 2A            [24] 1215 	sjmp	00111$
      000544                       1216 00384$:
      000544 02 04 14         [24] 1217 	ljmp	00162$
                                   1218 ;	cc2530_radio.c:239: case 0: if(c==0xFE){ st=1; } break;
      000547                       1219 00104$:
      000547 BF FE 02         [24] 1220 	cjne	r7,#0xfe,00385$
      00054A 80 03            [24] 1221 	sjmp	00386$
      00054C                       1222 00385$:
      00054C 02 04 14         [24] 1223 	ljmp	00162$
      00054F                       1224 00386$:
      00054F 75 13 01         [24] 1225 	mov	_main_st_10000_46,#0x01
      000552 02 04 14         [24] 1226 	ljmp	00162$
                                   1227 ;	cc2530_radio.c:240: case 1: ln=c; idx=0; st=2; if(ln==0||ln>137){ st=0; } break;
      000555                       1228 00107$:
      000555 8F 06            [24] 1229 	mov	ar6,r7
      000557 75 14 00         [24] 1230 	mov	_main_idx_10000_46,#0x00
      00055A 75 13 02         [24] 1231 	mov	_main_st_10000_46,#0x02
      00055D EF               [12] 1232 	mov	a,r7
      00055E 60 08            [24] 1233 	jz	00108$
      000560 EF               [12] 1234 	mov	a,r7
      000561 24 76            [12] 1235 	add	a,#0xff - 0x89
      000563 40 03            [24] 1236 	jc	00388$
      000565 02 04 14         [24] 1237 	ljmp	00162$
      000568                       1238 00388$:
      000568                       1239 00108$:
      000568 75 13 00         [24] 1240 	mov	_main_st_10000_46,#0x00
      00056B 02 04 14         [24] 1241 	ljmp	00162$
                                   1242 ;	cc2530_radio.c:241: case 2:
      00056E                       1243 00111$:
                                   1244 ;	cc2530_radio.c:242: cmd[idx++]=c;
      00056E AD 14            [24] 1245 	mov	r5,_main_idx_10000_46
      000570 05 14            [12] 1246 	inc	_main_idx_10000_46
      000572 ED               [12] 1247 	mov	a,r5
      000573 24 8D            [12] 1248 	add	a, #_main_cmd_10000_46
      000575 F5 82            [12] 1249 	mov	dpl,a
      000577 E4               [12] 1250 	clr	a
      000578 34 00            [12] 1251 	addc	a, #(_main_cmd_10000_46 >> 8)
      00057A F5 83            [12] 1252 	mov	dph,a
      00057C EF               [12] 1253 	mov	a,r7
      00057D F0               [24] 1254 	movx	@dptr,a
                                   1255 ;	cc2530_radio.c:243: if(idx>=ln+1){                 /* got LEN payload bytes + FCS */
      00057E 8E 05            [24] 1256 	mov	ar5,r6
      000580 7F 00            [12] 1257 	mov	r7,#0x00
      000582 0D               [12] 1258 	inc	r5
      000583 BD 00 01         [24] 1259 	cjne	r5,#0x00,00389$
      000586 0F               [12] 1260 	inc	r7
      000587                       1261 00389$:
      000587 AB 14            [24] 1262 	mov	r3,_main_idx_10000_46
      000589 7C 00            [12] 1263 	mov	r4,#0x00
      00058B C3               [12] 1264 	clr	c
      00058C EB               [12] 1265 	mov	a,r3
      00058D 9D               [12] 1266 	subb	a,r5
      00058E EC               [12] 1267 	mov	a,r4
      00058F 64 80            [12] 1268 	xrl	a,#0x80
      000591 8F F0            [24] 1269 	mov	b,r7
      000593 63 F0 80         [24] 1270 	xrl	b,#0x80
      000596 95 F0            [12] 1271 	subb	a,b
      000598 50 03            [24] 1272 	jnc	00390$
      00059A 02 04 14         [24] 1273 	ljmp	00162$
      00059D                       1274 00390$:
                                   1275 ;	cc2530_radio.c:244: unsigned char cc=cmd[0], i, good=ln;
      00059D 90 00 8D         [24] 1276 	mov	dptr,#_main_cmd_10000_46
      0005A0 E0               [24] 1277 	movx	a,@dptr
      0005A1 FF               [12] 1278 	mov	r7,a
      0005A2 8E 05            [24] 1279 	mov	ar5,r6
                                   1280 ;	cc2530_radio.c:245: for(i=0;i<ln;i++) good^=cmd[i];
      0005A4 7C 00            [12] 1281 	mov	r4,#0x00
      0005A6                       1282 00160$:
      0005A6 C3               [12] 1283 	clr	c
      0005A7 EC               [12] 1284 	mov	a,r4
      0005A8 9E               [12] 1285 	subb	a,r6
      0005A9 50 10            [24] 1286 	jnc	00112$
      0005AB EC               [12] 1287 	mov	a,r4
      0005AC 24 8D            [12] 1288 	add	a, #_main_cmd_10000_46
      0005AE F5 82            [12] 1289 	mov	dpl,a
      0005B0 E4               [12] 1290 	clr	a
      0005B1 34 00            [12] 1291 	addc	a, #(_main_cmd_10000_46 >> 8)
      0005B3 F5 83            [12] 1292 	mov	dph,a
      0005B5 E0               [24] 1293 	movx	a,@dptr
      0005B6 62 05            [12] 1294 	xrl	ar5,a
      0005B8 0C               [12] 1295 	inc	r4
      0005B9 80 EB            [24] 1296 	sjmp	00160$
      0005BB                       1297 00112$:
                                   1298 ;	cc2530_radio.c:246: if(good==cmd[ln]){
      0005BB EE               [12] 1299 	mov	a,r6
      0005BC 24 8D            [12] 1300 	add	a, #_main_cmd_10000_46
      0005BE F5 82            [12] 1301 	mov	dpl,a
      0005C0 E4               [12] 1302 	clr	a
      0005C1 34 00            [12] 1303 	addc	a, #(_main_cmd_10000_46 >> 8)
      0005C3 F5 83            [12] 1304 	mov	dph,a
      0005C5 E0               [24] 1305 	movx	a,@dptr
      0005C6 FC               [12] 1306 	mov	r4,a
      0005C7 ED               [12] 1307 	mov	a,r5
      0005C8 B5 04 02         [24] 1308 	cjne	a,ar4,00392$
      0005CB 80 03            [24] 1309 	sjmp	00393$
      0005CD                       1310 00392$:
      0005CD 02 07 D3         [24] 1311 	ljmp	00149$
      0005D0                       1312 00393$:
                                   1313 ;	cc2530_radio.c:247: if(cc==CMD_PING){
      0005D0 BF 01 21         [24] 1314 	cjne	r7,#0x01,00146$
                                   1315 ;	cc2530_radio.c:248: tmp[0]=FW_VER_HI; tmp[1]=FW_VER_LO; send_frame(RSP_PONG,tmp,2);
      0005D3 90 01 19         [24] 1316 	mov	dptr,#_main_tmp_10000_46
      0005D6 E4               [12] 1317 	clr	a
      0005D7 F0               [24] 1318 	movx	@dptr,a
      0005D8 90 01 1A         [24] 1319 	mov	dptr,#(_main_tmp_10000_46 + 0x0001)
      0005DB 74 04            [12] 1320 	mov	a,#0x04
      0005DD F0               [24] 1321 	movx	@dptr,a
      0005DE 75 10 19         [24] 1322 	mov	_send_frame_PARM_2,#_main_tmp_10000_46
      0005E1 75 11 01         [24] 1323 	mov	(_send_frame_PARM_2 + 1),#(_main_tmp_10000_46 >> 8)
      0005E4 75 12 02         [24] 1324 	mov	_send_frame_PARM_3,#0x02
      0005E7 75 82 81         [24] 1325 	mov	dpl, #0x81
      0005EA C0 06            [24] 1326 	push	ar6
      0005EC 12 03 49         [24] 1327 	lcall	_send_frame
      0005EF D0 06            [24] 1328 	pop	ar6
      0005F1 02 07 D3         [24] 1329 	ljmp	00149$
      0005F4                       1330 00146$:
                                   1331 ;	cc2530_radio.c:250: else if(cc==CMD_SET_CHANNEL && ln>=2){
      0005F4 BF 02 24         [24] 1332 	cjne	r7,#0x02,00142$
      0005F7 BE 02 00         [24] 1333 	cjne	r6,#0x02,00398$
      0005FA                       1334 00398$:
      0005FA 40 1F            [24] 1335 	jc	00142$
                                   1336 ;	cc2530_radio.c:251: radio_init(cmd[1]); send_frame(RSP_OK,tmp,0);
      0005FC 90 00 8E         [24] 1337 	mov	dptr,#(_main_cmd_10000_46 + 0x0001)
      0005FF E0               [24] 1338 	movx	a,@dptr
      000600 F5 82            [12] 1339 	mov	dpl,a
      000602 C0 06            [24] 1340 	push	ar6
      000604 12 01 B8         [24] 1341 	lcall	_radio_init
      000607 75 10 19         [24] 1342 	mov	_send_frame_PARM_2,#_main_tmp_10000_46
      00060A 75 11 01         [24] 1343 	mov	(_send_frame_PARM_2 + 1),#(_main_tmp_10000_46 >> 8)
      00060D 75 12 00         [24] 1344 	mov	_send_frame_PARM_3,#0x00
      000610 75 82 82         [24] 1345 	mov	dpl, #0x82
      000613 12 03 49         [24] 1346 	lcall	_send_frame
      000616 D0 06            [24] 1347 	pop	ar6
      000618 02 07 D3         [24] 1348 	ljmp	00149$
      00061B                       1349 00142$:
                                   1350 ;	cc2530_radio.c:253: else if(cc==CMD_TX){
      00061B BF 03 3D         [24] 1351 	cjne	r7,#0x03,00139$
                                   1352 ;	cc2530_radio.c:254: unsigned char attempts=0;
      00061E 75 15 00         [24] 1353 	mov	_main_attempts_80000_63,#0x00
                                   1354 ;	cc2530_radio.c:255: unsigned char r=radio_tx(&cmd[1],(unsigned char)(ln-1),tx_retries,&attempts);
      000621 8E 05            [24] 1355 	mov	ar5,r6
      000623 1D               [12] 1356 	dec	r5
      000624 8D 0A            [24] 1357 	mov	_radio_tx_PARM_2,r5
      000626 75 0C 15         [24] 1358 	mov	_radio_tx_PARM_4,#_main_attempts_80000_63
      000629 75 0D 00         [24] 1359 	mov	(_radio_tx_PARM_4 + 1),#0x00
      00062C 75 0E 40         [24] 1360 	mov	(_radio_tx_PARM_4 + 2),#0x40
      00062F 85 09 0B         [24] 1361 	mov	_radio_tx_PARM_3,_tx_retries
      000632 90 00 8E         [24] 1362 	mov	dptr,#(_main_cmd_10000_46 + 0x0001)
      000635 C0 06            [24] 1363 	push	ar6
      000637 12 02 81         [24] 1364 	lcall	_radio_tx
      00063A AD 82            [24] 1365 	mov	r5, dpl
                                   1366 ;	cc2530_radio.c:256: tmp[0]=r; tmp[1]=attempts; send_frame(RSP_TXSTAT,tmp,2);
      00063C 90 01 19         [24] 1367 	mov	dptr,#_main_tmp_10000_46
      00063F ED               [12] 1368 	mov	a,r5
      000640 F0               [24] 1369 	movx	@dptr,a
      000641 90 01 1A         [24] 1370 	mov	dptr,#(_main_tmp_10000_46 + 0x0001)
      000644 E5 15            [12] 1371 	mov	a,_main_attempts_80000_63
      000646 F0               [24] 1372 	movx	@dptr,a
      000647 75 10 19         [24] 1373 	mov	_send_frame_PARM_2,#_main_tmp_10000_46
      00064A 75 11 01         [24] 1374 	mov	(_send_frame_PARM_2 + 1),#(_main_tmp_10000_46 >> 8)
      00064D 75 12 02         [24] 1375 	mov	_send_frame_PARM_3,#0x02
      000650 75 82 83         [24] 1376 	mov	dpl, #0x83
      000653 12 03 49         [24] 1377 	lcall	_send_frame
      000656 D0 06            [24] 1378 	pop	ar6
      000658 02 07 D3         [24] 1379 	ljmp	00149$
      00065B                       1380 00139$:
                                   1381 ;	cc2530_radio.c:258: else if(cc==CMD_SET_PROMISC && ln>=2){
      00065B BF 04 2C         [24] 1382 	cjne	r7,#0x04,00135$
      00065E BE 02 00         [24] 1383 	cjne	r6,#0x02,00404$
      000661                       1384 00404$:
      000661 40 27            [24] 1385 	jc	00135$
                                   1386 ;	cc2530_radio.c:259: if(cmd[1]) mac_flags |= MAC_FLAG_FILTER;
      000663 90 00 8E         [24] 1387 	mov	dptr,#(_main_cmd_10000_46 + 0x0001)
      000666 E0               [24] 1388 	movx	a,@dptr
      000667 60 05            [24] 1389 	jz	00114$
      000669 43 08 01         [24] 1390 	orl	_mac_flags,#0x01
      00066C 80 03            [24] 1391 	sjmp	00115$
      00066E                       1392 00114$:
                                   1393 ;	cc2530_radio.c:260: else mac_flags &= (unsigned char)~MAC_FLAG_FILTER;
      00066E 53 08 FE         [24] 1394 	anl	_mac_flags,#0xfe
      000671                       1395 00115$:
                                   1396 ;	cc2530_radio.c:261: apply_mac(); send_frame(RSP_OK,tmp,0);
      000671 C0 06            [24] 1397 	push	ar6
      000673 12 00 F9         [24] 1398 	lcall	_apply_mac
      000676 75 10 19         [24] 1399 	mov	_send_frame_PARM_2,#_main_tmp_10000_46
      000679 75 11 01         [24] 1400 	mov	(_send_frame_PARM_2 + 1),#(_main_tmp_10000_46 >> 8)
      00067C 75 12 00         [24] 1401 	mov	_send_frame_PARM_3,#0x00
      00067F 75 82 82         [24] 1402 	mov	dpl, #0x82
      000682 12 03 49         [24] 1403 	lcall	_send_frame
      000685 D0 06            [24] 1404 	pop	ar6
      000687 02 07 D3         [24] 1405 	ljmp	00149$
      00068A                       1406 00135$:
                                   1407 ;	cc2530_radio.c:263: else if(cc==CMD_SET_ADDR && ln>=13){
      00068A BF 05 21         [24] 1408 	cjne	r7,#0x05,00131$
      00068D BE 0D 00         [24] 1409 	cjne	r6,#0x0d,00409$
      000690                       1410 00409$:
      000690 40 1C            [24] 1411 	jc	00131$
                                   1412 ;	cc2530_radio.c:264: set_address(&cmd[1]); send_frame(RSP_OK,tmp,0);
      000692 90 00 8E         [24] 1413 	mov	dptr,#(_main_cmd_10000_46 + 0x0001)
      000695 C0 06            [24] 1414 	push	ar6
      000697 12 01 1E         [24] 1415 	lcall	_set_address
      00069A 75 10 19         [24] 1416 	mov	_send_frame_PARM_2,#_main_tmp_10000_46
      00069D 75 11 01         [24] 1417 	mov	(_send_frame_PARM_2 + 1),#(_main_tmp_10000_46 >> 8)
      0006A0 75 12 00         [24] 1418 	mov	_send_frame_PARM_3,#0x00
      0006A3 75 82 82         [24] 1419 	mov	dpl, #0x82
      0006A6 12 03 49         [24] 1420 	lcall	_send_frame
      0006A9 D0 06            [24] 1421 	pop	ar6
      0006AB 02 07 D3         [24] 1422 	ljmp	00149$
      0006AE                       1423 00131$:
                                   1424 ;	cc2530_radio.c:266: else if(cc==CMD_SET_MAC && ln>=3){
      0006AE BF 06 2E         [24] 1425 	cjne	r7,#0x06,00127$
      0006B1 BE 03 00         [24] 1426 	cjne	r6,#0x03,00413$
      0006B4                       1427 00413$:
      0006B4 40 29            [24] 1428 	jc	00127$
                                   1429 ;	cc2530_radio.c:267: mac_flags=(unsigned char)(cmd[1] & (MAC_FLAG_FILTER|MAC_FLAG_AUTOACK|MAC_FLAG_CCA_TX));
      0006B6 90 00 8E         [24] 1430 	mov	dptr,#(_main_cmd_10000_46 + 0x0001)
      0006B9 E0               [24] 1431 	movx	a,@dptr
      0006BA FD               [12] 1432 	mov	r5,a
      0006BB 74 07            [12] 1433 	mov	a,#0x07
      0006BD 5D               [12] 1434 	anl	a,r5
      0006BE F5 08            [12] 1435 	mov	_mac_flags,a
                                   1436 ;	cc2530_radio.c:268: tx_retries=cmd[2];
      0006C0 90 00 8F         [24] 1437 	mov	dptr,#(_main_cmd_10000_46 + 0x0002)
      0006C3 E0               [24] 1438 	movx	a,@dptr
      0006C4 F5 09            [12] 1439 	mov	_tx_retries,a
                                   1440 ;	cc2530_radio.c:269: apply_mac(); send_frame(RSP_OK,tmp,0);
      0006C6 C0 06            [24] 1441 	push	ar6
      0006C8 12 00 F9         [24] 1442 	lcall	_apply_mac
      0006CB 75 10 19         [24] 1443 	mov	_send_frame_PARM_2,#_main_tmp_10000_46
      0006CE 75 11 01         [24] 1444 	mov	(_send_frame_PARM_2 + 1),#(_main_tmp_10000_46 >> 8)
      0006D1 75 12 00         [24] 1445 	mov	_send_frame_PARM_3,#0x00
      0006D4 75 82 82         [24] 1446 	mov	dpl, #0x82
      0006D7 12 03 49         [24] 1447 	lcall	_send_frame
      0006DA D0 06            [24] 1448 	pop	ar6
      0006DC 02 07 D3         [24] 1449 	ljmp	00149$
      0006DF                       1450 00127$:
                                   1451 ;	cc2530_radio.c:271: else if(cc==CMD_GET_MAC){
      0006DF BF 07 02         [24] 1452 	cjne	r7,#0x07,00415$
      0006E2 80 03            [24] 1453 	sjmp	00416$
      0006E4                       1454 00415$:
      0006E4 02 07 68         [24] 1455 	ljmp	00124$
      0006E7                       1456 00416$:
                                   1457 ;	cc2530_radio.c:272: tmp[0]=mac_flags; tmp[1]=tx_retries;
      0006E7 90 01 19         [24] 1458 	mov	dptr,#_main_tmp_10000_46
      0006EA E5 08            [12] 1459 	mov	a,_mac_flags
      0006EC F0               [24] 1460 	movx	@dptr,a
      0006ED 90 01 1A         [24] 1461 	mov	dptr,#(_main_tmp_10000_46 + 0x0001)
      0006F0 E5 09            [12] 1462 	mov	a,_tx_retries
      0006F2 F0               [24] 1463 	movx	@dptr,a
                                   1464 ;	cc2530_radio.c:273: tmp[2]=PAN_ID0; tmp[3]=PAN_ID1;
      0006F3 90 61 72         [24] 1465 	mov	dptr,#_PAN_ID0
      0006F6 E0               [24] 1466 	movx	a,@dptr
      0006F7 90 01 1B         [24] 1467 	mov	dptr,#(_main_tmp_10000_46 + 0x0002)
      0006FA F0               [24] 1468 	movx	@dptr,a
      0006FB 90 61 73         [24] 1469 	mov	dptr,#_PAN_ID1
      0006FE E0               [24] 1470 	movx	a,@dptr
      0006FF 90 01 1C         [24] 1471 	mov	dptr,#(_main_tmp_10000_46 + 0x0003)
      000702 F0               [24] 1472 	movx	@dptr,a
                                   1473 ;	cc2530_radio.c:274: tmp[4]=SHORT_ADDR0; tmp[5]=SHORT_ADDR1;
      000703 90 61 74         [24] 1474 	mov	dptr,#_SHORT_ADDR0
      000706 E0               [24] 1475 	movx	a,@dptr
      000707 90 01 1D         [24] 1476 	mov	dptr,#(_main_tmp_10000_46 + 0x0004)
      00070A F0               [24] 1477 	movx	@dptr,a
      00070B 90 61 75         [24] 1478 	mov	dptr,#_SHORT_ADDR1
      00070E E0               [24] 1479 	movx	a,@dptr
      00070F 90 01 1E         [24] 1480 	mov	dptr,#(_main_tmp_10000_46 + 0x0005)
      000712 F0               [24] 1481 	movx	@dptr,a
                                   1482 ;	cc2530_radio.c:275: tmp[6]=EXT_ADDR0; tmp[7]=EXT_ADDR1; tmp[8]=EXT_ADDR2; tmp[9]=EXT_ADDR3;
      000713 90 61 6A         [24] 1483 	mov	dptr,#_EXT_ADDR0
      000716 E0               [24] 1484 	movx	a,@dptr
      000717 90 01 1F         [24] 1485 	mov	dptr,#(_main_tmp_10000_46 + 0x0006)
      00071A F0               [24] 1486 	movx	@dptr,a
      00071B 90 61 6B         [24] 1487 	mov	dptr,#_EXT_ADDR1
      00071E E0               [24] 1488 	movx	a,@dptr
      00071F 90 01 20         [24] 1489 	mov	dptr,#(_main_tmp_10000_46 + 0x0007)
      000722 F0               [24] 1490 	movx	@dptr,a
      000723 90 61 6C         [24] 1491 	mov	dptr,#_EXT_ADDR2
      000726 E0               [24] 1492 	movx	a,@dptr
      000727 90 01 21         [24] 1493 	mov	dptr,#(_main_tmp_10000_46 + 0x0008)
      00072A F0               [24] 1494 	movx	@dptr,a
      00072B 90 61 6D         [24] 1495 	mov	dptr,#_EXT_ADDR3
      00072E E0               [24] 1496 	movx	a,@dptr
      00072F 90 01 22         [24] 1497 	mov	dptr,#(_main_tmp_10000_46 + 0x0009)
      000732 F0               [24] 1498 	movx	@dptr,a
                                   1499 ;	cc2530_radio.c:276: tmp[10]=EXT_ADDR4; tmp[11]=EXT_ADDR5; tmp[12]=EXT_ADDR6; tmp[13]=EXT_ADDR7;
      000733 90 61 6E         [24] 1500 	mov	dptr,#_EXT_ADDR4
      000736 E0               [24] 1501 	movx	a,@dptr
      000737 90 01 23         [24] 1502 	mov	dptr,#(_main_tmp_10000_46 + 0x000a)
      00073A F0               [24] 1503 	movx	@dptr,a
      00073B 90 61 6F         [24] 1504 	mov	dptr,#_EXT_ADDR5
      00073E E0               [24] 1505 	movx	a,@dptr
      00073F 90 01 24         [24] 1506 	mov	dptr,#(_main_tmp_10000_46 + 0x000b)
      000742 F0               [24] 1507 	movx	@dptr,a
      000743 90 61 70         [24] 1508 	mov	dptr,#_EXT_ADDR6
      000746 E0               [24] 1509 	movx	a,@dptr
      000747 90 01 25         [24] 1510 	mov	dptr,#(_main_tmp_10000_46 + 0x000c)
      00074A F0               [24] 1511 	movx	@dptr,a
      00074B 90 61 71         [24] 1512 	mov	dptr,#_EXT_ADDR7
      00074E E0               [24] 1513 	movx	a,@dptr
      00074F 90 01 26         [24] 1514 	mov	dptr,#(_main_tmp_10000_46 + 0x000d)
      000752 F0               [24] 1515 	movx	@dptr,a
                                   1516 ;	cc2530_radio.c:277: send_frame(RSP_MAC_INFO,tmp,14);
      000753 75 10 19         [24] 1517 	mov	_send_frame_PARM_2,#_main_tmp_10000_46
      000756 75 11 01         [24] 1518 	mov	(_send_frame_PARM_2 + 1),#(_main_tmp_10000_46 >> 8)
      000759 75 12 0E         [24] 1519 	mov	_send_frame_PARM_3,#0x0e
      00075C 75 82 85         [24] 1520 	mov	dpl, #0x85
      00075F C0 06            [24] 1521 	push	ar6
      000761 12 03 49         [24] 1522 	lcall	_send_frame
      000764 D0 06            [24] 1523 	pop	ar6
      000766 80 6B            [24] 1524 	sjmp	00149$
      000768                       1525 00124$:
                                   1526 ;	cc2530_radio.c:279: else if(cc==CMD_TX_ADV && ln>=2){
      000768 BF 08 45         [24] 1527 	cjne	r7,#0x08,00120$
      00076B BE 02 00         [24] 1528 	cjne	r6,#0x02,00419$
      00076E                       1529 00419$:
      00076E 40 40            [24] 1530 	jc	00120$
                                   1531 ;	cc2530_radio.c:280: unsigned char attempts=0;
      000770 75 16 00         [24] 1532 	mov	_main_attempts_80000_68,#0x00
                                   1533 ;	cc2530_radio.c:281: unsigned char r=radio_tx(&cmd[2],(unsigned char)(ln-2),cmd[1],&attempts);
      000773 8E 05            [24] 1534 	mov	ar5,r6
      000775 1D               [12] 1535 	dec	r5
      000776 1D               [12] 1536 	dec	r5
      000777 8D 0A            [24] 1537 	mov	_radio_tx_PARM_2,r5
      000779 90 00 8E         [24] 1538 	mov	dptr,#(_main_cmd_10000_46 + 0x0001)
      00077C E0               [24] 1539 	movx	a,@dptr
      00077D F5 0B            [12] 1540 	mov	_radio_tx_PARM_3,a
      00077F 75 0C 16         [24] 1541 	mov	_radio_tx_PARM_4,#_main_attempts_80000_68
      000782 75 0D 00         [24] 1542 	mov	(_radio_tx_PARM_4 + 1),#0x00
      000785 75 0E 40         [24] 1543 	mov	(_radio_tx_PARM_4 + 2),#0x40
      000788 90 00 8F         [24] 1544 	mov	dptr,#(_main_cmd_10000_46 + 0x0002)
      00078B C0 06            [24] 1545 	push	ar6
      00078D 12 02 81         [24] 1546 	lcall	_radio_tx
      000790 AD 82            [24] 1547 	mov	r5, dpl
                                   1548 ;	cc2530_radio.c:282: tmp[0]=r; tmp[1]=attempts; send_frame(RSP_TXSTAT,tmp,2);
      000792 90 01 19         [24] 1549 	mov	dptr,#_main_tmp_10000_46
      000795 ED               [12] 1550 	mov	a,r5
      000796 F0               [24] 1551 	movx	@dptr,a
      000797 90 01 1A         [24] 1552 	mov	dptr,#(_main_tmp_10000_46 + 0x0001)
      00079A E5 16            [12] 1553 	mov	a,_main_attempts_80000_68
      00079C F0               [24] 1554 	movx	@dptr,a
      00079D 75 10 19         [24] 1555 	mov	_send_frame_PARM_2,#_main_tmp_10000_46
      0007A0 75 11 01         [24] 1556 	mov	(_send_frame_PARM_2 + 1),#(_main_tmp_10000_46 >> 8)
      0007A3 75 12 02         [24] 1557 	mov	_send_frame_PARM_3,#0x02
      0007A6 75 82 83         [24] 1558 	mov	dpl, #0x83
      0007A9 12 03 49         [24] 1559 	lcall	_send_frame
      0007AC D0 06            [24] 1560 	pop	ar6
      0007AE 80 23            [24] 1561 	sjmp	00149$
      0007B0                       1562 00120$:
                                   1563 ;	cc2530_radio.c:284: else if(cc==CMD_SET_TX_POWER && ln>=2){
      0007B0 BF 09 20         [24] 1564 	cjne	r7,#0x09,00149$
      0007B3 BE 02 00         [24] 1565 	cjne	r6,#0x02,00423$
      0007B6                       1566 00423$:
      0007B6 40 1B            [24] 1567 	jc	00149$
                                   1568 ;	cc2530_radio.c:285: TXPOWER=cmd[1]; send_frame(RSP_OK,tmp,0);
      0007B8 90 00 8E         [24] 1569 	mov	dptr,#(_main_cmd_10000_46 + 0x0001)
      0007BB E0               [24] 1570 	movx	a,@dptr
      0007BC 90 61 90         [24] 1571 	mov	dptr,#_TXPOWER
      0007BF F0               [24] 1572 	movx	@dptr,a
      0007C0 75 10 19         [24] 1573 	mov	_send_frame_PARM_2,#_main_tmp_10000_46
      0007C3 75 11 01         [24] 1574 	mov	(_send_frame_PARM_2 + 1),#(_main_tmp_10000_46 >> 8)
      0007C6 75 12 00         [24] 1575 	mov	_send_frame_PARM_3,#0x00
      0007C9 75 82 82         [24] 1576 	mov	dpl, #0x82
      0007CC C0 06            [24] 1577 	push	ar6
      0007CE 12 03 49         [24] 1578 	lcall	_send_frame
      0007D1 D0 06            [24] 1579 	pop	ar6
      0007D3                       1580 00149$:
                                   1581 ;	cc2530_radio.c:288: st=0;
      0007D3 75 13 00         [24] 1582 	mov	_main_st_10000_46,#0x00
                                   1583 ;	cc2530_radio.c:291: }
                                   1584 ;	cc2530_radio.c:294: }
      0007D6 02 04 14         [24] 1585 	ljmp	00162$
                                   1586 	.area CSEG    (CODE)
                                   1587 	.area CONST   (CODE)
                                   1588 	.area XINIT   (CODE)
                                   1589 	.area CABS    (ABS,CODE)
