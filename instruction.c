#include "instruction.h"

word4_t read_nibble(uint8_t mem[], word20_t a)
{
	unsigned long a8 = a.u >> 2;
	uint8_t mem8 = mem[a8];

	word4_t nibble;
	if (a.u % 2)
		nibble.u = mem8 >> 4;
	else
		nibble.u = mem8;

	return nibble;
}

word20_t word20_add(word20_t a, int i)
{
	word20_t n = a;
	n.u += i;
	return n;
}

void set_carry()
{
	saturn.CARRY.u = 1;
}

void clear_carry()
{
	saturn.CARRY.u = 0;
}

word20_t word20_adc(word20_t a, int i)
{
	unsigned long l = a.u;

	l += i;
	if (l & ~0xFFFFF)
		set_carry();
	else
		clear_carry();

	word20_t result = { l };
	return result;
}

word4_t get_op(int offset)
{
	word20_t code_ptr = saturn.PC;
	code_ptr.u += offset;

	word4_t op = read_nibble(hp48_rom, code_ptr);

	return op;
}

word20_t pop_return_addr()
{
	word20_t a = saturn.return_stack[saturn.return_stack_idx];
	saturn.return_stack_idx--;
	return a;
}

void push_return_addr(word20_t a)
{
	saturn.return_stack_idx++;	// no overflow handlimg
	saturn.return_stack[saturn.return_stack_idx] = a;
}

/* Using the register_t type to ba able to do field masking magic */
void register_to_status(register_t * r)
{
	register_t old_status = { saturn.ST.u };
	old_status.x = r->x;
	saturn.ST.u = old_status.w;
}

void status_to_register(register_t * r)
{
	word12_t status = { saturn.ST.u };	// only copy the 12 bits
	r->x = status.u;
}

void swap_register_status(register_t * r)
{
	register_t old_status = { saturn.ST.u };
	register_to_status(r);
	r->x = old_status.x;
}

void clear_status(void)
{
	register_t old_status = { saturn.ST.u };
	old_status.x = 0;
	saturn.ST.u = old_status.w;
}

enum fields {
	P = 0,
	WP, XS, X, S, M, B, W,

	A = -1,
};

word4_t get_nibble_register(register_t * r, word4_t nibble_idx)
{
	uint8_t nibble_idx_bits = nibble_idx.u * 4;
	uint64_t value = r->w >> nibble_idx_bits;
	word4_t result = { value };
	return result;
}

void set_nibble_register(register_t * r, word4_t nibble_idx, word4_t value)
{
	uint8_t nibble_idx_bits = nibble_idx.u * 4;
	uint64_t nibble_idx_mask = 0xF << nibble_idx_bits;

	r->w &= ~nibble_idx_mask;
	r->w |= value.u << nibble_idx_bits;
}

uint64_t get_mask(int fs)
{
	switch (fs) {
	case P:{
			uint8_t nibble_idx_bits = saturn.P.u * 4;
			uint64_t nibble_idx_mask = 0xF << nibble_idx_bits;
			return nibble_idx_mask;
		}
	case WP:{
			uint8_t nibble_idx_bits = saturn.P.u * 4;
			uint64_t nibble_idx_mask_reverse =
			    0xFFFFFFFFFFFFFFFF << nibble_idx_bits;
			uint64_t nibble_idx_mask = ~nibble_idx_mask_reverse;
			return nibble_idx_mask;
		}
	case XS:
		return 0x0000000000000F00;
	case B:
		return 0x00000000000000FF;
	case X:
		return 0x0000000000000FFF;
	case M:
		return 0x0FFFFFFFFFFFF000;
	case S:
		return 0xF000000000000000;
	case A:
		return 0x00000000000FFFFF;
	case W:
		return 0xFFFFFFFFFFFFFFFF;
	}
}

static
void and_register_mask(register_t * res, register_t * r1, register_t * r2,
		       uint64_t nibble_mask)
{
	uint64_t r1_masked = r1->w & nibble_mask;
	uint64_t r2_masked = r2->w & nibble_mask;
	uint64_t value = r1_masked & r2_masked;
	res->w &= ~nibble_mask;
	res->w |= value;
}

static
void and_register(register_t * res, register_t * r1, register_t * r2, int field)
{
	uint64_t nibble_mask = get_mask(field);
	and_register_mask(res, r1, r2, nibble_mask);
}

static
void or_register_mask(register_t * res, register_t * r1, register_t * r2,
		      uint64_t nibble_mask)
{
	uint64_t r1_masked = r1->w & nibble_mask;
	uint64_t r2_masked = r2->w & nibble_mask;
	uint64_t value = r1_masked | r2_masked;
	res->w &= ~nibble_mask;
	res->w |= value;
}

static
void or_register(register_t * res, register_t * r1, register_t * r2, int field)
{
	uint64_t nibble_mask = get_mask(field);
	or_register(res, r1, r2, nibble_mask);
}

void do_return_interrupt(void);

