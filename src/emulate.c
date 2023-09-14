/*
 *  This file is part of x48, an emulator of the HP-48sx Calculator.
 *  Copyright (C) 1994  Eddie C. Dost  (ecd@dressler.de)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * $Log: emulate.c,v $
 * Revision 1.16  1995/01/11  18:20:01  ecd
 * major update to support HP48 G/GX
 *
 * Revision 1.15  1994/12/07  20:20:50  ecd
 * fiddled around with sound
 *
 * Revision 1.15  1994/12/07  20:20:50  ecd
 * fiddled around with sound
 *
 * Revision 1.14  1994/11/28  02:00:51  ecd
 * added TRAP instruction 64001
 * played with output register ...
 * removed some unused switch statements
 *
 * Revision 1.13  1994/11/04  03:42:34  ecd
 * changed includes
 *
 * Revision 1.12  1994/11/02  19:13:04  ecd
 * fixed missing log
 *
 *
 * $Id: emulate.c,v 1.16 1995/01/11 18:20:01 ecd Exp ecd $
 */

#include "global.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "debugger.h"
#include "device.h"
#include "hp48.h"
#include "hp48_emu.h"
#include "timer.h"
#include "x48_x11.h"

extern int throttle;

#define DEBUG_TIMER
#define DEBUG_SCHED 1
#define DEBUG_DISP_SCHED

static long jumpaddr;

unsigned long instructions = 0;
unsigned long old_instr = 0;

int rece_instr = 0;
int device_check = 0;

int adj_time_pending = 0;

int set_t1;

long schedule_event = 0;

#define SrvcIoStart 0x3c0
#define SrvcIoEnd 0x5ec

#define SCHED_INSTR_ROLLOVER 0x3fffffff
#define SCHED_RECEIVE 0x7ff
#define SCHED_ADJTIME 0x1ffe
#define SCHED_TIMER1 0x1e00
#define SCHED_TIMER2 0xf
#define SCHED_STATISTICS 0x7ffff
#define SCHED_NEVER 0x7fffffff

#define NR_SAMPLES 10

long sched_instr_rollover = SCHED_INSTR_ROLLOVER;
long sched_receive = SCHED_RECEIVE;
long sched_adjtime = SCHED_ADJTIME;
long sched_timer1 = SCHED_TIMER1;
long sched_timer2 = SCHED_TIMER2;
long sched_statistics = SCHED_STATISTICS;
long sched_display = SCHED_NEVER;

unsigned long t1_i_per_tick;
unsigned long t2_i_per_tick;
unsigned long s_1;
unsigned long s_16;
unsigned long old_s_1;
unsigned long old_s_16;
unsigned long delta_t_1;
unsigned long delta_t_16;
unsigned long delta_i;
word_64 run;

static word_20 jumpmasks[] = {0xffffffff, 0xfffffff0, 0xffffff00, 0xfffff000,
                              0xffff0000, 0xfff00000, 0xff000000, 0xf0000000};

char current_instr[32] = { 0 };
void trace_instr(char instr[]) {
  strcpy(current_instr, instr);
}

char nibble_to_hex(char nibble) {
  assert(nibble < 0x10);
  if (nibble < 0xA) return '0' + nibble;
  return 'A' + nibble - 0xA;
}

char current_opcode[32] = { 0 };
void trace_rom_read(char nibble) {
  int current_opcode_idx = strlen(current_opcode);
  current_opcode[current_opcode_idx] = nibble_to_hex(nibble);
}

int decode_group_80(void) {
  int t, op3, op4, op5, op6;
  unsigned char *REG;
  long addr;
  op3 = read_nibble(saturn.PC + 2);
  switch (op3) {
  case 0: trace_instr("OUT=CS"); /* OUT=CS */
    saturn.PC += 3;
    copy_register(saturn.OUT, saturn.C, OUTS_FIELD);
#if 0
      check_out_register();
#endif
    return 0;
  case 1: trace_instr("OUT=C"); /* OUT=C */
    saturn.PC += 3;
    copy_register(saturn.OUT, saturn.C, OUT_FIELD);
#if 0
      check_out_register();
#endif
    return 0;
  case 2: trace_instr("A=IN"); /* A=IN */
    saturn.PC += 3;
    do_in();
    copy_register(saturn.A, saturn.IN, IN_FIELD);
    return 0;
  case 3: trace_instr("C=IN"); /* C=IN */
    saturn.PC += 3;
    do_in();
    copy_register(saturn.C, saturn.IN, IN_FIELD);
    return 0;
  case 4: trace_instr("UNCNFG"); /* UNCNFG */
    saturn.PC += 3;
    do_unconfigure();
    return 0;
  case 5: trace_instr("CONFIG"); /* CONFIG */
    saturn.PC += 3;
    do_configure();
    return 0;
  case 6: trace_instr("C=ID"); /* C=ID */
    saturn.PC += 3;
    return get_identification();
  case 7: trace_instr("SHUTDN"); /* SHUTDN */
    saturn.PC += 3;
    saturn.shutdown = 1;
    return 0;
  case 8:
    op4 = read_nibble(saturn.PC + 3);
    switch (op4) {
    case 0: trace_instr("INTON"); /* INTON */
      saturn.PC += 4;
      do_inton();
      return 0;
    case 1: trace_instr("RSI..."); /* RSI... */
      op5 = read_nibble(saturn.PC + 4);
      saturn.PC += 5;
      do_reset_interrupt_system();
      return 0;
    case 2: trace_instr("LA..."); /* LA... */
      op5 = read_nibble(saturn.PC + 4);
      load_constant(saturn.A, op5 + 1, saturn.PC + 5);
      saturn.PC += 6 + op5;
      return 0;
    case 3: trace_instr("BUSCB"); /* BUSCB */
      saturn.PC += 4;
      return 0;
    case 4: trace_instr("ABIT=0"); /* ABIT=0 */
      op5 = read_nibble(saturn.PC + 4);
      saturn.PC += 5;
      clear_register_bit(saturn.A, op5);
      return 0;
    case 5: trace_instr("ABIT=1"); /* ABIT=1 */
      op5 = read_nibble(saturn.PC + 4);
      saturn.PC += 5;
      set_register_bit(saturn.A, op5);
      return 0;
    case 8: trace_instr("CBIT=0"); /* CBIT=0 */
      op5 = read_nibble(saturn.PC + 4);
      saturn.PC += 5;
      clear_register_bit(saturn.C, op5);
      return 0;
    case 9: trace_instr("CBIT=1"); /* CBIT=1 */
      op5 = read_nibble(saturn.PC + 4);
      saturn.PC += 5;
      set_register_bit(saturn.C, op5);
      return 0;
    case 6: trace_instr("?ABIT=0"); /* ?ABIT=0 */
    case 7: trace_instr("?ABIT=1"); /* ?ABIT=1 */
    case 0xa: trace_instr("?CBIT=0"); /* ?CBIT=0 */
    case 0xb: trace_instr("?CBIT=1"); /* ?CBIT=1 */
      op5 = read_nibble(saturn.PC + 4);
      if (op4 < 8)
        REG = saturn.A;
      else
        REG = saturn.C;
      if (op4 == 6 || op4 == 0xa)
        t = 0;
      else
        t = 1;
      saturn.CARRY = (get_register_bit(REG, op5) == t) ? 1 : 0;
      if (saturn.CARRY) {
        saturn.PC += 5;
        op6 = read_nibbles(saturn.PC, 2);
        if (op6) {
          if (op6 & 0x80)
            op6 |= jumpmasks[2];
          jumpaddr = (saturn.PC + op6) & 0xfffff;
          saturn.PC = jumpaddr;
        } else {
          saturn.PC = pop_return_addr();
        }
      } else {
        saturn.PC += 7;
      }
      return 0;
    case 0xc: trace_instr("PC=(A)"); /* PC=(A) */
      addr = dat_to_addr(saturn.A);
      jumpaddr = read_nibbles(addr, 5);
      saturn.PC = jumpaddr;
      return 0;
    case 0xd: trace_instr("BUSCD"); /* BUSCD */
      saturn.PC += 4;
      return 0;
    case 0xe: trace_instr("PC=(C)"); /* PC=(C) */
      addr = dat_to_addr(saturn.C);
      jumpaddr = read_nibbles(addr, 5);
      saturn.PC = jumpaddr;
      return 0;
    case 0xf: trace_instr("INTOFF"); /* INTOFF */
      saturn.PC += 4;
      do_intoff();
      return 0;
    default:
      return 1;
    }
  case 9: trace_instr("C+P+1"); /* C+P+1 */
    saturn.PC += 3;
    add_p_plus_one(saturn.C);
    return 0;
  case 0xa: trace_instr("RESET"); /* RESET */
    saturn.PC += 3;
    do_reset();
    return 0;
  case 0xb: trace_instr("BUSCC"); /* BUSCC */
    saturn.PC += 3;
    return 0;
  case 0xc: trace_instr("C=P n"); /* C=P n */
    op4 = read_nibble(saturn.PC + 3);
    saturn.PC += 4;
    set_register_nibble(saturn.C, op4, saturn.P);
    return 0;
  case 0xd: trace_instr("P=C n"); /* P=C n */
    op4 = read_nibble(saturn.PC + 3);
    saturn.PC += 4;
    saturn.P = get_register_nibble(saturn.C, op4);
    return 0;
  case 0xe: trace_instr("SREQ?"); /* SREQ? */
    saturn.PC += 3;
    saturn.C[0] = 0;
    saturn.SR = 0;
    return 0;
  case 0xf: trace_instr("CPEX n"); /* CPEX n */
    op4 = read_nibble(saturn.PC + 3);
    saturn.PC += 4;
    t = get_register_nibble(saturn.C, op4);
    set_register_nibble(saturn.C, op4, saturn.P);
    saturn.P = t;
    return 0;
  default:
    return 1;
  }
}

