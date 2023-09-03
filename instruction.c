#include "instruction.h"

word4_t read_nibble(uint8_t mem[], word20_t a) {
    unsigned long a8 = a.u >> 2;
    uint8_t mem8 = mem[a.u];

    word4_t nibble;
    if (a.u % 2)
        nibble.u = mem8 >> 4;
    else
        nibble.u = mem8;

    return nibble;
}

word20_t inc_20(word20_t a, int i) {
    a.u += i;
    return a;
}

word4_t get_op(int offset) {
    word20_t code_ptr = saturn.PC;
    code_ptr.u += offset;

    word4_t op = read_nibble(hp48_rom, code_ptr);

    return op;
}

word4_t pop_return_addr() {
      return saturn.rstk[saturn.rstkp--];
}

void execution(Cpu* cpu) {
    word4_t op0 = get_op(0);
    word4_t op1 = get_op(1);
    switch (op0.u) {
        case 0:
        switch (op1.u) {
            case 0: /* RTNSXM */
                saturn.HST.XM = 1;
                saturn.PC.u = pop_return_addr();
                break;
            case 1: /* RTN */
                saturn.PC.u = pop_return_addr();
                break;
            case 2: /* RTNSC */
                saturn.CARRY = 1;
      saturn.PC = pop_return_addr();
      break;
    case 3: /* RTNCC */
      saturn.CARRY = 0;
      saturn.PC = pop_return_addr();
      break;
    case 4: /* SETHEX */
      saturn.PC += 2;
      saturn.hexmode = HEX;
      break;
    case 5: /* SETDEC */
      saturn.PC += 2;
      saturn.hexmode = DEC;
      break;
    case 6: /* RSTK=C */
      jumpaddr = dat_to_addr(saturn.C);
      push_return_addr(jumpaddr);
      saturn.PC += 2;
      break;
    case 7: /* C=RSTK */
      saturn.PC += 2;
      jumpaddr = pop_return_addr();
      addr_to_dat(jumpaddr, saturn.C);
      break;
    case 8: /* CLRST */
      saturn.PC += 2;
      clear_status();
      break;
    case 9: /* C=ST */
      saturn.PC += 2;
      status_to_register(saturn.C);
      break;
    case 0xa: /* ST=C */
      saturn.PC += 2;
      register_to_status(saturn.C);
      break;
    case 0xb: /* CSTEX */
      saturn.PC += 2;
      swap_register_status(saturn.C);
      break;
    case 0xc: /* P=P+1 */
      saturn.PC += 2;
      if (saturn.P == 0xf) {
        saturn.P = 0;
        saturn.CARRY = 1;
      } else {
        saturn.P += 1;
        saturn.CARRY = 0;
      }
      break;
    case 0xd: /* P=P-1 */
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
      case 0: /* A=A&B */
        saturn.PC += 4;
        and_register(saturn.A, saturn.A, saturn.B, op2);
        break;
      case 1: /* B=B&C */
        saturn.PC += 4;
        and_register(saturn.B, saturn.B, saturn.C, op2);
        break;
      case 2: /* C=C&A */
        saturn.PC += 4;
        and_register(saturn.C, saturn.C, saturn.A, op2);
        break;
      case 3: /* D=D&C */
        saturn.PC += 4;
        and_register(saturn.D, saturn.D, saturn.C, op2);
        break;
      case 4: /* B=B&A */
        saturn.PC += 4;
        and_register(saturn.B, saturn.B, saturn.A, op2);
        break;
      case 5: /* C=C&B */
        saturn.PC += 4;
        and_register(saturn.C, saturn.C, saturn.B, op2);
        break;
      case 6: /* A=A&C */
        saturn.PC += 4;
        and_register(saturn.A, saturn.A, saturn.C, op2);
        break;
      case 7: /* C=C&D */
        saturn.PC += 4;
        and_register(saturn.C, saturn.C, saturn.D, op2);
        break;
      case 8: /* A=A!B */
        saturn.PC += 4;
        or_register(saturn.A, saturn.A, saturn.B, op2);
        break;
      case 9: /* B=B!C */
        saturn.PC += 4;
        or_register(saturn.B, saturn.B, saturn.C, op2);
        break;
      case 0xa: /* C=C!A */
        saturn.PC += 4;
        or_register(saturn.C, saturn.C, saturn.A, op2);
        break;
      case 0xb: /* D=D!C */
        saturn.PC += 4;
        or_register(saturn.D, saturn.D, saturn.C, op2);
        break;
      case 0xc: /* B=B!A */
        saturn.PC += 4;
        or_register(saturn.B, saturn.B, saturn.A, op2);
        break;
      case 0xd: /* C=C!B */
        saturn.PC += 4;
        or_register(saturn.C, saturn.C, saturn.B, op2);
        break;
      case 0xe: /* A=A!C */
        saturn.PC += 4;
        or_register(saturn.A, saturn.A, saturn.C, op2);
        break;
      case 0xf: /* C=C!D */
        saturn.PC += 4;
        or_register(saturn.C, saturn.C, saturn.D, op2);
        break;
      default:
        stop = 1;
        break;
      }
      break;
    case 0xf: /* RTI */
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
  case 2:
    op2 = read_nibble(saturn.PC + 1);
    saturn.PC += 2;
    saturn.P = op2;
    break;
  case 3:
    op2 = read_nibble(saturn.PC + 1);
    load_constant(saturn.C, op2 + 1, saturn.PC + 2);
    saturn.PC += 3 + op2;
    break;
  case 4:
    op2 = read_nibbles(saturn.PC + 1, 2);
    if (op2 == 0x02) {
      saturn.PC += 3;
    } else if (saturn.CARRY != 0) {
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
  case 5:
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
  case 7:
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