int decode_group_0(void)
{
	word4_t op1, op2, op3;

	op1 = get_op(1);
	switch (op1.u) {
	case 0:		/* RTNSXM */
		saturn.HST.XM = 1;
		saturn.PC = pop_return_addr();
		break;
	case 1:		/* RTN */
		saturn.PC = pop_return_addr();
		break;
	case 2:		/* RTNSC */
		saturn.CARRY.u = 1;
		saturn.PC = pop_return_addr();
		break;
	case 3:		/* RTNCC */
		saturn.CARRY.u = 0;
		saturn.PC = pop_return_addr();
		break;
	case 4:		/* SETHEX */
		saturn.PC.u += 2;
		saturn.hexmode.u = HEX;
		break;
	case 5:		/* SETDEC */
		saturn.PC.u += 2;
		saturn.hexmode.u = DEC;
		break;
	case 6:		/* RSTK=C */
		saturn.PC.u += 2;
		push_return_addr((word20_t) saturn.C.a);
		break;
	case 7:		/* C=RSTK */
		saturn.PC.u += 2;
		saturn.C.a = pop_return_addr().u;
		break;
	case 8:		/* CLRST */
		saturn.PC.u += 2;
		clear_status();
		break;
	case 9:		/* C=ST */
		saturn.PC.u += 2;
		status_to_register(&saturn.C);
		break;
	case 0xa:		/* ST=C */
		saturn.PC.u += 2;
		register_to_status(&saturn.C);
		break;
	case 0xb:		/* CSTEX */
		saturn.PC.u += 2;
		swap_register_status(&saturn.C);
		break;
	case 0xc:		/* P=P+1 */
		saturn.PC.u += 2;
		if (saturn.P.u == 0xf) {
			saturn.P.u = 0;
			saturn.CARRY.u = 1;
		} else {
			saturn.P.u += 1;
			saturn.CARRY.u = 0;
		}
		break;
	case 0xd:		/* P=P-1 */
		saturn.PC.u += 2;
		if (saturn.P.u == 0) {
			saturn.P.u = 0xf;
			saturn.CARRY.u = 1;
		} else {
			saturn.P.u -= 1;
			saturn.CARRY.u = 0;
		}
		break;
	case 0xe:
		op2 = get_op(2);
		op3 = get_op(3);
		switch (op3.u) {
		case 0:	/* A=A&B */
			saturn.PC.u += 4;
			and_register(&saturn.A, &saturn.A, &saturn.B, op2.u);
			break;
		case 1:	/* B=B&C */
			saturn.PC.u += 4;
			and_register(&saturn.B, &saturn.B, &saturn.C, op2.u);
			break;
		case 2:	/* C=C&A */
			saturn.PC.u += 4;
			and_register(&saturn.C, &saturn.C, &saturn.A, op2.u);
			break;
		case 3:	/* D=D&C */
			saturn.PC.u += 4;
			and_register(&saturn.D, &saturn.D, &saturn.C, op2.u);
			break;
		case 4:	/* B=B&A */
			saturn.PC.u += 4;
			and_register(&saturn.B, &saturn.B, &saturn.A, op2.u);
			break;
		case 5:	/* C=C&B */
			saturn.PC.u += 4;
			and_register(&saturn.C, &saturn.C, &saturn.B, op2.u);
			break;
		case 6:	/* A=A&C */
			saturn.PC.u += 4;
			and_register(&saturn.A, &saturn.A, &saturn.C, op2.u);
			break;
		case 7:	/* C=C&D */
			saturn.PC.u += 4;
			and_register(&saturn.C, &saturn.C, &saturn.D, op2.u);
			break;
		case 8:	/* A=A!B */
			saturn.PC.u += 4;
			or_register(&saturn.A, &saturn.A, &saturn.B, op2.u);
			break;
		case 9:	/* B=B!C */
			saturn.PC.u += 4;
			or_register(&saturn.B, &saturn.B, &saturn.C, op2.u);
			break;
		case 0xa:	/* C=C!A */
			saturn.PC.u += 4;
			or_register(&saturn.C, &saturn.C, &saturn.A, op2.u);
			break;
		case 0xb:	/* D=D!C */
			saturn.PC.u += 4;
			or_register(&saturn.D, &saturn.D, &saturn.C, op2.u);
			break;
		case 0xc:	/* B=B!A */
			saturn.PC.u += 4;
			or_register(&saturn.B, &saturn.B, &saturn.A, op2.u);
			break;
		case 0xd:	/* C=C!B */
			saturn.PC.u += 4;
			or_register(&saturn.C, &saturn.C, &saturn.B, op2.u);
			break;
		case 0xe:	/* A=A!C */
			saturn.PC.u += 4;
			or_register(&saturn.A, &saturn.A, &saturn.C, op2.u);
			break;
		case 0xf:	/* C=C!D */
			saturn.PC.u += 4;
			or_register(&saturn.C, &saturn.C, &saturn.D, op2.u);
			break;
		default:
			return 1;
			break;
		}
		break;
	case 0xf:		/* RTI */
		do_return_interrupt();
		break;
	default:
		return 1;
		break;
	}

	return 0;
}

int execution()
{
	word4_t op0, op1, op2, op3;
	int stop;

	op0 = get_op(0);
	word20_t jumpaddr;
	switch (op0.u) {
	case 0:
		stop = decode_group_0();
	case 1:
		stop = decode_group_1();
		break;
	case 2:
		op2 = get_op(1);
		saturn.PC.u += 2;
		saturn.P = op2;
		break;
	case 3:
		op2 = get_op(1);
		load_constant(saturn.C, op2 + 1, saturn.PC + 2);
		saturn.PC.u += 3 + op2;
		break;
	case 4:
		op2 = read_nibbles(saturn.PC + 1, 2);
		if (op2 == 0x02) {
			saturn.PC.u += 3;
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
			saturn.PC.u += 3;
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
			saturn.PC.u += 3;
		}
		break;
	case 6:
		op2 = read_nibbles(saturn.PC + 1, 3);
		if (op2 == 0x003) {
			saturn.PC.u += 4;
		} else if (op2 == 0x004) {
			op3 = read_nibbles(saturn.PC + 4, 1);
			saturn.PC.u += 5;
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

	return stop;
}