int decode_group_1(void) {
  int op, op2, op3, op4;

  op2 = read_nibble(saturn.PC + 1);
  switch (op2) {
  case 0:
    op3 = read_nibble(saturn.PC + 2);
    switch (op3) {
    case 0: trace_instr("saturn.R0=A"); /* saturn.R0=A */
      saturn.PC += 3;
      copy_register(saturn.R0, saturn.A, W_FIELD);
      return 0;
    case 1: trace_instr("saturn.R1=A"); /* saturn.R1=A */
    case 5:
      saturn.PC += 3;
      copy_register(saturn.R1, saturn.A, W_FIELD);
      return 0;
    case 2: trace_instr("saturn.R2=A"); /* saturn.R2=A */
    case 6:
      saturn.PC += 3;
      copy_register(saturn.R2, saturn.A, W_FIELD);
      return 0;
    case 3: trace_instr("saturn.R3=A"); /* saturn.R3=A */
    case 7:
      saturn.PC += 3;
      copy_register(saturn.R3, saturn.A, W_FIELD);
      return 0;
    case 4: trace_instr("saturn.R4=A"); /* saturn.R4=A */
      saturn.PC += 3;
      copy_register(saturn.R4, saturn.A, W_FIELD);
      return 0;
    case 8: trace_instr("saturn.R0=C"); /* saturn.R0=C */
      saturn.PC += 3;
      copy_register(saturn.R0, saturn.C, W_FIELD);
      return 0;
    case 9: trace_instr("saturn.R1=C"); /* saturn.R1=C */
    case 0xd:
      saturn.PC += 3;
      copy_register(saturn.R1, saturn.C, W_FIELD);
      return 0;
    case 0xa: trace_instr("saturn.R2=C"); /* saturn.R2=C */
    case 0xe:
      saturn.PC += 3;
      copy_register(saturn.R2, saturn.C, W_FIELD);
      return 0;
    case 0xb: trace_instr("saturn.R3=C"); /* saturn.R3=C */
    case 0xf:
      saturn.PC += 3;
      copy_register(saturn.R3, saturn.C, W_FIELD);
      return 0;
    case 0xc: trace_instr("saturn.R4=C"); /* saturn.R4=C */
      saturn.PC += 3;
      copy_register(saturn.R4, saturn.C, W_FIELD);
      return 0;
    default:
      return 1;
    }
  case 1:
    op3 = read_nibble(saturn.PC + 2);
    switch (op3) {
    case 0: trace_instr("A=R0"); /* A=R0 */
      saturn.PC += 3;
      copy_register(saturn.A, saturn.R0, W_FIELD);
      return 0;
    case 1: trace_instr("A=R1"); /* A=R1 */
    case 5:
      saturn.PC += 3;
      copy_register(saturn.A, saturn.R1, W_FIELD);
      return 0;
    case 2: trace_instr("A=R2"); /* A=R2 */
    case 6:
      saturn.PC += 3;
      copy_register(saturn.A, saturn.R2, W_FIELD);
      return 0;
    case 3: trace_instr("A=R3"); /* A=R3 */
    case 7:
      saturn.PC += 3;
      copy_register(saturn.A, saturn.R3, W_FIELD);
      return 0;
    case 4: trace_instr("A=R4"); /* A=R4 */
      saturn.PC += 3;
      copy_register(saturn.A, saturn.R4, W_FIELD);
      return 0;
    case 8: trace_instr("C=R0"); /* C=R0 */
      saturn.PC += 3;
      copy_register(saturn.C, saturn.R0, W_FIELD);
      return 0;
    case 9: trace_instr("C=R1"); /* C=R1 */
    case 0xd:
      saturn.PC += 3;
      copy_register(saturn.C, saturn.R1, W_FIELD);
      return 0;
    case 0xa: trace_instr("C=R2"); /* C=R2 */
    case 0xe:
      saturn.PC += 3;
      copy_register(saturn.C, saturn.R2, W_FIELD);
      return 0;
    case 0xb: trace_instr("C=R3"); /* C=R3 */
    case 0xf:
      saturn.PC += 3;
      copy_register(saturn.C, saturn.R3, W_FIELD);
      return 0;
    case 0xc: trace_instr("C=R4"); /* C=R4 */
      saturn.PC += 3;
      copy_register(saturn.C, saturn.R4, W_FIELD);
      return 0;
    default:
      return 1;
    }
  case 2:
    op3 = read_nibble(saturn.PC + 2);
    switch (op3) {
    case 0: trace_instr("AR0EX"); /* AR0EX */
      saturn.PC += 3;
      exchange_register(saturn.A, saturn.R0, W_FIELD);
      return 0;
    case 1: trace_instr("AR1EX"); /* AR1EX */
    case 5:
      saturn.PC += 3;
      exchange_register(saturn.A, saturn.R1, W_FIELD);
      return 0;
    case 2: trace_instr("AR2EX"); /* AR2EX */
    case 6:
      saturn.PC += 3;
      exchange_register(saturn.A, saturn.R2, W_FIELD);
      return 0;
    case 3: trace_instr("AR3EX"); /* AR3EX */
    case 7:
      saturn.PC += 3;
      exchange_register(saturn.A, saturn.R3, W_FIELD);
      return 0;
    case 4: trace_instr("AR4EX"); /* AR4EX */
      saturn.PC += 3;
      exchange_register(saturn.A, saturn.R4, W_FIELD);
      return 0;
    case 8: trace_instr("CR0EX"); /* CR0EX */
      saturn.PC += 3;
      exchange_register(saturn.C, saturn.R0, W_FIELD);
      return 0;
    case 9: trace_instr("CR1EX"); /* CR1EX */
    case 0xd:
      saturn.PC += 3;
      exchange_register(saturn.C, saturn.R1, W_FIELD);
      return 0;
    case 0xa: trace_instr("CR2EX"); /* CR2EX */
    case 0xe:
      saturn.PC += 3;
      exchange_register(saturn.C, saturn.R2, W_FIELD);
      return 0;
    case 0xb: trace_instr("CR3EX"); /* CR3EX */
    case 0xf:
      saturn.PC += 3;
      exchange_register(saturn.C, saturn.R3, W_FIELD);
      return 0;
    case 0xc: trace_instr("CR4EX"); /* CR4EX */
      saturn.PC += 3;
      exchange_register(saturn.C, saturn.R4, W_FIELD);
      return 0;
    default:
      return 1;
    }
  case 3:
    op3 = read_nibble(saturn.PC + 2);
    switch (op3) {
    case 0: trace_instr("D0=A"); /* D0=A */
      saturn.PC += 3;
      register_to_address(saturn.A, &saturn.D0, 0);
      return 0;
    case 1: trace_instr("D1=A"); /* D1=A */
      saturn.PC += 3;
      register_to_address(saturn.A, &saturn.D1, 0);
      return 0;
    case 2: trace_instr("AD0EX"); /* AD0EX */
      saturn.PC += 3;
      exchange_reg(saturn.A, &saturn.D0, A_FIELD);
      return 0;
    case 3: trace_instr("AD1EX"); /* AD1EX */
      saturn.PC += 3;
      exchange_reg(saturn.A, &saturn.D1, A_FIELD);
      return 0;
    case 4: trace_instr("D0=C"); /* D0=C */
      saturn.PC += 3;
      register_to_address(saturn.C, &saturn.D0, 0);
      return 0;
    case 5: trace_instr("D1=C"); /* D1=C */
      saturn.PC += 3;
      register_to_address(saturn.C, &saturn.D1, 0);
      return 0;
    case 6: trace_instr("CD0EX"); /* CD0EX */
      saturn.PC += 3;
      exchange_reg(saturn.C, &saturn.D0, A_FIELD);
      return 0;
    case 7: trace_instr("CD1EX"); /* CD1EX */
      saturn.PC += 3;
      exchange_reg(saturn.C, &saturn.D1, A_FIELD);
      return 0;
    case 8: trace_instr("D0=AS"); /* D0=AS */
      saturn.PC += 3;
      register_to_address(saturn.A, &saturn.D0, 1);
      return 0;
    case 9: trace_instr("saturn.D1=AS"); /* saturn.D1=AS */
      saturn.PC += 3;
      register_to_address(saturn.A, &saturn.D1, 1);
      return 0;
    case 0xa: trace_instr("AD0XS"); /* AD0XS */
      saturn.PC += 3;
      exchange_reg(saturn.A, &saturn.D0, IN_FIELD);
      return 0;
    case 0xb: trace_instr("AD1XS"); /* AD1XS */
      saturn.PC += 3;
      exchange_reg(saturn.A, &saturn.D1, IN_FIELD);
      return 0;
    case 0xc: trace_instr("D0=CS"); /* D0=CS */
      saturn.PC += 3;
      register_to_address(saturn.C, &saturn.D0, 1);
      return 0;
    case 0xd: trace_instr("D1=CS"); /* D1=CS */
      saturn.PC += 3;
      register_to_address(saturn.C, &saturn.D1, 1);
      return 0;
    case 0xe: trace_instr("CD0XS"); /* CD0XS */
      saturn.PC += 3;
      exchange_reg(saturn.C, &saturn.D0, IN_FIELD);
      return 0;
    case 0xf: trace_instr("CD1XS"); /* CD1XS */
      saturn.PC += 3;
      exchange_reg(saturn.C, &saturn.D1, IN_FIELD);
      return 0;
    default:
      return 1;
    }
  case 4:
    op3 = read_nibble(saturn.PC + 2);
    op = op3 < 8 ? 0xf : 6;
    switch (op3 & 7) {
    case 0: trace_instr("DAT0=A"); /* DAT0=A */
      saturn.PC += 3;
      store(saturn.D0, saturn.A, op);
      return 0;
    case 1: trace_instr("DAT1=A"); /* DAT1=A */
      saturn.PC += 3;
      store(saturn.D1, saturn.A, op);
      return 0;
    case 2: trace_instr("A=DAT0"); /* A=DAT0 */
      saturn.PC += 3;
      recall(saturn.A, saturn.D0, op);
      return 0;
    case 3: trace_instr("A=DAT1"); /* A=DAT1 */
      saturn.PC += 3;
      recall(saturn.A, saturn.D1, op);
      return 0;
    case 4: trace_instr("DAT0=C"); /* DAT0=C */
      saturn.PC += 3;
      store(saturn.D0, saturn.C, op);
      return 0;
    case 5: trace_instr("DAT1=C"); /* DAT1=C */
      saturn.PC += 3;
      store(saturn.D1, saturn.C, op);
      return 0;
    case 6: trace_instr("C=DAT0"); /* C=DAT0 */
      saturn.PC += 3;
      recall(saturn.C, saturn.D0, op);
      return 0;
    case 7: trace_instr("C=DAT1"); /* C=DAT1 */
      saturn.PC += 3;
      recall(saturn.C, saturn.D1, op);
      return 0;
    default:
      return 1;
    }
  case 5:
    op3 = read_nibble(saturn.PC + 2);
    op4 = read_nibble(saturn.PC + 3);
    if (op3 >= 8) {
      switch (op3 & 7) {
      case 0: trace_instr("DAT0=A"); /* DAT0=A */
        saturn.PC += 4;
        store_n(saturn.D0, saturn.A, op4 + 1);
        return 0;
      case 1: trace_instr("DAT1=A"); /* DAT1=A */
        saturn.PC += 4;
        store_n(saturn.D1, saturn.A, op4 + 1);
        return 0;
      case 2: trace_instr("A=DAT0"); /* A=DAT0 */
        saturn.PC += 4;
        recall_n(saturn.A, saturn.D0, op4 + 1);
        return 0;
      case 3: trace_instr("A=DAT1"); /* A=DAT1 */
        saturn.PC += 4;
        recall_n(saturn.A, saturn.D1, op4 + 1);
        return 0;
      case 4: trace_instr("DAT0=C"); /* DAT0=C */
        saturn.PC += 4;
        store_n(saturn.D0, saturn.C, op4 + 1);
        return 0;
      case 5: trace_instr("DAT1=C"); /* DAT1=C */
        saturn.PC += 4;
        store_n(saturn.D1, saturn.C, op4 + 1);
        return 0;
      case 6: trace_instr("C=DAT0"); /* C=DAT0 */
        saturn.PC += 4;
        recall_n(saturn.C, saturn.D0, op4 + 1);
        return 0;
      case 7: trace_instr("C=DAT1"); /* C=DAT1 */
        saturn.PC += 4;
        recall_n(saturn.C, saturn.D1, op4 + 1);
        return 0;
      default:
        return 1;
      }
    } else {
      switch (op3) {
      case 0: trace_instr("DAT0=A"); /* DAT0=A */
        saturn.PC += 4;
        store(saturn.D0, saturn.A, op4);
        return 0;
      case 1: trace_instr("DAT1=A"); /* DAT1=A */
        saturn.PC += 4;
        store(saturn.D1, saturn.A, op4);
        return 0;
      case 2: trace_instr("A=DAT0"); /* A=DAT0 */
        saturn.PC += 4;
        recall(saturn.A, saturn.D0, op4);
        return 0;
      case 3: trace_instr("A=DAT1"); /* A=DAT1 */
        saturn.PC += 4;
        recall(saturn.A, saturn.D1, op4);
        return 0;
      case 4: trace_instr("DAT0=C"); /* DAT0=C */
        saturn.PC += 4;
        store(saturn.D0, saturn.C, op4);
        return 0;
      case 5: trace_instr("DAT1=C"); /* DAT1=C */
        saturn.PC += 4;
        store(saturn.D1, saturn.C, op4);
        return 0;
      case 6: trace_instr("C=DAT0"); /* C=DAT0 */
        saturn.PC += 4;
        recall(saturn.C, saturn.D0, op4);
        return 0;
      case 7: trace_instr("C=DAT1"); /* C=DAT1 */
        saturn.PC += 4;
        recall(saturn.C, saturn.D1, op4);
        return 0;
      default:
        return 1;
      }
    }
  case 6: trace_instr("D0=D0+  n");
    op3 = read_nibble(saturn.PC + 2);
    saturn.PC += 3;
    add_address(&saturn.D0, op3 + 1);
    return 0;
  case 7: trace_instr("D1=D1+  d");
    op3 = read_nibble(saturn.PC + 2);
    saturn.PC += 3;
    add_address(&saturn.D1, op3 + 1);
    return 0;
  case 8: trace_instr("D0=D0-  n");
    op3 = read_nibble(saturn.PC + 2);
    saturn.PC += 3;
    add_address(&saturn.D0, -(op3 + 1));
    return 0;
  case 9: trace_instr("D0=HEX hh");
    load_addr(&saturn.D0, saturn.PC + 2, 2);
    saturn.PC += 4;
    return 0;
  case 0xa: trace_instr("D0=HEX hhhh");
    load_addr(&saturn.D0, saturn.PC + 2, 4);
    saturn.PC += 6;
    return 0;
  case 0xb: trace_instr("D0=HEX hhhhh");
    load_addr(&saturn.D0, saturn.PC + 2, 5);
    saturn.PC += 7;
    return 0;
  case 0xc: trace_instr("D1=D1-  n");
    op3 = read_nibble(saturn.PC + 2);
    saturn.PC += 3;
    add_address(&saturn.D1, -(op3 + 1));
    return 0;
  case 0xd: trace_instr("D0=HEX hhhhh");
    load_addr(&saturn.D1, saturn.PC + 2, 2);
    saturn.PC += 4;
    return 0;
  case 0xe: trace_instr("D1=(4)  nnnn");
    load_addr(&saturn.D1, saturn.PC + 2, 4);
    saturn.PC += 6;
    return 0;
  case 0xf: trace_instr("D1=(5)  nnnnn");
    load_addr(&saturn.D1, saturn.PC + 2, 5);
    saturn.PC += 7;
    return 0;
  default:
    return 1;
  }
}

