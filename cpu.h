#ifndef __CPU_H__
#define __CPU_H__

#include <stdint.h>

typedef unsigned char word_1;
typedef unsigned char word_4;
typedef unsigned char word_8;
typedef unsigned short word_12;
typedef unsigned short word_16;
typedef long word_20;
typedef long word_32;

#define P_FIELD 0
#define WP_FIELD 1
#define XS_FIELD 2
#define X_FIELD 3
#define S_FIELD 4
#define M_FIELD 5
#define B_FIELD 6
#define W_FIELD 7
#define A_FIELD 15
#define IN_FIELD 16
#define OUT_FIELD 17
#define OUTS_FIELD 18

#define DEC 10
#define HEX 16

#define NR_RSTK 8
#define NR_PSTAT 16

#define NR_MCTL 6

typedef struct keystate_t {
  short rows[9];
} keystate_t;

typedef struct mem_cntl_t {
  unsigned short unconfigured;
  word_20 config[2];
} mem_cntl_t;

typedef struct saturn {

  unsigned long magic;
  char version[4];

  unsigned char A[16], B[16], C[16], D[16];

  word_20 d[2];

#define D0 d[0]
#define D1 d[1]

  word_4 P;
  word_20 PC;

  unsigned char R0[16], R1[16], R2[16], R3[16], R4[16];
  unsigned char IN[4];
  unsigned char OUT[3];

  word_1 CARRY;

  unsigned char PSTAT[NR_PSTAT];
  unsigned char XM, SB, SR, MP;

  word_4 hexmode;

  word_20 rstk[NR_RSTK];
  short rstkp;

  keystate_t keybuf;

  unsigned char intenable;
  unsigned char int_pending;
  unsigned char kbd_ien;

  word_4 disp_io;

  word_4 contrast_ctrl;
  word_8 disp_test;

  word_16 crc;

  word_4 power_status;
  word_4 power_ctrl;

  word_4 mode;

  word_8 annunc;

  word_4 baud;

  word_4 card_ctrl;
  word_4 card_status;

  word_4 io_ctrl;
  word_4 rcs;
  word_4 tcs;

  word_8 rbr;
  word_8 tbr;

  word_8 sreq;

  word_4 ir_ctrl;

  word_4 base_off;

  word_4 lcr;
  word_4 lbr;

  word_4 scratch;

  word_4 base_nibble;

  word_20 disp_addr;
  word_12 line_offset;
  word_8 line_count;

  word_16 unknown;

  word_4 t1_ctrl;
  word_4 t2_ctrl;

  word_20 menu_addr;

  word_8 unknown2;

  char timer1; /* may NOT be unsigned !!! */
  word_32 timer2;

  long t1_instr;
  long t2_instr;

  short t1_tick;
  short t2_tick;
  long i_per_s;

  short bank_switch;
  mem_cntl_t mem_cntl[NR_MCTL];

  unsigned char *rom;
  unsigned char *ram;
  unsigned char *port1;
  unsigned char *port2;

} saturn_t;

extern saturn_t saturn;

extern uint8_t hp48_ram[];

#endif // __CPU_H__