static inline int decode_8_thru_f(int op1) {
  int op2, op3, op4, op5, op6;

  op2 = read_nibble(saturn.PC + 1);
  switch (op1) {
  case 8:
    switch (op2) {
    case 0:
      return decode_group_80();
    case 1:
      op3 = read_nibble(saturn.PC + 2);
      switch (op3) {
      case 0: trace_instr("ASLC"); /* ASLC */
        saturn.PC += 3;
        shift_left_circ_register(saturn.A, W_FIELD);
        return 0;
      case 1: trace_instr("BSLC"); /* BSLC */
        saturn.PC += 3;
        shift_left_circ_register(saturn.B, W_FIELD);
        return 0;
      case 2: trace_instr("CSLC"); /* CSLC */
        saturn.PC += 3;
        shift_left_circ_register(saturn.C, W_FIELD);
        return 0;
      case 3: trace_instr("DSLC"); /* DSLC */
        saturn.PC += 3;
        shift_left_circ_register(saturn.D, W_FIELD);
        return 0;
      case 4: trace_instr("ASRC"); /* ASRC */
        saturn.PC += 3;
        shift_right_circ_register(saturn.A, W_FIELD);
        return 0;
      case 5: trace_instr("BSRC"); /* BSRC */
        saturn.PC += 3;
        shift_right_circ_register(saturn.B, W_FIELD);
        return 0;
      case 6: trace_instr("CSRC"); /* CSRC */
        saturn.PC += 3;
        shift_right_circ_register(saturn.C, W_FIELD);
        return 0;
      case 7: trace_instr("DSRC"); /* DSRC */
        saturn.PC += 3;
        shift_right_circ_register(saturn.D, W_FIELD);
        return 0;
      case 8: trace_instr("R = R +/- CON"); /* R = R +/- CON */
        op4 = read_nibble(saturn.PC + 3);
        op5 = read_nibble(saturn.PC + 4);
        op6 = read_nibble(saturn.PC + 5);
        if (op5 < 8) { /* PLUS */
          switch (op5 & 3) {
          case 0: trace_instr("A=A+CON"); /* A=A+CON */
            saturn.PC += 6;
            add_register_constant(saturn.A, op4, op6 + 1);
            return 0;
          case 1: trace_instr("B=B+CON"); /* B=B+CON */
            saturn.PC += 6;
            add_register_constant(saturn.B, op4, op6 + 1);
            return 0;
          case 2: trace_instr("C=C+CON"); /* C=C+CON */
            saturn.PC += 6;
            add_register_constant(saturn.C, op4, op6 + 1);
            return 0;
          case 3: trace_instr("D=D+CON"); /* D=D+CON */
            saturn.PC += 6;
            add_register_constant(saturn.D, op4, op6 + 1);
            return 0;
          default:
            return 1;
          }
        } else { /* MINUS */
          switch (op5 & 3) {
          case 0: trace_instr("A=A-CON"); /* A=A-CON */
            saturn.PC += 6;
            sub_register_constant(saturn.A, op4, op6 + 1);
            return 0;
          case 1: trace_instr("B=B-CON"); /* B=B-CON */
            saturn.PC += 6;
            sub_register_constant(saturn.B, op4, op6 + 1);
            return 0;
          case 2: trace_instr("C=C-CON"); /* C=C-CON */
            saturn.PC += 6;
            sub_register_constant(saturn.C, op4, op6 + 1);
            return 0;
          case 3: trace_instr("D=D-CON"); /* D=D-CON */
            saturn.PC += 6;
            sub_register_constant(saturn.D, op4, op6 + 1);
            return 0;
          default:
            return 1;
          }
        }
      case 9: trace_instr("R SRB FIELD"); /* R SRB FIELD */
        op4 = read_nibble(saturn.PC + 3);
        op5 = read_nibble(saturn.PC + 4);
        switch (op5 & 3) {
        case 0:
          saturn.PC += 5;
          shift_right_bit_register(saturn.A, op4);
          return 0;
        case 1:
          saturn.PC += 5;
          shift_right_bit_register(saturn.B, op4);
          return 0;
        case 2:
          saturn.PC += 5;
          shift_right_bit_register(saturn.C, op4);
          return 0;
        case 3:
          saturn.PC += 5;
          shift_right_bit_register(saturn.D, op4);
          return 0;
        default:
          return 1;
        }
      case 0xa: trace_instr("R = R FIELD, etc."); /* R = R FIELD, etc. */
        op4 = read_nibble(saturn.PC + 3);
        op5 = read_nibble(saturn.PC + 4);
        op6 = read_nibble(saturn.PC + 5);
        switch (op5) {
        case 0:
          switch (op6) {
          case 0: trace_instr("saturn.R0=A"); /* saturn.R0=A */
            saturn.PC += 6;
            copy_register(saturn.R0, saturn.A, op4);
            return 0;
          case 1: trace_instr("saturn.R1=A"); /* saturn.R1=A */
          case 5:
            saturn.PC += 6;
            copy_register(saturn.R1, saturn.A, op4);
            return 0;
          case 2: trace_instr("saturn.R2=A"); /* saturn.R2=A */
          case 6:
            saturn.PC += 6;
            copy_register(saturn.R2, saturn.A, op4);
            return 0;
          case 3: trace_instr("saturn.R3=A"); /* saturn.R3=A */
          case 7:
            saturn.PC += 6;
            copy_register(saturn.R3, saturn.A, op4);
            return 0;
          case 4: trace_instr("saturn.R4=A"); /* saturn.R4=A */
            saturn.PC += 6;
            copy_register(saturn.R4, saturn.A, op4);
            return 0;
          case 8: trace_instr("saturn.R0=C"); /* saturn.R0=C */
            saturn.PC += 6;
            copy_register(saturn.R0, saturn.C, op4);
            return 0;
          case 9: trace_instr("saturn.R1=C"); /* saturn.R1=C */
          case 0xd:
            saturn.PC += 6;
            copy_register(saturn.R1, saturn.C, op4);
            return 0;
          case 0xa: trace_instr("saturn.R2=C"); /* saturn.R2=C */
          case 0xe:
            saturn.PC += 6;
            copy_register(saturn.R2, saturn.C, op4);
            return 0;
          case 0xb: trace_instr("saturn.R3=C"); /* saturn.R3=C */
          case 0xf:
            saturn.PC += 6;
            copy_register(saturn.R3, saturn.C, op4);
            return 0;
          case 0xc: trace_instr("saturn.R4=C"); /* saturn.R4=C */
            saturn.PC += 6;
            copy_register(saturn.R4, saturn.C, op4);
            return 0;
          default:
            return 1;
          }
        case 1:
          switch (op6) {
          case 0: trace_instr("A=R0"); /* A=R0 */
            saturn.PC += 6;
            copy_register(saturn.A, saturn.R0, op4);
            return 0;
          case 1: trace_instr("A=R1"); /* A=R1 */
          case 5:
            saturn.PC += 6;
            copy_register(saturn.A, saturn.R1, op4);
            return 0;
          case 2: trace_instr("A=R2"); /* A=R2 */
          case 6:
            saturn.PC += 6;
            copy_register(saturn.A, saturn.R2, op4);
            return 0;
          case 3: trace_instr("A=R3"); /* A=R3 */
          case 7:
            saturn.PC += 6;
            copy_register(saturn.A, saturn.R3, op4);
            return 0;
          case 4: trace_instr("A=R4"); /* A=R4 */
            saturn.PC += 6;
            copy_register(saturn.A, saturn.R4, op4);
            return 0;
          case 8: trace_instr("C=R0"); /* C=R0 */
            saturn.PC += 6;
            copy_register(saturn.C, saturn.R0, op4);
            return 0;
          case 9: trace_instr("C=R1"); /* C=R1 */
          case 0xd:
            saturn.PC += 6;
            copy_register(saturn.C, saturn.R1, op4);
            return 0;
          case 0xa: trace_instr("C=R2"); /* C=R2 */
          case 0xe:
            saturn.PC += 6;
            copy_register(saturn.C, saturn.R2, op4);
            return 0;
          case 0xb: trace_instr("C=R3"); /* C=R3 */
          case 0xf:
            saturn.PC += 6;
            copy_register(saturn.C, saturn.R3, op4);
            return 0;
          case 0xc: trace_instr("C=R4"); /* C=R4 */
            saturn.PC += 6;
            copy_register(saturn.C, saturn.R4, op4);
            return 0;
          default:
            return 1;
          }
        case 2:
          switch (op6) {
          case 0: trace_instr("AR0EX"); /* AR0EX */
            saturn.PC += 6;
            exchange_register(saturn.A, saturn.R0, op4);
            return 0;
          case 1: trace_instr("AR1EX"); /* AR1EX */
          case 5:
            saturn.PC += 6;
            exchange_register(saturn.A, saturn.R1, op4);
            return 0;
          case 2: trace_instr("AR2EX"); /* AR2EX */
          case 6:
            saturn.PC += 6;
            exchange_register(saturn.A, saturn.R2, op4);
            return 0;
          case 3: trace_instr("AR3EX"); /* AR3EX */
          case 7:
            saturn.PC += 6;
            exchange_register(saturn.A, saturn.R3, op4);
            return 0;
          case 4: trace_instr("AR4EX"); /* AR4EX */
            saturn.PC += 6;
            exchange_register(saturn.A, saturn.R4, op4);
            return 0;
          case 8: trace_instr("CR0EX"); /* CR0EX */
            saturn.PC += 6;
            exchange_register(saturn.C, saturn.R0, op4);
            return 0;
          case 9: trace_instr("CR1EX"); /* CR1EX */
          case 0xd:
            saturn.PC += 6;
            exchange_register(saturn.C, saturn.R1, op4);
            return 0;
          case 0xa: trace_instr("CR2EX"); /* CR2EX */
          case 0xe:
            saturn.PC += 6;
            exchange_register(saturn.C, saturn.R2, op4);
            return 0;
          case 0xb: trace_instr("CR3EX"); /* CR3EX */
          case 0xf:
            saturn.PC += 6;
            exchange_register(saturn.C, saturn.R3, op4);
            return 0;
          case 0xc: trace_instr("CR4EX"); /* CR4EX */
            saturn.PC += 6;
            exchange_register(saturn.C, saturn.R4, op4);
            return 0;
          default:
            return 1;
          }
        default:
          return 1;
        }
      case 0xb:
        op4 = read_nibble(saturn.PC + 3);
        switch (op4) {
        case 2: trace_instr("PC=A"); /* PC=A */
          jumpaddr = dat_to_addr(saturn.A);
          saturn.PC = jumpaddr;
          return 0;
        case 3: trace_instr("PC=C"); /* PC=C */
          jumpaddr = dat_to_addr(saturn.C);
          saturn.PC = jumpaddr;
          return 0;
        case 4: trace_instr("A=PC"); /* A=PC */
          saturn.PC += 4;
          addr_to_dat(saturn.PC, saturn.A);
          return 0;
        case 5: trace_instr("C=PC"); /* C=PC */
          saturn.PC += 4;
          addr_to_dat(saturn.PC, saturn.C);
          return 0;
        case 6: trace_instr("APCEX"); /* APCEX */
          saturn.PC += 4;
          jumpaddr = dat_to_addr(saturn.A);
          addr_to_dat(saturn.PC, saturn.A);
          saturn.PC = jumpaddr;
          return 0;
        case 7: trace_instr("CPCEX"); /* CPCEX */
          saturn.PC += 4;
          jumpaddr = dat_to_addr(saturn.C);
          addr_to_dat(saturn.PC, saturn.C);
          saturn.PC = jumpaddr;
          return 0;
        default:
          return 1;
        }
      case 0xc: trace_instr("ASRB"); /* ASRB */
        saturn.PC += 3;
        shift_right_bit_register(saturn.A, W_FIELD);
        return 0;
      case 0xd: trace_instr("BSRB"); /* BSRB */
        saturn.PC += 3;
        shift_right_bit_register(saturn.B, W_FIELD);
        return 0;
      case 0xe: trace_instr("CSRB"); /* CSRB */
        saturn.PC += 3;
        shift_right_bit_register(saturn.C, W_FIELD);
        return 0;
      case 0xf: trace_instr("DSRB"); /* DSRB */
        saturn.PC += 3;
        shift_right_bit_register(saturn.D, W_FIELD);
        return 0;
      default:
        return 1;
      }
    case 2: trace_instr("CLRHST");
      op3 = read_nibble(saturn.PC + 2);
      saturn.PC += 3;
      clear_hardware_stat(op3);
      return 0;
    case 3: trace_instr("?MP/SB/SR/XM=0");
      op3 = read_nibble(saturn.PC + 2);
      saturn.CARRY = is_zero_hardware_stat(op3);
      if (saturn.CARRY) {
        saturn.PC += 3;
        op4 = read_nibbles(saturn.PC, 2);
        if (op4) {
          if (op4 & 0x80)
            op4 |= jumpmasks[2];
          jumpaddr = (saturn.PC + op4) & 0xfffff;
          saturn.PC = jumpaddr;
        } else {
          saturn.PC = pop_return_addr();
        }
      } else {
        saturn.PC += 5;
      }
      return 0;
    case 4:
    case 5:
      op3 = read_nibble(saturn.PC + 2);
      if (op2 == 4) { trace_instr("ST=0   n");
        saturn.PC += 3;
        clear_program_stat(op3);
      } else { trace_instr("ST=1   n");
        saturn.PC += 3;
        set_program_stat(op3);
      }
      return 0;
    case 6:
    case 7:
      op3 = read_nibble(saturn.PC + 2);
      if (op2 == 6) {
        trace_instr("?ST#1   n");
        saturn.CARRY = (get_program_stat(op3) == 0) ? 1 : 0;
      } else {
        trace_instr("?ST=1   n");
        saturn.CARRY = (get_program_stat(op3) != 0) ? 1 : 0;
      }
      if (saturn.CARRY) {
        saturn.PC += 3;
        op4 = read_nibbles(saturn.PC, 2);
        if (op4) {
          if (op4 & 0x80)
            op4 |= jumpmasks[2];
          jumpaddr = (saturn.PC + op4) & 0xfffff;
          saturn.PC = jumpaddr;
        } else {
          saturn.PC = pop_return_addr();
        }
      } else {
        saturn.PC += 5;
      }
      return 0;
    case 8:
    case 9:
      op3 = read_nibble(saturn.PC + 2);
      if (op2 == 8) {
        trace_instr("?P#   n");
        saturn.CARRY = (saturn.P != op3) ? 1 : 0;
      } else {
        trace_instr("?P=    n");
        saturn.CARRY = (saturn.P == op3) ? 1 : 0;
      }
      if (saturn.CARRY) {
        saturn.PC += 3;
        op4 = read_nibbles(saturn.PC, 2);
        if (op4) {
          if (op4 & 0x80)
            op4 |= jumpmasks[2];
          jumpaddr = (saturn.PC + op4) & 0xfffff;
          saturn.PC = jumpaddr;
        } else {
          saturn.PC = pop_return_addr();
        }
      } else {
        saturn.PC += 5;
      }
      return 0;
    case 0xa:
      op3 = read_nibble(saturn.PC + 2);
      switch (op3) {
      case 0: trace_instr("?A=B"); /* ?A=B */
        saturn.CARRY = is_equal_register(saturn.A, saturn.B, A_FIELD);
        break;
      case 1: trace_instr("?B=C"); /* ?B=C */
        saturn.CARRY = is_equal_register(saturn.B, saturn.C, A_FIELD);
        break;
      case 2: trace_instr("?A=C"); /* ?A=C */
        saturn.CARRY = is_equal_register(saturn.A, saturn.C, A_FIELD);
        break;
      case 3: trace_instr("?C=D"); /* ?C=D */
        saturn.CARRY = is_equal_register(saturn.C, saturn.D, A_FIELD);
        break;
      case 4: trace_instr("?A#B"); /* ?A#B */
        saturn.CARRY = is_not_equal_register(saturn.A, saturn.B, A_FIELD);
        break;
      case 5: trace_instr("?B#C"); /* ?B#C */
        saturn.CARRY = is_not_equal_register(saturn.B, saturn.C, A_FIELD);
        break;
      case 6: trace_instr("?A#C"); /* ?A#C */
        saturn.CARRY = is_not_equal_register(saturn.A, saturn.C, A_FIELD);
        break;
      case 7: trace_instr("?C#D"); /* ?C#D */
        saturn.CARRY = is_not_equal_register(saturn.C, saturn.D, A_FIELD);
        break;
      case 8: trace_instr("?A=0"); /* ?A=0 */
        saturn.CARRY = is_zero_register(saturn.A, A_FIELD);
        break;
      case 9: trace_instr("?B=0"); /* ?B=0 */
        saturn.CARRY = is_zero_register(saturn.B, A_FIELD);
        break;
      case 0xa: trace_instr("?C=0"); /* ?C=0 */
        saturn.CARRY = is_zero_register(saturn.C, A_FIELD);
        break;
      case 0xb: trace_instr("?D=0"); /* ?D=0 */
        saturn.CARRY = is_zero_register(saturn.D, A_FIELD);
        break;
      case 0xc: trace_instr("?A#0"); /* ?A#0 */
        saturn.CARRY = is_not_zero_register(saturn.A, A_FIELD);
        break;
      case 0xd: trace_instr("?B#0"); /* ?B#0 */
        saturn.CARRY = is_not_zero_register(saturn.B, A_FIELD);
        break;
      case 0xe: trace_instr("?C#0"); /* ?C#0 */
        saturn.CARRY = is_not_zero_register(saturn.C, A_FIELD);
        break;
      case 0xf: trace_instr("?D#0"); /* ?D#0 */
        saturn.CARRY = is_not_zero_register(saturn.D, A_FIELD);
        break;
      default:
        return 1;
      }
      if (saturn.CARRY) {
        saturn.PC += 3;
        op4 = read_nibbles(saturn.PC, 2);
        if (op4) {
          if (op4 & 0x80)
            op4 |= jumpmasks[2];
          jumpaddr = (saturn.PC + op4) & 0xfffff;
          saturn.PC = jumpaddr;
        } else {
          saturn.PC = pop_return_addr();
        }
      } else {
        saturn.PC += 5;
      }
      return 0;
    case 0xb:
      op3 = read_nibble(saturn.PC + 2);
      switch (op3) {
      case 0: trace_instr("?A>B"); /* ?A>B */
        saturn.CARRY = is_greater_register(saturn.A, saturn.B, A_FIELD);
        break;
      case 1: trace_instr("?B>C"); /* ?B>C */
        saturn.CARRY = is_greater_register(saturn.B, saturn.C, A_FIELD);
        break;
      case 2: trace_instr("?C>A"); /* ?C>A */
        saturn.CARRY = is_greater_register(saturn.C, saturn.A, A_FIELD);
        break;
      case 3: trace_instr("?D>C"); /* ?D>C */
        saturn.CARRY = is_greater_register(saturn.D, saturn.C, A_FIELD);
        break;
      case 4: trace_instr("?A<B"); /* ?A<B */
        saturn.CARRY = is_less_register(saturn.A, saturn.B, A_FIELD);
        break;
      case 5: trace_instr("?B<C"); /* ?B<C */
        saturn.CARRY = is_less_register(saturn.B, saturn.C, A_FIELD);
        break;
      case 6: trace_instr("?C<A"); /* ?C<A */
        saturn.CARRY = is_less_register(saturn.C, saturn.A, A_FIELD);
        break;
      case 7: trace_instr("?D<C"); /* ?D<C */
        saturn.CARRY = is_less_register(saturn.D, saturn.C, A_FIELD);
        break;
      case 8: trace_instr("?A>=B"); /* ?A>=B */
        saturn.CARRY =
            is_greater_or_equal_register(saturn.A, saturn.B, A_FIELD);
        break;
      case 9: trace_instr("?B>=C"); /* ?B>=C */
        saturn.CARRY =
            is_greater_or_equal_register(saturn.B, saturn.C, A_FIELD);
        break;
      case 0xa: trace_instr("?C>=A"); /* ?C>=A */
        saturn.CARRY =
            is_greater_or_equal_register(saturn.C, saturn.A, A_FIELD);
        break;
      case 0xb: trace_instr("?D>=C"); /* ?D>=C */
        saturn.CARRY =
            is_greater_or_equal_register(saturn.D, saturn.C, A_FIELD);
        break;
      case 0xc: trace_instr("?A<=B"); /* ?A<=B */
        saturn.CARRY = is_less_or_equal_register(saturn.A, saturn.B, A_FIELD);
        break;
      case 0xd: trace_instr("?B<=C"); /* ?B<=C */
        saturn.CARRY = is_less_or_equal_register(saturn.B, saturn.C, A_FIELD);
        break;
      case 0xe: trace_instr("?C<=A"); /* ?C<=A */
        saturn.CARRY = is_less_or_equal_register(saturn.C, saturn.A, A_FIELD);
        break;
      case 0xf: trace_instr("?D<=C"); /* ?D<=C */
        saturn.CARRY = is_less_or_equal_register(saturn.D, saturn.C, A_FIELD);
        break;
      default:
        return 1;
      }
      if (saturn.CARRY) {
        saturn.PC += 3;
        op4 = read_nibbles(saturn.PC, 2);
        if (op4) {
          if (op4 & 0x80)
            op4 |= jumpmasks[2];
          jumpaddr = (saturn.PC + op4) & 0xfffff;
          saturn.PC = jumpaddr;
        } else {
          saturn.PC = pop_return_addr();
        }
      } else {
        saturn.PC += 5;
      }
      return 0;
    case 0xc:
      op3 = read_nibbles(saturn.PC + 2, 4);
      if (op3 & 0x8000)
        op3 |= jumpmasks[4];
      jumpaddr = (saturn.PC + op3 + 2) & 0xfffff;
      saturn.PC = jumpaddr;
      return 0;
    case 0xd:
      op3 = read_nibbles(saturn.PC + 2, 5);
      jumpaddr = op3;
      saturn.PC = jumpaddr;
      return 0;
    case 0xe:
      op3 = read_nibbles(saturn.PC + 2, 4);
      if (op3 & 0x8000)
        op3 |= jumpmasks[4];
      jumpaddr = (saturn.PC + op3 + 6) & 0xfffff;
      push_return_addr(saturn.PC + 6);
      saturn.PC = jumpaddr;
      return 0;
    case 0xf:
      op3 = read_nibbles(saturn.PC + 2, 5);
      jumpaddr = op3;
      push_return_addr(saturn.PC + 7);
      saturn.PC = jumpaddr;
      return 0;
    default:
      return 1;
    }
  case 9:
    op3 = read_nibble(saturn.PC + 2);
    if (op2 < 8) {
      switch (op3) {
      case 0: trace_instr("?A=B"); /* ?A=B */
        saturn.CARRY = is_equal_register(saturn.A, saturn.B, op2);
        break;
      case 1: trace_instr("?B=C"); /* ?B=C */
        saturn.CARRY = is_equal_register(saturn.B, saturn.C, op2);
        break;
      case 2: trace_instr("?A=C"); /* ?A=C */
        saturn.CARRY = is_equal_register(saturn.A, saturn.C, op2);
        break;
      case 3: trace_instr("?C=D"); /* ?C=D */
        saturn.CARRY = is_equal_register(saturn.C, saturn.D, op2);
        break;
      case 4: trace_instr("?A#B"); /* ?A#B */
        saturn.CARRY = is_not_equal_register(saturn.A, saturn.B, op2);
        break;
      case 5: trace_instr("?B#C"); /* ?B#C */
        saturn.CARRY = is_not_equal_register(saturn.B, saturn.C, op2);
        break;
      case 6: trace_instr("?A#C"); /* ?A#C */
        saturn.CARRY = is_not_equal_register(saturn.A, saturn.C, op2);
        break;
      case 7: trace_instr("?C#D"); /* ?C#D */
        saturn.CARRY = is_not_equal_register(saturn.C, saturn.D, op2);
        break;
      case 8: trace_instr("?A=0"); /* ?A=0 */
        saturn.CARRY = is_zero_register(saturn.A, op2);
        break;
      case 9: trace_instr("?B=0"); /* ?B=0 */
        saturn.CARRY = is_zero_register(saturn.B, op2);
        break;
      case 0xa: trace_instr("?C=0"); /* ?C=0 */
        saturn.CARRY = is_zero_register(saturn.C, op2);
        break;
      case 0xb: trace_instr("?D=0"); /* ?D=0 */
        saturn.CARRY = is_zero_register(saturn.D, op2);
        break;
      case 0xc: trace_instr("?A#0"); /* ?A#0 */
        saturn.CARRY = is_not_zero_register(saturn.A, op2);
        break;
      case 0xd: trace_instr("?B#0"); /* ?B#0 */
        saturn.CARRY = is_not_zero_register(saturn.B, op2);
        break;
      case 0xe: trace_instr("?C#0"); /* ?C#0 */
        saturn.CARRY = is_not_zero_register(saturn.C, op2);
        break;
      case 0xf: trace_instr("?D#0"); /* ?D#0 */
        saturn.CARRY = is_not_zero_register(saturn.D, op2);
        break;
      default:
        return 1;
      }
    } else {
      op2 &= 7;
      switch (op3) {
      case 0: trace_instr("?A>B"); /* ?A>B */
        saturn.CARRY = is_greater_register(saturn.A, saturn.B, op2);
        break;
      case 1: trace_instr("?B>C"); /* ?B>C */
        saturn.CARRY = is_greater_register(saturn.B, saturn.C, op2);
        break;
      case 2: trace_instr("?C>A"); /* ?C>A */
        saturn.CARRY = is_greater_register(saturn.C, saturn.A, op2);
        break;
      case 3: trace_instr("?D>C"); /* ?D>C */
        saturn.CARRY = is_greater_register(saturn.D, saturn.C, op2);
        break;
      case 4: trace_instr("?A<B"); /* ?A<B */
        saturn.CARRY = is_less_register(saturn.A, saturn.B, op2);
        break;
      case 5: trace_instr("?B<C"); /* ?B<C */
        saturn.CARRY = is_less_register(saturn.B, saturn.C, op2);
        break;
      case 6: trace_instr("?C<A"); /* ?C<A */
        saturn.CARRY = is_less_register(saturn.C, saturn.A, op2);
        break;
      case 7: trace_instr("?D<C"); /* ?D<C */
        saturn.CARRY = is_less_register(saturn.D, saturn.C, op2);
        break;
      case 8: trace_instr("?A>=B"); /* ?A>=B */
        saturn.CARRY = is_greater_or_equal_register(saturn.A, saturn.B, op2);
        break;
      case 9: trace_instr("?B>=C"); /* ?B>=C */
        saturn.CARRY = is_greater_or_equal_register(saturn.B, saturn.C, op2);
        break;
      case 0xa: trace_instr("?C>=A"); /* ?C>=A */
        saturn.CARRY = is_greater_or_equal_register(saturn.C, saturn.A, op2);
        break;
      case 0xb: trace_instr("?D>=C"); /* ?D>=C */
        saturn.CARRY = is_greater_or_equal_register(saturn.D, saturn.C, op2);
        break;
      case 0xc: trace_instr("?A<=B"); /* ?A<=B */
        saturn.CARRY = is_less_or_equal_register(saturn.A, saturn.B, op2);
        break;
      case 0xd: trace_instr("?B<=C"); /* ?B<=C */
        saturn.CARRY = is_less_or_equal_register(saturn.B, saturn.C, op2);
        break;
      case 0xe: trace_instr("?C<=A"); /* ?C<=A */
        saturn.CARRY = is_less_or_equal_register(saturn.C, saturn.A, op2);
        break;
      case 0xf: trace_instr("?D<=C"); /* ?D<=C */
        saturn.CARRY = is_less_or_equal_register(saturn.D, saturn.C, op2);
        break;
      default:
        return 1;
      }
    }
    if (saturn.CARRY) {
      saturn.PC += 3;
      op4 = read_nibbles(saturn.PC, 2);
      if (op4) {
        if (op4 & 0x80)
          op4 |= jumpmasks[2];
        jumpaddr = (saturn.PC + op4) & 0xfffff;
        saturn.PC = jumpaddr;
      } else {
        saturn.PC = pop_return_addr();
      }
    } else {
      saturn.PC += 5;
    }
    return 0;
  case 0xa:
    op3 = read_nibble(saturn.PC + 2);
    if (op2 < 8) {
      switch (op3) {
      case 0: trace_instr("A=A+B"); /* A=A+B */
        saturn.PC += 3;
        add_register(saturn.A, saturn.A, saturn.B, op2);
        return 0;
      case 1: trace_instr("B=B+C"); /* B=B+C */
        saturn.PC += 3;
        add_register(saturn.B, saturn.B, saturn.C, op2);
        return 0;
      case 2: trace_instr("C=C+A"); /* C=C+A */
        saturn.PC += 3;
        add_register(saturn.C, saturn.C, saturn.A, op2);
        return 0;
      case 3: trace_instr("D=D+C"); /* D=D+C */
        saturn.PC += 3;
        add_register(saturn.D, saturn.D, saturn.C, op2);
        return 0;
      case 4: trace_instr("A=A+A"); /* A=A+A */
        saturn.PC += 3;
        add_register(saturn.A, saturn.A, saturn.A, op2);
        return 0;
      case 5: trace_instr("B=B+B"); /* B=B+B */
        saturn.PC += 3;
        add_register(saturn.B, saturn.B, saturn.B, op2);
        return 0;
      case 6: trace_instr("C=C+C"); /* C=C+C */
        saturn.PC += 3;
        add_register(saturn.C, saturn.C, saturn.C, op2);
        return 0;
      case 7: trace_instr("D=D+D"); /* D=D+D */
        saturn.PC += 3;
        add_register(saturn.D, saturn.D, saturn.D, op2);
        return 0;
      case 8: trace_instr("B=B+A"); /* B=B+A */
        saturn.PC += 3;
        add_register(saturn.B, saturn.B, saturn.A, op2);
        return 0;
      case 9: trace_instr("C=C+B"); /* C=C+B */
        saturn.PC += 3;
        add_register(saturn.C, saturn.C, saturn.B, op2);
        return 0;
      case 0xa: trace_instr("A=A+C"); /* A=A+C */
        saturn.PC += 3;
        add_register(saturn.A, saturn.A, saturn.C, op2);
        return 0;
      case 0xb: trace_instr("C=C+D"); /* C=C+D */
        saturn.PC += 3;
        add_register(saturn.C, saturn.C, saturn.D, op2);
        return 0;
      case 0xc: trace_instr("A=A-1"); /* A=A-1 */
        saturn.PC += 3;
        dec_register(saturn.A, op2);
        return 0;
      case 0xd: trace_instr("B=B-1"); /* B=B-1 */
        saturn.PC += 3;
        dec_register(saturn.B, op2);
        return 0;
      case 0xe: trace_instr("C=C-1"); /* C=C-1 */
        saturn.PC += 3;
        dec_register(saturn.C, op2);
        return 0;
      case 0xf: trace_instr("D=D-1"); /* D=D-1 */
        saturn.PC += 3;
        dec_register(saturn.D, op2);
        return 0;
      default:
        return 1;
      }
    } else {
      op2 &= 7;
      switch (op3) {
      case 0: trace_instr("A=0"); /* A=0 */
        saturn.PC += 3;
        zero_register(saturn.A, op2);
        return 0;
      case 1: trace_instr("B=0"); /* B=0 */
        saturn.PC += 3;
        zero_register(saturn.B, op2);
        return 0;
      case 2: trace_instr("C=0"); /* C=0 */
        saturn.PC += 3;
        zero_register(saturn.C, op2);
        return 0;
      case 3: trace_instr("D=0"); /* D=0 */
        saturn.PC += 3;
        zero_register(saturn.D, op2);
        return 0;
      case 4: trace_instr("A=B"); /* A=B */
        saturn.PC += 3;
        copy_register(saturn.A, saturn.B, op2);
        return 0;
      case 5: trace_instr("B=C"); /* B=C */
        saturn.PC += 3;
        copy_register(saturn.B, saturn.C, op2);
        return 0;
      case 6: trace_instr("C=A"); /* C=A */
        saturn.PC += 3;
        copy_register(saturn.C, saturn.A, op2);
        return 0;
      case 7: trace_instr("D=C"); /* D=C */
        saturn.PC += 3;
        copy_register(saturn.D, saturn.C, op2);
        return 0;
      case 8: trace_instr("B=A"); /* B=A */
        saturn.PC += 3;
        copy_register(saturn.B, saturn.A, op2);
        return 0;
      case 9: trace_instr("C=B"); /* C=B */
        saturn.PC += 3;
        copy_register(saturn.C, saturn.B, op2);
        return 0;
      case 0xa: trace_instr("A=C"); /* A=C */
        saturn.PC += 3;
        copy_register(saturn.A, saturn.C, op2);
        return 0;
      case 0xb: trace_instr("C=D"); /* C=D */
        saturn.PC += 3;
        copy_register(saturn.C, saturn.D, op2);
        return 0;
      case 0xc: trace_instr("ABEX"); /* ABEX */
        saturn.PC += 3;
        exchange_register(saturn.A, saturn.B, op2);
        return 0;
      case 0xd: trace_instr("BCEX"); /* BCEX */
        saturn.PC += 3;
        exchange_register(saturn.B, saturn.C, op2);
        return 0;
      case 0xe: trace_instr("ACEX"); /* ACEX */
        saturn.PC += 3;
        exchange_register(saturn.A, saturn.C, op2);
        return 0;
      case 0xf: trace_instr("CDEX"); /* CDEX */
        saturn.PC += 3;
        exchange_register(saturn.C, saturn.D, op2);
        return 0;
      default:
        return 1;
      }
    }
  case 0xb:
    op3 = read_nibble(saturn.PC + 2);
    if (op2 < 8) {
      switch (op3) {
      case 0: trace_instr("A=A-B"); /* A=A-B */
        saturn.PC += 3;
        sub_register(saturn.A, saturn.A, saturn.B, op2);
        return 0;
      case 1: trace_instr("B=B-C"); /* B=B-C */
        saturn.PC += 3;
        sub_register(saturn.B, saturn.B, saturn.C, op2);
        return 0;
      case 2: trace_instr("C=C-A"); /* C=C-A */
        saturn.PC += 3;
        sub_register(saturn.C, saturn.C, saturn.A, op2);
        return 0;
      case 3: trace_instr("D=D-C"); /* D=D-C */
        saturn.PC += 3;
        sub_register(saturn.D, saturn.D, saturn.C, op2);
        return 0;
      case 4: trace_instr("A=A+1"); /* A=A+1 */
        saturn.PC += 3;
        inc_register(saturn.A, op2);
        return 0;
      case 5: trace_instr("B=B+1"); /* B=B+1 */
        saturn.PC += 3;
        inc_register(saturn.B, op2);
        return 0;
      case 6: trace_instr("C=C+1"); /* C=C+1 */
        saturn.PC += 3;
        inc_register(saturn.C, op2);
        return 0;
      case 7: trace_instr("D=D+1"); /* D=D+1 */
        saturn.PC += 3;
        inc_register(saturn.D, op2);
        return 0;
      case 8: trace_instr("B=B-A"); /* B=B-A */
        saturn.PC += 3;
        sub_register(saturn.B, saturn.B, saturn.A, op2);
        return 0;
      case 9: trace_instr("C=C-B"); /* C=C-B */
        saturn.PC += 3;
        sub_register(saturn.C, saturn.C, saturn.B, op2);
        return 0;
      case 0xa: trace_instr("A=A-C"); /* A=A-C */
        saturn.PC += 3;
        sub_register(saturn.A, saturn.A, saturn.C, op2);
        return 0;
      case 0xb: trace_instr("C=C-D"); /* C=C-D */
        saturn.PC += 3;
        sub_register(saturn.C, saturn.C, saturn.D, op2);
        return 0;
      case 0xc: trace_instr("A=B-A"); /* A=B-A */
        saturn.PC += 3;
        sub_register(saturn.A, saturn.B, saturn.A, op2);
        return 0;
      case 0xd: trace_instr("B=C-B"); /* B=C-B */
        saturn.PC += 3;
        sub_register(saturn.B, saturn.C, saturn.B, op2);
        return 0;
      case 0xe: trace_instr("C=A-C"); /* C=A-C */
        saturn.PC += 3;
        sub_register(saturn.C, saturn.A, saturn.C, op2);
        return 0;
      case 0xf: trace_instr("D=C-D"); /* D=C-D */
        saturn.PC += 3;
        sub_register(saturn.D, saturn.C, saturn.D, op2);
        return 0;
      default:
        return 1;
      }
    } else {
      op2 &= 7;
      switch (op3) {
      case 0: trace_instr("ASL"); /* ASL */
        saturn.PC += 3;
        shift_left_register(saturn.A, op2);
        return 0;
      case 1: trace_instr("BSL"); /* BSL */
        saturn.PC += 3;
        shift_left_register(saturn.B, op2);
        return 0;
      case 2: trace_instr("CSL"); /* CSL */
        saturn.PC += 3;
        shift_left_register(saturn.C, op2);
        return 0;
      case 3: trace_instr("DSL"); /* DSL */
        saturn.PC += 3;
        shift_left_register(saturn.D, op2);
        return 0;
      case 4: trace_instr("ASR"); /* ASR */
        saturn.PC += 3;
        shift_right_register(saturn.A, op2);
        return 0;
      case 5: trace_instr("BSR"); /* BSR */
        saturn.PC += 3;
        shift_right_register(saturn.B, op2);
        return 0;
      case 6: trace_instr("CSR"); /* CSR */
        saturn.PC += 3;
        shift_right_register(saturn.C, op2);
        return 0;
      case 7: trace_instr("DSR"); /* DSR */
        saturn.PC += 3;
        shift_right_register(saturn.D, op2);
        return 0;
      case 8: trace_instr("A=-A"); /* A=-A */
        saturn.PC += 3;
        complement_2_register(saturn.A, op2);
        return 0;
      case 9: trace_instr("B=-B"); /* B=-B */
        saturn.PC += 3;
        complement_2_register(saturn.B, op2);
        return 0;
      case 0xa: trace_instr("C=-C"); /* C=-C */
        saturn.PC += 3;
        complement_2_register(saturn.C, op2);
        return 0;
      case 0xb: trace_instr("D=-D"); /* D=-D */
        saturn.PC += 3;
        complement_2_register(saturn.D, op2);
        return 0;
      case 0xc: trace_instr("A=-A-1"); /* A=-A-1 */
        saturn.PC += 3;
        complement_1_register(saturn.A, op2);
        return 0;
      case 0xd: trace_instr("B=-B-1"); /* B=-B-1 */
        saturn.PC += 3;
        complement_1_register(saturn.B, op2);
        return 0;
      case 0xe: trace_instr("C=-C-1"); /* C=-C-1 */
        saturn.PC += 3;
        complement_1_register(saturn.C, op2);
        return 0;
      case 0xf: trace_instr("D=-D-1"); /* D=-D-1 */
        saturn.PC += 3;
        complement_1_register(saturn.D, op2);
        return 0;
      default:
        return 1;
      }
    }
  case 0xc:
    switch (op2) {
    case 0: trace_instr("A=A+B"); /* A=A+B */
      saturn.PC += 2;
      add_register(saturn.A, saturn.A, saturn.B, A_FIELD);
      return 0;
    case 1: trace_instr("B=B+C"); /* B=B+C */
      saturn.PC += 2;
      add_register(saturn.B, saturn.B, saturn.C, A_FIELD);
      return 0;
    case 2: trace_instr("C=C+A"); /* C=C+A */
      saturn.PC += 2;
      add_register(saturn.C, saturn.C, saturn.A, A_FIELD);
      return 0;
    case 3: trace_instr("D=D+C"); /* D=D+C */
      saturn.PC += 2;
      add_register(saturn.D, saturn.D, saturn.C, A_FIELD);
      return 0;
    case 4: trace_instr("A=A+A"); /* A=A+A */
      saturn.PC += 2;
      add_register(saturn.A, saturn.A, saturn.A, A_FIELD);
      return 0;
    case 5: trace_instr("B=B+B"); /* B=B+B */
      saturn.PC += 2;
      add_register(saturn.B, saturn.B, saturn.B, A_FIELD);
      return 0;
    case 6: trace_instr("C=C+C"); /* C=C+C */
      saturn.PC += 2;
      add_register(saturn.C, saturn.C, saturn.C, A_FIELD);
      return 0;
    case 7: trace_instr("D=D+D"); /* D=D+D */
      saturn.PC += 2;
      add_register(saturn.D, saturn.D, saturn.D, A_FIELD);
      return 0;
    case 8: trace_instr("B=B+A"); /* B=B+A */
      saturn.PC += 2;
      add_register(saturn.B, saturn.B, saturn.A, A_FIELD);
      return 0;
    case 9: trace_instr("C=C+B"); /* C=C+B */
      saturn.PC += 2;
      add_register(saturn.C, saturn.C, saturn.B, A_FIELD);
      return 0;
    case 0xa: trace_instr("A=A+C"); /* A=A+C */
      saturn.PC += 2;
      add_register(saturn.A, saturn.A, saturn.C, A_FIELD);
      return 0;
    case 0xb: trace_instr("C=C+D"); /* C=C+D */
      saturn.PC += 2;
      add_register(saturn.C, saturn.C, saturn.D, A_FIELD);
      return 0;
    case 0xc: trace_instr("A=A-1"); /* A=A-1 */
      saturn.PC += 2;
      dec_register(saturn.A, A_FIELD);
      return 0;
    case 0xd: trace_instr("B=B-1"); /* B=B-1 */
      saturn.PC += 2;
      dec_register(saturn.B, A_FIELD);
      return 0;
    case 0xe: trace_instr("C=C-1"); /* C=C-1 */
      saturn.PC += 2;
      dec_register(saturn.C, A_FIELD);
      return 0;
    case 0xf: trace_instr("D=D-1"); /* D=D-1 */
      saturn.PC += 2;
      dec_register(saturn.D, A_FIELD);
      return 0;
    default:
      return 1;
    }
  case 0xd:
    switch (op2) {
    case 0: trace_instr("A=0"); /* A=0 */
      saturn.PC += 2;
      zero_register(saturn.A, A_FIELD);
      return 0;
    case 1: trace_instr("B=0"); /* B=0 */
      saturn.PC += 2;
      zero_register(saturn.B, A_FIELD);
      return 0;
    case 2: trace_instr("C=0"); /* C=0 */
      saturn.PC += 2;
      zero_register(saturn.C, A_FIELD);
      return 0;
    case 3: trace_instr("D=0"); /* D=0 */
      saturn.PC += 2;
      zero_register(saturn.D, A_FIELD);
      return 0;
    case 4: trace_instr("A=B"); /* A=B */
      saturn.PC += 2;
      copy_register(saturn.A, saturn.B, A_FIELD);
      return 0;
    case 5: trace_instr("B=C"); /* B=C */
      saturn.PC += 2;
      copy_register(saturn.B, saturn.C, A_FIELD);
      return 0;
    case 6: trace_instr("C=A"); /* C=A */
      saturn.PC += 2;
      copy_register(saturn.C, saturn.A, A_FIELD);
      return 0;
    case 7: trace_instr("D=C"); /* D=C */
      saturn.PC += 2;
      copy_register(saturn.D, saturn.C, A_FIELD);
      return 0;
    case 8: trace_instr("B=A"); /* B=A */
      saturn.PC += 2;
      copy_register(saturn.B, saturn.A, A_FIELD);
      return 0;
    case 9: trace_instr("C=B"); /* C=B */
      saturn.PC += 2;
      copy_register(saturn.C, saturn.B, A_FIELD);
      return 0;
    case 0xa: trace_instr("A=C"); /* A=C */
      saturn.PC += 2;
      copy_register(saturn.A, saturn.C, A_FIELD);
      return 0;
    case 0xb: trace_instr("C=D"); /* C=D */
      saturn.PC += 2;
      copy_register(saturn.C, saturn.D, A_FIELD);
      return 0;
    case 0xc: trace_instr("ABEX"); /* ABEX */
      saturn.PC += 2;
      exchange_register(saturn.A, saturn.B, A_FIELD);
      return 0;
    case 0xd: trace_instr("BCEX"); /* BCEX */
      saturn.PC += 2;
      exchange_register(saturn.B, saturn.C, A_FIELD);
      return 0;
    case 0xe: trace_instr("ACEX"); /* ACEX */
      saturn.PC += 2;
      exchange_register(saturn.A, saturn.C, A_FIELD);
      return 0;
    case 0xf: trace_instr("CDEX"); /* CDEX */
      saturn.PC += 2;
      exchange_register(saturn.C, saturn.D, A_FIELD);
      return 0;
    default:
      return 1;
    }
  case 0xe:
    switch (op2) {
    case 0: trace_instr("A=A-B"); /* A=A-B */
      saturn.PC += 2;
      sub_register(saturn.A, saturn.A, saturn.B, A_FIELD);
      return 0;
    case 1: trace_instr("B=B-C"); /* B=B-C */
      saturn.PC += 2;
      sub_register(saturn.B, saturn.B, saturn.C, A_FIELD);
      return 0;
    case 2: trace_instr("C=C-A"); /* C=C-A */
      saturn.PC += 2;
      sub_register(saturn.C, saturn.C, saturn.A, A_FIELD);
      return 0;
    case 3: trace_instr("D=D-C"); /* D=D-C */
      saturn.PC += 2;
      sub_register(saturn.D, saturn.D, saturn.C, A_FIELD);
      return 0;
    case 4: trace_instr("A=A+1"); /* A=A+1 */
      saturn.PC += 2;
      inc_register(saturn.A, A_FIELD);
      return 0;
    case 5: trace_instr("B=B+1"); /* B=B+1 */
      saturn.PC += 2;
      inc_register(saturn.B, A_FIELD);
      return 0;
    case 6: trace_instr("C=C+1"); /* C=C+1 */
      saturn.PC += 2;
      inc_register(saturn.C, A_FIELD);
      return 0;
    case 7: trace_instr("D=D+1"); /* D=D+1 */
      saturn.PC += 2;
      inc_register(saturn.D, A_FIELD);
      return 0;
    case 8: trace_instr("B=B-A"); /* B=B-A */
      saturn.PC += 2;
      sub_register(saturn.B, saturn.B, saturn.A, A_FIELD);
      return 0;
    case 9: trace_instr("C=C-B"); /* C=C-B */
      saturn.PC += 2;
      sub_register(saturn.C, saturn.C, saturn.B, A_FIELD);
      return 0;
    case 0xa: trace_instr("A=A-C"); /* A=A-C */
      saturn.PC += 2;
      sub_register(saturn.A, saturn.A, saturn.C, A_FIELD);
      return 0;
    case 0xb: trace_instr("C=C-D"); /* C=C-D */
      saturn.PC += 2;
      sub_register(saturn.C, saturn.C, saturn.D, A_FIELD);
      return 0;
    case 0xc: trace_instr("A=B-A"); /* A=B-A */
      saturn.PC += 2;
      sub_register(saturn.A, saturn.B, saturn.A, A_FIELD);
      return 0;
    case 0xd: trace_instr("B=C-B"); /* B=C-B */
      saturn.PC += 2;
      sub_register(saturn.B, saturn.C, saturn.B, A_FIELD);
      return 0;
    case 0xe: trace_instr("C=A-C"); /* C=A-C */
      saturn.PC += 2;
      sub_register(saturn.C, saturn.A, saturn.C, A_FIELD);
      return 0;
    case 0xf: trace_instr("D=C-D"); /* D=C-D */
      saturn.PC += 2;
      sub_register(saturn.D, saturn.C, saturn.D, A_FIELD);
      return 0;
    default:
      return 1;
    }
  case 0xf:
    switch (op2) {
    case 0: trace_instr("ASL"); /* ASL */
      saturn.PC += 2;
      shift_left_register(saturn.A, A_FIELD);
      return 0;
    case 1: trace_instr("BSL"); /* BSL */
      saturn.PC += 2;
      shift_left_register(saturn.B, A_FIELD);
      return 0;
    case 2: trace_instr("CSL"); /* CSL */
      saturn.PC += 2;
      shift_left_register(saturn.C, A_FIELD);
      return 0;
    case 3: trace_instr("DSL"); /* DSL */
      saturn.PC += 2;
      shift_left_register(saturn.D, A_FIELD);
      return 0;
    case 4: trace_instr("ASR"); /* ASR */
      saturn.PC += 2;
      shift_right_register(saturn.A, A_FIELD);
      return 0;
    case 5: trace_instr("BSR"); /* BSR */
      saturn.PC += 2;
      shift_right_register(saturn.B, A_FIELD);
      return 0;
    case 6: trace_instr("CSR"); /* CSR */
      saturn.PC += 2;
      shift_right_register(saturn.C, A_FIELD);
      return 0;
    case 7: trace_instr("DSR"); /* DSR */
      saturn.PC += 2;
      shift_right_register(saturn.D, A_FIELD);
      return 0;
    case 8: trace_instr("A=-A"); /* A=-A */
      saturn.PC += 2;
      complement_2_register(saturn.A, A_FIELD);
      return 0;
    case 9: trace_instr("B=-B"); /* B=-B */
      saturn.PC += 2;
      complement_2_register(saturn.B, A_FIELD);
      return 0;
    case 0xa: trace_instr("C=-C"); /* C=-C */
      saturn.PC += 2;
      complement_2_register(saturn.C, A_FIELD);
      return 0;
    case 0xb: trace_instr("D=-D"); /* D=-D */
      saturn.PC += 2;
      complement_2_register(saturn.D, A_FIELD);
      return 0;
    case 0xc: trace_instr("A=-A-1"); /* A=-A-1 */
      saturn.PC += 2;
      complement_1_register(saturn.A, A_FIELD);
      return 0;
    case 0xd: trace_instr("B=-B-1"); /* B=-B-1 */
      saturn.PC += 2;
      complement_1_register(saturn.B, A_FIELD);
      return 0;
    case 0xe: trace_instr("C=-C-1"); /* C=-C-1 */
      saturn.PC += 2;
      complement_1_register(saturn.C, A_FIELD);
      return 0;
    case 0xf: trace_instr("D=-D-1"); /* D=-D-1 */
      saturn.PC += 2;
      complement_1_register(saturn.D, A_FIELD);
      return 0;
    default:
      return 1;
    }
  default:
    return 1;
  }
}

inline int step_instruction(void) {
  int op0, op1, op2, op3;
  int stop = 0;

  jumpaddr = 0;

  op0 = read_nibble(saturn.PC);
  switch (op0) {
  case 0:
    op1 = read_nibble(saturn.PC + 1);
    switch (op1) {
    case 0: trace_instr("RTNSXM"); /* RTNSXM */
      saturn.XM = 1;
      saturn.PC = pop_return_addr();
      break;
    case 1: trace_instr("RTN"); /* RTN */
      saturn.PC = pop_return_addr();
      break;
    case 2: trace_instr("RTNSC"); /* RTNSC */
      saturn.CARRY = 1;
      saturn.PC = pop_return_addr();
      break;
    case 3: trace_instr("RTNCC"); /* RTNCC */
      saturn.CARRY = 0;
      saturn.PC = pop_return_addr();
      break;
    case 4: trace_instr("SETHEX"); /* SETHEX */
      saturn.PC += 2;
      saturn.hexmode = HEX;
      break;
    case 5: trace_instr("SETDEC"); /* SETDEC */
      saturn.PC += 2;
      saturn.hexmode = DEC;
      break;
    case 6: trace_instr("RSTK=C"); /* RSTK=C */
      jumpaddr = dat_to_addr(saturn.C);
      push_return_addr(jumpaddr);
      saturn.PC += 2;
      break;
    case 7: trace_instr("C=RSTK"); /* C=RSTK */
      saturn.PC += 2;
      jumpaddr = pop_return_addr();
      addr_to_dat(jumpaddr, saturn.C);
      break;
    case 8: trace_instr("CLRST"); /* CLRST */
      saturn.PC += 2;
      clear_status();
      break;
    case 9: trace_instr("C=ST"); /* C=ST */
      saturn.PC += 2;
      status_to_register(saturn.C);
      break;
    case 0xa: trace_instr("ST=C"); /* ST=C */
      saturn.PC += 2;
      register_to_status(saturn.C);
      break;
    case 0xb: trace_instr("CSTEX"); /* CSTEX */
      saturn.PC += 2;
      swap_register_status(saturn.C);
      break;
    case 0xc: trace_instr("P=P+1"); /* P=P+1 */
      saturn.PC += 2;
      if (saturn.P == 0xf) {
        saturn.P = 0;
        saturn.CARRY = 1;
      } else {
        saturn.P += 1;
        saturn.CARRY = 0;
      }
      break;
    case 0xd: trace_instr("P=P-1"); /* P=P-1 */
      saturn.PC += 2;
      if (saturn.P == 0) {
        saturn.P = 0xf;
        saturn.CARRY = 1;
      } else {
        saturn.P -= 1;
        saturn.CARRY = 0;
      }
      break;
    case 0xe:
      op2 = read_nibble(saturn.PC + 2);
      op3 = read_nibble(saturn.PC + 3);
      switch (op3) {
      case 0: trace_instr("A=A&B"); /* A=A&B */
        saturn.PC += 4;
        and_register(saturn.A, saturn.A, saturn.B, op2);
        break;
      case 1: trace_instr("B=B&C"); /* B=B&C */
        saturn.PC += 4;
        and_register(saturn.B, saturn.B, saturn.C, op2);
        break;
      case 2: trace_instr("C=C&A"); /* C=C&A */
        saturn.PC += 4;
        and_register(saturn.C, saturn.C, saturn.A, op2);
        break;
      case 3: trace_instr("D=D&C"); /* D=D&C */
        saturn.PC += 4;
        and_register(saturn.D, saturn.D, saturn.C, op2);
        break;
      case 4: trace_instr("B=B&A"); /* B=B&A */
        saturn.PC += 4;
        and_register(saturn.B, saturn.B, saturn.A, op2);
        break;
      case 5: trace_instr("C=C&B"); /* C=C&B */
        saturn.PC += 4;
        and_register(saturn.C, saturn.C, saturn.B, op2);
        break;
      case 6: trace_instr("A=A&C"); /* A=A&C */
        saturn.PC += 4;
        and_register(saturn.A, saturn.A, saturn.C, op2);
        break;
      case 7: trace_instr("C=C&D"); /* C=C&D */
        saturn.PC += 4;
        and_register(saturn.C, saturn.C, saturn.D, op2);
        break;
      case 8: trace_instr("A=A!B"); /* A=A!B */
        saturn.PC += 4;
        or_register(saturn.A, saturn.A, saturn.B, op2);
        break;
      case 9: trace_instr("B=B!C"); /* B=B!C */
        saturn.PC += 4;
        or_register(saturn.B, saturn.B, saturn.C, op2);
        break;
      case 0xa: trace_instr("C=C!A"); /* C=C!A */
        saturn.PC += 4;
        or_register(saturn.C, saturn.C, saturn.A, op2);
        break;
      case 0xb: trace_instr("D=D!C"); /* D=D!C */
        saturn.PC += 4;
        or_register(saturn.D, saturn.D, saturn.C, op2);
        break;
      case 0xc: trace_instr("B=B!A"); /* B=B!A */
        saturn.PC += 4;
        or_register(saturn.B, saturn.B, saturn.A, op2);
        break;
      case 0xd: trace_instr("C=C!B"); /* C=C!B */
        saturn.PC += 4;
        or_register(saturn.C, saturn.C, saturn.B, op2);
        break;
      case 0xe: trace_instr("A=A!C"); /* A=A!C */
        saturn.PC += 4;
        or_register(saturn.A, saturn.A, saturn.C, op2);
        break;
      case 0xf: trace_instr("C=C!D"); /* C=C!D */
        saturn.PC += 4;
        or_register(saturn.C, saturn.C, saturn.D, op2);
        break;
      default:
        stop = 1;
        break;
      }
      break;
    case 0xf: trace_instr("RTI"); /* RTI */
      do_return_interupt();
      break;
    default:
      stop = 1;
      break;
    }
    break;
  case 1:
    stop = decode_group_1();
    break;
  case 2: trace_instr("P=     n");
    op2 = read_nibble(saturn.PC + 1);
    saturn.PC += 2;
    saturn.P = op2;
    break;
  case 3: trace_instr("LCHEX  h..h");
    op2 = read_nibble(saturn.PC + 1);
    load_constant(saturn.C, op2 + 1, saturn.PC + 2);
    saturn.PC += 3 + op2;
    break;
  case 4: trace_instr("GOC    label");
    op2 = read_nibbles(saturn.PC + 1, 2);
    if (op2 == 0x02) {
      trace_instr("NOP3");
      saturn.PC += 3;
    } else if (saturn.CARRY != 0) {
      if (op2) {
        if (op2 & 0x80)
          op2 |= jumpmasks[2];
        jumpaddr = (saturn.PC + op2 + 1) & 0xfffff;
        saturn.PC = jumpaddr;
      } else {
        trace_instr("RTNC");
        saturn.PC = pop_return_addr();
      }
    } else {
      trace_instr("RTNC");
      saturn.PC += 3;
    }
    break;
  case 5: trace_instr("GONC   label");
    if (saturn.CARRY == 0) {
      op2 = read_nibbles(saturn.PC + 1, 2);
      if (op2) {
        if (op2 & 0x80)
          op2 |= jumpmasks[2];
        jumpaddr = (saturn.PC + op2 + 1) & 0xfffff;
        saturn.PC = jumpaddr;
      } else {
        saturn.PC = pop_return_addr();
      }
    } else {
      saturn.PC += 3;
    }
    break;
  case 6:
    op2 = read_nibbles(saturn.PC + 1, 3);
    if (op2 == 0x003) {
      saturn.PC += 4;
    } else if (op2 == 0x004) {
      op3 = read_nibbles(saturn.PC + 4, 1);
      saturn.PC += 5;
      if (op3 != 0) {
        enter_debugger |= TRAP_INSTRUCTION;
        return 1;
      }
    } else {
      if (op2 & 0x800)
        op2 |= jumpmasks[3];
      jumpaddr = (op2 + saturn.PC + 1) & 0xfffff;
      saturn.PC = jumpaddr;
    }
    break;
  case 7: trace_instr("GOSUB  label");
    op2 = read_nibbles(saturn.PC + 1, 3);
    if (op2 & 0x800)
      op2 |= jumpmasks[3];
    jumpaddr = (op2 + saturn.PC + 4) & 0xfffff;
    push_return_addr(saturn.PC + 4);
    saturn.PC = jumpaddr;
    break;
  default:
    stop = decode_8_thru_f(op0);
    break;
  }
  instructions++;
  if (stop) {
    enter_debugger |= ILLEGAL_INSTRUCTION;
  }
  return stop;
}

inline void schedule(void) {
  t1_t2_ticks ticks;
  unsigned long steps;
  static unsigned long old_stat_instr;
  static unsigned long old_sched_instr;

  steps = instructions - old_sched_instr;
  old_sched_instr = instructions;

#ifdef DEBUG_SCHED
  fprintf(stderr, "schedule called after %ld instructions\n", steps);
#endif

  if ((sched_timer2 -= steps) <= 0) {
    if (!saturn.intenable) {
      sched_timer2 = SCHED_TIMER2;
    } else {
      sched_timer2 = saturn.t2_tick;
    }
    saturn.t2_instr += steps;
    if (saturn.t2_ctrl & 0x01) {
      saturn.timer2--;
    }
    if (saturn.timer2 == 0 && (saturn.t2_ctrl & 0x02)) {
      saturn.t2_ctrl |= 0x08;
      do_interupt();
    }
  }
  schedule_event = sched_timer2;

#ifdef DEBUG_SCHED
  fprintf(stderr, "next timer 2 step: %ld, event: %ld\n", sched_timer2,
          schedule_event);
#endif

  if (device_check) {
    device_check = 0;
    if ((sched_display -= steps) <= 0) {
      if (device.display_touched)
        device.display_touched -= steps;
      if (device.display_touched < 0)
        device.display_touched = 1;
#ifdef DEBUG_DISP_SCHED
      fprintf(stderr, "check_device: disp_when %d, disp_touched %d\n",
              sched_display, device.display_touched);
#endif
    }
    check_devices();
    sched_display = SCHED_NEVER;
    if (device.display_touched) {
      if (device.display_touched < sched_display)
        sched_display = device.display_touched - 1;
      if (sched_display < schedule_event)
        schedule_event = sched_display;
    }
  }

  if ((sched_receive -= steps) <= 0) {
    sched_receive = SCHED_RECEIVE;
    if ((saturn.rcs & 0x01) == 0) {
      receive_char();
    }
  }
  if (sched_receive < schedule_event)
    schedule_event = sched_receive;

#ifdef DEBUG_SCHED
  fprintf(stderr, "next receive: %ld, event: %ld\n", sched_receive,
          schedule_event);
#endif

  if ((sched_adjtime -= steps) <= 0) {

    sched_adjtime = SCHED_ADJTIME;

    if (saturn.PC < SrvcIoStart || saturn.PC > SrvcIoEnd) {

      ticks = get_t1_t2();
      if (saturn.t2_ctrl & 0x01) {
        saturn.timer2 = ticks.t2_ticks;
      }

      if ((saturn.t2_ctrl & 0x08) == 0 && saturn.timer2 <= 0) {
        if (saturn.t2_ctrl & 0x02) {
          saturn.t2_ctrl |= 0x08;
          do_interupt();
        }
      }

      adj_time_pending = 0;

      saturn.timer1 = set_t1 - ticks.t1_ticks;
      if ((saturn.t1_ctrl & 0x08) == 0 && saturn.timer1 <= 0) {
        if (saturn.t1_ctrl & 0x02) {
          saturn.t1_ctrl |= 0x08;
          do_interupt();
        }
      }
      saturn.timer1 &= 0x0f;

    } else {

      adj_time_pending = 1;
    }
  }
  if (sched_adjtime < schedule_event)
    schedule_event = sched_adjtime;

#ifdef DEBUG_SCHED
  fprintf(stderr, "next adjtime: %ld, event: %ld\n", sched_adjtime,
          schedule_event);
#endif

  if ((sched_timer1 -= steps) <= 0) {
    if (!saturn.intenable) {
      sched_timer1 = SCHED_TIMER1;
    } else {
      sched_timer1 = saturn.t1_tick;
    }
    saturn.t1_instr += steps;
    saturn.timer1 = (saturn.timer1 - 1) & 0xf;
    if (saturn.timer1 == 0 && (saturn.t1_ctrl & 0x02)) {
      saturn.t1_ctrl |= 0x08;
      do_interupt();
    }
  }
  if (sched_timer1 < schedule_event)
    schedule_event = sched_timer1;

#ifdef DEBUG_SCHED
  fprintf(stderr, "next timer 1 step: %ld, event: %ld\n", sched_timer1,
          schedule_event);
#endif

  if ((sched_statistics -= steps) <= 0) {
    sched_statistics = SCHED_STATISTICS;
    run = get_timer(RUN_TIMER);
    delta_t_1 = s_1 - old_s_1;
    delta_t_16 = s_16 - old_s_16;
    old_s_1 = s_1;
    old_s_16 = s_16;
    delta_i = instructions - old_stat_instr;
    old_stat_instr = instructions;
    if (delta_t_1 > 0) {
      t1_i_per_tick =
          ((NR_SAMPLES - 1) * t1_i_per_tick + (delta_i / delta_t_16)) /
          NR_SAMPLES;
      t2_i_per_tick = t1_i_per_tick / 512;
      saturn.i_per_s =
          ((NR_SAMPLES - 1) * saturn.i_per_s + (delta_i / delta_t_1)) /
          NR_SAMPLES;
    } else {
      t1_i_per_tick = 8192;
      t2_i_per_tick = 16;
    }
    saturn.t1_tick = t1_i_per_tick;
    saturn.t2_tick = t2_i_per_tick;

#ifdef DEBUG_TIMER
    if (delta_t_1 > 0) {
#if 0
      fprintf(stderr, "I/s = %ld, T1 I/TICK = %d (%ld), T2 I/TICK = %d (%ld)\n",
              saturn.i_per_s, saturn.t1_tick, t1_i_per_tick,
              saturn.t2_tick, t2_i_per_tick);
#else
      fprintf(stderr, "I/s = %ld, T1 I/TICK = %d, T2 I/TICK = %d (%ld)\n",
              saturn.i_per_s, saturn.t1_tick, saturn.t2_tick, t2_i_per_tick);
#endif
    }
#endif
  }
  if (sched_statistics < schedule_event)
    schedule_event = sched_statistics;

#ifdef DEBUG_SCHED
  fprintf(stderr, "next statistics: %ld, event: %ld\n", sched_statistics,
          schedule_event);
#endif

  if ((sched_instr_rollover -= steps) <= 0) {
    sched_instr_rollover = SCHED_INSTR_ROLLOVER;
    instructions = 1;
    old_sched_instr = 1;
    reset_timer(RUN_TIMER);
    reset_timer(IDLE_TIMER);
    start_timer(RUN_TIMER);
  }
  if (sched_instr_rollover < schedule_event)
    schedule_event = sched_instr_rollover;

#ifdef DEBUG_SCHED
  fprintf(stderr, "next instruction rollover: %ld, event: %ld\n",
          sched_instr_rollover, schedule_event);
#endif

  schedule_event--;

  if (got_alarm) {
    got_alarm = 0;
    GetEvent();
  }
}

int emulate(void) {
  struct timeval tv;
  struct timeval tv2;
#ifndef SOLARIS
  struct timezone tz;
#endif

  reset_timer(T1_TIMER);
  reset_timer(RUN_TIMER);
  reset_timer(IDLE_TIMER);

  set_accesstime();
  start_timer(T1_TIMER);

  start_timer(RUN_TIMER);

  sched_timer1 = t1_i_per_tick = saturn.t1_tick;
  sched_timer2 = t2_i_per_tick = saturn.t2_tick;

  set_t1 = saturn.timer1;

  do {
    memset(current_opcode, '\0', sizeof(current_opcode));
    memset(current_instr, '\0', sizeof(current_instr));
    printf("%x\t", saturn.PC);
    step_instruction();
    printf("%16s : %s ", current_opcode, current_instr);
    printf("\n");

    if (saturn.shutdown) do_shutdown();

    {
      int i;
      for (i = 0;
           i < sizeof(saturn.keybuf.rows) / sizeof(saturn.keybuf.rows[0]);
           i++) {
        if (saturn.keybuf.rows[i] || throttle) {
          gettimeofday(&tv, &tz);
          while ((tv.tv_sec == tv2.tv_sec) &&
                 ((tv.tv_usec - tv2.tv_usec) < 2)) {
            gettimeofday(&tv, &tz);
          }

          tv2.tv_usec = tv.tv_usec;
          tv2.tv_sec = tv.tv_sec;
          break;
        }
      }
    }

    /* We need to throttle the speed here. */

    if (schedule_event < 0) {
      // puts("bug");
      //	schedule_event = 0;
    }
    if (schedule_event-- <= 0) {
      schedule();
    }
  } while (!enter_debugger);

  return 0;
}
