#include <assert.h>

#include "cpu.h"

void write_dev_mem(long addr, int val)
{
}

int read_dev_mem(long addr)
{
	return 0x00;
}

static inline int calc_crc(int nib)
{
	saturn.crc = (saturn.crc >> 4) ^ (((saturn.crc ^ nib) & 0xf) * 0x1081);
	return nib;
}

int read_nibble_compressed(const unsigned char mem[], long addr_in_nibbles)
{
	unsigned long addr_in_bytes = addr_in_nibbles >> 2;
	unsigned char value_byte = mem[addr_in_bytes];

	unsigned char value_nibble;
	if (addr_in_nibbles % 2)
		value_nibble = value_byte >> 4;
	else
		value_nibble = value_byte & 0x0F;

	return value_nibble;
}

/* from memory.c */
__attribute__((weak))
void trace_nibble_rom(int nibble)
{
}

int read_nibble(long addr)
{
	addr &= 0xfffff;

	if (addr >= 0x100 && addr <= 0x13F) {
		return read_dev_mem(addr);
	}

	if (addr > 0x70000 && addr <= 0x7FFFF) {
		return saturn.ram[addr - 0x70000];
	}

	if (addr < 0x80000) {
		int nibble = read_nibble_compressed(saturn.rom, addr);
		trace_nibble_rom(nibble);
		return nibble;
	}
	// PORT1/2 not supported
	return 0x00;
}

int read_nibble_crc(long addr)
{
	addr &= 0xfffff;

	if (addr >= 0x100 && addr <= 0x13F) {
		return read_dev_mem(addr);
	}

	int nibble = read_nibble(addr);
	calc_crc(nibble);
	return nibble;
}

void write_nibble(long addr, int val)
{
	addr &= 0xfffff;
	val &= 0x0f;

	if (addr >= 0x100 && addr <= 0x13F) {
		write_dev_mem(addr, val);
	}

	if (addr > 0x70000 && addr <= 0x7FFFF) {
		saturn.ram[addr - 0x70000] = val;
	}
	// writing to ROM
	assert(0);

	return;
}

int read_nibbles(long addr, int nb)
{
	int v = read_nibble(addr);
	for (int i = 1; i < nb; i++) {
		v <<= 4;
		v |= read_nibble(addr + i);
	}
	return v;
}

/* from actions.c */

void clear_program_stat(int n)
{
	saturn.PSTAT[n] = 0;
}

void set_program_stat(int n)
{
	saturn.PSTAT[n] = 1;
}

int get_program_stat(int n)
{
	return saturn.PSTAT[n];
}

void register_to_status(unsigned char *r)
{
	int i;

	for (i = 0; i < 12; i++) {
		saturn.PSTAT[i] = (r[i / 4] >> (i % 4)) & 1;
	}
}

void status_to_register(unsigned char *r)
{
	int i;

	for (i = 0; i < 12; i++) {
		if (saturn.PSTAT[i]) {
			r[i / 4] |= 1 << (i % 4);
		} else {
			r[i / 4] &= ~(1 << (i % 4)) & 0xf;
		}
	}
}

void swap_register_status(unsigned char *r)
{
	int i, tmp;

	for (i = 0; i < 12; i++) {
		tmp = saturn.PSTAT[i];
		saturn.PSTAT[i] = (r[i / 4] >> (i % 4)) & 1;
		if (tmp) {
			r[i / 4] |= 1 << (i % 4);
		} else {
			r[i / 4] &= ~(1 << (i % 4)) & 0xf;
		}
	}
}

void clear_status(void)
{
	int i;

	for (i = 0; i < 12; i++) {
		saturn.PSTAT[i] = 0;
	}
}

void set_register_nibble(unsigned char *reg, int n, unsigned char val)
{
	reg[n] = val;
}

unsigned char get_register_nibble(unsigned char *reg, int n)
{
	return reg[n];
}

void set_register_bit(unsigned char *reg, int n)
{
	reg[n / 4] |= (1 << (n % 4));
}

void clear_register_bit(unsigned char *reg, int n)
{
	reg[n / 4] &= ~(1 << (n % 4));
}

int get_register_bit(unsigned char *reg, int n)
{
	return ((int)(reg[n / 4] & (1 << (n % 4))) > 0) ? 1 : 0;
}

void push_return_addr(long addr)
{
	int i;

	if (++saturn.rstkp >= NR_RSTK) {
		for (i = 1; i < NR_RSTK; i++)
			saturn.rstk[i - 1] = saturn.rstk[i];
		saturn.rstkp--;
	}
	saturn.rstk[saturn.rstkp] = addr;
}

long pop_return_addr(void)
{
	if (saturn.rstkp < 0)
		return 0;
	return saturn.rstk[saturn.rstkp--];
}

void do_return_interupt(void)
{
	if (saturn.int_pending) {
		saturn.int_pending = 0;
		saturn.intenable = 0;
		saturn.PC = 0xf;
	} else {
		saturn.PC = pop_return_addr();
		saturn.intenable = 1;
	}
}

void do_interupt(void)
{
	if (saturn.intenable) {
		push_return_addr(saturn.PC);
		saturn.PC = 0xf;
		saturn.intenable = 0;
	}
}

void set_hardware_stat(int op)
{
	if (op & 1)
		saturn.XM = 1;
	if (op & 2)
		saturn.SB = 1;
	if (op & 4)
		saturn.SR = 1;
	if (op & 8)
		saturn.MP = 1;
}

void clear_hardware_stat(int op)
{
	if (op & 1)
		saturn.XM = 0;
	if (op & 2)
		saturn.SB = 0;
	if (op & 4)
		saturn.SR = 0;
	if (op & 8)
		saturn.MP = 0;
}

int is_zero_hardware_stat(int op)
{
	if (op & 1)
		if (saturn.XM != 0)
			return 0;
	if (op & 2)
		if (saturn.SB != 0)
			return 0;
	if (op & 4)
		if (saturn.SR != 0)
			return 0;
	if (op & 8)
		if (saturn.MP != 0)
			return 0;
	return 1;
}

char *make_hexstr(long addr, int n)
{
	static char str[44];
	int i, t, trunc;

	trunc = 0;
	if (n > 40) {
		n = 40;
		trunc = 1;
	}
	for (i = 0; i < n; i++) {
		t = read_nibble(addr + i);
		if (t <= 9)
			str[i] = '0' + t;
		else
			str[i] = 'a' + (t - 10);
	}
	str[n] = '\0';
	if (trunc) {
		str[n] = '.';
		str[n + 1] = '.';
		str[n + 2] = '.';
		str[n + 3] = '\0';
	}
	return str;
}

void load_constant(unsigned char *reg, int n, long addr)
{
	int i, p;

	p = saturn.P;
	for (i = 0; i < n; i++) {
		reg[p] = read_nibble(addr + i);
		p = (p + 1) & 0xf;
	}
}

static long nibble_masks[16] = {
	0x0000000f, 0x000000f0, 0x00000f00, 0x0000f000,
	0x000f0000, 0x00f00000, 0x0f000000, 0xf0000000,
	0x0000000f, 0x000000f0, 0x00000f00, 0x0000f000,
	0x000f0000, 0x00f00000, 0x0f000000, 0xf0000000
};

static int start_fields[] = {
	-1, 0, 2, 0, 15, 3, 0, 0, -1, 0,
	2, 0, 15, 3, 0, 0, 0, 0, 0
};

static int end_fields[] = {
	-1, -1, 2, 2, 15, 14, 1, 15, -1, -1,
	2, 2, 15, 14, 1, 4, 3, 2, 0
};

static inline int get_start(int code)
{
	int s;

	if ((s = start_fields[code]) == -1) {
		s = saturn.P;
	}
	return s;
}

static inline int get_end(int code)
{
	int e;

	if ((e = end_fields[code]) == -1) {
		e = saturn.P;
	}
	return e;
}

void load_addr(word_20 * dat, long addr, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		*dat &= ~nibble_masks[i];
		*dat |= read_nibble(addr + i) << (i * 4);
	}
}

void load_address(unsigned char *reg, long addr, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		reg[i] = read_nibble(addr + i);
	}
}

void register_to_address(unsigned char *reg, word_20 * dat, int s)
{
	int i, n;

	if (s)
		n = 4;
	else
		n = 5;
	for (i = 0; i < n; i++) {
		*dat &= ~nibble_masks[i];
		*dat |= (reg[i] & 0x0f) << (i * 4);
	}
}

void address_to_register(word_20 dat, unsigned char *reg, int s)
{
	int i, n;

	if (s)
		n = 4;
	else
		n = 5;
	for (i = 0; i < n; i++) {
		reg[i] = dat & 0x0f;
		dat >>= 4;
	}
}

long dat_to_addr(unsigned char *dat)
{
	int i;
	long addr;

	addr = 0;
	for (i = 4; i >= 0; i--) {
		addr <<= 4;
		addr |= (dat[i] & 0xf);
	}
	return addr;
}

void addr_to_dat(long addr, unsigned char *dat)
{
	int i;

	for (i = 0; i < 5; i++) {
		dat[i] = (addr & 0xf);
		addr >>= 4;
	}
}

void add_address(word_20 * dat, int add)
{
	*dat += add;
	if (*dat & (word_20) 0xfff00000) {
		saturn.CARRY = 1;
	} else {
		saturn.CARRY = 0;
	}
	*dat &= 0xfffff;
}

void store(word_20 dat, unsigned char *reg, int code)
{
	int i, s, e;

	s = get_start(code);
	e = get_end(code);
	for (i = s; i <= e; i++) {
		write_nibble(dat++, reg[i]);
	}
}

void store_n(word_20 dat, unsigned char *reg, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		write_nibble(dat++, reg[i]);
	}
}

void recall(unsigned char *reg, word_20 dat, int code)
{
	int i, s, e;

	s = get_start(code);
	e = get_end(code);
	for (i = s; i <= e; i++) {
		reg[i] = read_nibble_crc(dat++);
	}
}

void recall_n(unsigned char *reg, word_20 dat, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		reg[i] = read_nibble_crc(dat++);
	}
}

/* from register.c */

void add_register(unsigned char *res, unsigned char *r1, unsigned char *r2, int code)
{
	int t, c, i, s, e;

	s = get_start(code);
	e = get_end(code);
	c = 0;
	for (i = s; i <= e; i++) {
		t = r1[i] + r2[i] + c;
		if (t < (int)saturn.hexmode) {
			res[i] = t & 0xf;
			c = 0;
		} else {
			res[i] = (t - saturn.hexmode) & 0xf;
			c = 1;
		}
	}
	if (c)
		saturn.CARRY = 1;
	else
		saturn.CARRY = 0;
}

void add_p_plus_one(unsigned char *r)
{
	int t, c, i, s, e;

	s = 0;
	e = 4;
	c = saturn.P + 1;
	for (i = s; i <= e; i++) {
		t = r[i] + c;
		if (t < 16) {
			r[i] = t & 0xf;
			c = 0;
		} else {
			r[i] = (t - 16) & 0xf;
			c = 1;
		}
	}
	if (c)
		saturn.CARRY = 1;
	else
		saturn.CARRY = 0;
}

void sub_register(unsigned char *res, unsigned char *r1, unsigned char *r2, int code)
{
	int t, c, i, s, e;

	s = get_start(code);
	e = get_end(code);
	c = 0;
	for (i = s; i <= e; i++) {
		t = r1[i] - r2[i] - c;
		if (t >= 0) {
			res[i] = t & 0xf;
			c = 0;
		} else {
			res[i] = (t + saturn.hexmode) & 0xf;
			c = 1;
		}
	}
	if (c)
		saturn.CARRY = 1;
	else
		saturn.CARRY = 0;
}

void complement_2_register(unsigned char *r, int code)
{
	int t, c, carry, i, s, e;

	s = get_start(code);
	e = get_end(code);
	c = 1;
	carry = 0;
	for (i = s; i <= e; i++) {
		t = (saturn.hexmode - 1) - r[i] + c;
		if (t < (int)saturn.hexmode) {
			r[i] = t & 0xf;
			c = 0;
		} else {
			r[i] = (t - saturn.hexmode) & 0xf;
			c = 1;
		}
		carry += r[i];
	}
	if (carry)
		saturn.CARRY = 1;
	else
		saturn.CARRY = 0;
}

void complement_1_register(unsigned char *r, int code)
{
	int t, i, s, e;

	s = get_start(code);
	e = get_end(code);
	for (i = s; i <= e; i++) {
		t = (saturn.hexmode - 1) - r[i];
		r[i] = t & 0xf;
	}
	saturn.CARRY = 0;
}

void inc_register(unsigned char *r, int code)
{
	int t, c, i, s, e;

	s = get_start(code);
	e = get_end(code);
	c = 1;
	for (i = s; i <= e; i++) {
		t = r[i] + c;
		if (t < (int)saturn.hexmode) {
			r[i] = t & 0xf;
			c = 0;
			break;
		} else {
			r[i] = (t - saturn.hexmode) & 0xf;
			c = 1;
		}
	}
	if (c)
		saturn.CARRY = 1;
	else
		saturn.CARRY = 0;
}

void add_register_constant(unsigned char *r, int code, int val)
{
	int t, c, i, s, e;

	s = get_start(code);
	e = get_end(code);
	c = val;
	for (i = s; i <= e; i++) {
		t = r[i] + c;
		if (t < 16) {
			r[i] = t & 0xf;
			c = 0;
			break;
		} else {
			r[i] = (t - 16) & 0xf;
			c = 1;
		}
	}
	if (c)
		saturn.CARRY = 1;
	else
		saturn.CARRY = 0;
}

void dec_register(unsigned char *r, int code)
{
	int t, c, i, s, e;

	s = get_start(code);
	e = get_end(code);
	c = 1;
	for (i = s; i <= e; i++) {
		t = r[i] - c;
		if (t >= 0) {
			r[i] = t & 0xf;
			c = 0;
			break;
		} else {
			r[i] = (t + saturn.hexmode) & 0xf;
			c = 1;
		}
	}
	if (c)
		saturn.CARRY = 1;
	else
		saturn.CARRY = 0;
}

void sub_register_constant(unsigned char *r, int code, int val)
{
	int t, c, i, s, e;

	s = get_start(code);
	e = get_end(code);
	c = val;
	for (i = s; i <= e; i++) {
		t = r[i] - c;
		if (t >= 0) {
			r[i] = t & 0xf;
			c = 0;
			break;
		} else {
			r[i] = (t + 16) & 0xf;
			c = 1;
		}
	}
	if (c)
		saturn.CARRY = 1;
	else
		saturn.CARRY = 0;
}

void zero_register(unsigned char *r, int code)
{
	int i, s, e;

	s = get_start(code);
	e = get_end(code);
	for (i = s; i <= e; i++)
		r[i] = 0;
}

void or_register(unsigned char *res, unsigned char *r1, unsigned char *r2, int code)
{
	int i, s, e;

	s = get_start(code);
	e = get_end(code);
	for (i = s; i <= e; i++) {
		res[i] = (r1[i] | r2[i]) & 0xf;
	}
}

void and_register(unsigned char *res, unsigned char *r1, unsigned char *r2, int code)
{
	int i, s, e;

	s = get_start(code);
	e = get_end(code);
	for (i = s; i <= e; i++) {
		res[i] = (r1[i] & r2[i]) & 0xf;
	}
}

void copy_register(unsigned char *to, unsigned char *from, int code)
{
	int i, s, e;

	s = get_start(code);
	e = get_end(code);
	for (i = s; i <= e; i++)
		to[i] = from[i];
}

void exchange_register(unsigned char *r1, unsigned char *r2, int code)
{
	int t, i, s, e;

	s = get_start(code);
	e = get_end(code);
	for (i = s; i <= e; i++) {
		t = r1[i];
		r1[i] = r2[i];
		r2[i] = t;
	}
}

void exchange_reg(unsigned char *r, word_20 * d, int code)
{
	int t, i, s, e;

	s = get_start(code);
	e = get_end(code);
	for (i = s; i <= e; i++) {
		t = r[i];
		r[i] = (*d >> (i * 4)) & 0x0f;
		*d &= ~nibble_masks[i];
		*d |= t << (i * 4);
	}
}

void shift_left_register(unsigned char *r, int code)
{
	int i, s, e;

	s = get_start(code);
	e = get_end(code);
	for (i = e; i > s; i--) {
		r[i] = r[i - 1] & 0x0f;
	}
	r[s] = 0;
}

void shift_left_circ_register(unsigned char *r, int code)
{
	int t, i, s, e;

	s = get_start(code);
	e = get_end(code);
	t = r[e] & 0x0f;
	for (i = e; i > s; i--) {
		r[i] = r[i - 1] & 0x0f;
	}
	r[s] = t;
}

void shift_right_register(unsigned char *r, int code)
{
	int i, s, e;

	s = get_start(code);
	e = get_end(code);
	if (r[s] & 0x0f)
		saturn.SB = 1;
	for (i = s; i < e; i++) {
		r[i] = r[i + 1] & 0x0f;
	}
	r[e] = 0;
}

void shift_right_circ_register(unsigned char *r, int code)
{
	int t, i, s, e;

	s = get_start(code);
	e = get_end(code);
	t = r[s] & 0x0f;
	for (i = s; i < e; i++) {
		r[i] = r[i + 1] & 0x0f;
	}
	r[e] = t;
	if (t)
		saturn.SB = 1;
}

void shift_right_bit_register(unsigned char *r, int code)
{
	int t, i, s, e, sb;

	s = get_start(code);
	e = get_end(code);
	sb = 0;
	for (i = e; i >= s; i--) {
		t = (((r[i] >> 1) & 7) | (sb << 3)) & 0x0f;
		sb = r[i] & 1;
		r[i] = t;
	}
	if (sb)
		saturn.SB = 1;
}

int is_zero_register(unsigned char *r, int code)
{
	int z, i, s, e;

	s = get_start(code);
	e = get_end(code);
	z = 1;
	for (i = s; i <= e; i++)
		if ((r[i] & 0xf) != 0) {
			z = 0;
			break;
		}
	return z;
}

int is_not_zero_register(unsigned char *r, int code)
{
	int z, i, s, e;

	s = get_start(code);
	e = get_end(code);
	z = 0;
	for (i = s; i <= e; i++)
		if ((r[i] & 0xf) != 0) {
			z = 1;
			break;
		}
	return z;
}

int is_equal_register(unsigned char *r1, unsigned char *r2, int code)
{
	int z, i, s, e;

	s = get_start(code);
	e = get_end(code);
	z = 1;
	for (i = s; i <= e; i++)
		if ((r1[i] & 0xf) != (r2[i] & 0xf)) {
			z = 0;
			break;
		}
	return z;
}

int is_not_equal_register(unsigned char *r1, unsigned char *r2, int code)
{
	int z, i, s, e;

	s = get_start(code);
	e = get_end(code);
	z = 0;
	for (i = s; i <= e; i++)
		if ((r1[i] & 0xf) != (r2[i] & 0xf)) {
			z = 1;
			break;
		}
	return z;
}

int is_less_register(unsigned char *r1, unsigned char *r2, int code)
{
	int z, i, s, e;

	s = get_start(code);
	e = get_end(code);
	z = 0;
	for (i = e; i >= s; i--) {
		if ((int)(r1[i] & 0xf) < (int)(r2[i] & 0xf)) {
			z = 1;
			break;
		}
		if ((int)(r1[i] & 0xf) > (int)(r2[i] & 0xf)) {
			z = 0;
			break;
		}
	}
	return z;
}

int is_less_or_equal_register(unsigned char *r1, unsigned char *r2, int code)
{
	int z, i, s, e;

	s = get_start(code);
	e = get_end(code);
	z = 1;
	for (i = e; i >= s; i--) {
		if ((int)(r1[i] & 0xf) < (int)(r2[i] & 0xf)) {
			z = 1;
			break;
		}
		if ((int)(r1[i] & 0xf) > (int)(r2[i] & 0xf)) {
			z = 0;
			break;
		}
	}
	return z;
}

int is_greater_register(unsigned char *r1, unsigned char *r2, int code)
{
	int z, i, s, e;

	s = get_start(code);
	e = get_end(code);
	z = 0;
	for (i = e; i >= s; i--) {
		if ((int)(r1[i] & 0xf) > (int)(r2[i] & 0xf)) {
			z = 1;
			break;
		}
		if ((int)(r1[i] & 0xf) < (int)(r2[i] & 0xf)) {
			z = 0;
			break;
		}
	}
	return z;
}

int is_greater_or_equal_register(unsigned char *r1, unsigned char *r2, int code)
{
	int z, i, s, e;

	s = get_start(code);
	e = get_end(code);
	z = 1;
	for (i = e; i >= s; i--) {
		if ((int)(r1[i] & 0xf) < (int)(r2[i] & 0xf)) {
			z = 0;
			break;
		}
		if ((int)(r1[i] & 0xf) > (int)(r2[i] & 0xf)) {
			z = 1;
			break;
		}
	}
	return z;
}

/* from emulate.c */

static word_20 jumpmasks[] = {
	0xffffffff, 0xfffffff0, 0xffffff00, 0xfffff000,
	0xffff0000, 0xfff00000, 0xff000000, 0xf0000000
};

static long jumpaddr;

void do_in()
{
}

void do_unconfigure()
{
}

void do_configure()
{
}

int get_identification()
{
	return 0;
}

void do_shutdown()
{
}

void do_inton()
{
}

void do_reset_interrupt_system()
{
}

void do_intoff()
{
}

void do_reset()
{
}

int decode_group_0()
{
	int op1, op2, op3;

	op1 = read_nibble(saturn.PC + 1);
	switch (op1) {
	case 0:		/* RTNSXM */
		saturn.XM = 1;
		saturn.PC = pop_return_addr();
		break;
	case 1:		/* RTN */
		saturn.PC = pop_return_addr();
		break;
	case 2:		/* RTNSC */
		saturn.CARRY = 1;
		saturn.PC = pop_return_addr();
		break;
	case 3:		/* RTNCC */
		saturn.CARRY = 0;
		saturn.PC = pop_return_addr();
		break;
	case 4:		/* SETHEX */
		saturn.PC += 2;
		saturn.hexmode = HEX;
		break;
	case 5:		/* SETDEC */
		saturn.PC += 2;
		saturn.hexmode = DEC;
		break;
	case 6:		/* RSTK=C */
		jumpaddr = dat_to_addr(saturn.C);
		push_return_addr(jumpaddr);
		saturn.PC += 2;
		break;
	case 7:		/* C=RSTK */
		saturn.PC += 2;
		jumpaddr = pop_return_addr();
		addr_to_dat(jumpaddr, saturn.C);
		break;
	case 8:		/* CLRST */
		saturn.PC += 2;
		clear_status();
		break;
	case 9:		/* C=ST */
		saturn.PC += 2;
		status_to_register(saturn.C);
		break;
	case 0xa:		/* ST=C */
		saturn.PC += 2;
		register_to_status(saturn.C);
		break;
	case 0xb:		/* CSTEX */
		saturn.PC += 2;
		swap_register_status(saturn.C);
		break;
	case 0xc:		/* P=P+1 */
		saturn.PC += 2;
		if (saturn.P == 0xf) {
			saturn.P = 0;
			saturn.CARRY = 1;
		} else {
			saturn.P += 1;
			saturn.CARRY = 0;
		}
		break;
	case 0xd:		/* P=P-1 */
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
		case 0:	/* A=A&B */
			saturn.PC += 4;
			and_register(saturn.A, saturn.A, saturn.B, op2);
			break;
		case 1:	/* B=B&C */
			saturn.PC += 4;
			and_register(saturn.B, saturn.B, saturn.C, op2);
			break;
		case 2:	/* C=C&A */
			saturn.PC += 4;
			and_register(saturn.C, saturn.C, saturn.A, op2);
			break;
		case 3:	/* D=D&C */
			saturn.PC += 4;
			and_register(saturn.D, saturn.D, saturn.C, op2);
			break;
		case 4:	/* B=B&A */
			saturn.PC += 4;
			and_register(saturn.B, saturn.B, saturn.A, op2);
			break;
		case 5:	/* C=C&B */
			saturn.PC += 4;
			and_register(saturn.C, saturn.C, saturn.B, op2);
			break;
		case 6:	/* A=A&C */
			saturn.PC += 4;
			and_register(saturn.A, saturn.A, saturn.C, op2);
			break;
		case 7:	/* C=C&D */
			saturn.PC += 4;
			and_register(saturn.C, saturn.C, saturn.D, op2);
			break;
		case 8:	/* A=A!B */
			saturn.PC += 4;
			or_register(saturn.A, saturn.A, saturn.B, op2);
			break;
		case 9:	/* B=B!C */
			saturn.PC += 4;
			or_register(saturn.B, saturn.B, saturn.C, op2);
			break;
		case 0xa:	/* C=C!A */
			saturn.PC += 4;
			or_register(saturn.C, saturn.C, saturn.A, op2);
			break;
		case 0xb:	/* D=D!C */
			saturn.PC += 4;
			or_register(saturn.D, saturn.D, saturn.C, op2);
			break;
		case 0xc:	/* B=B!A */
			saturn.PC += 4;
			or_register(saturn.B, saturn.B, saturn.A, op2);
			break;
		case 0xd:	/* C=C!B */
			saturn.PC += 4;
			or_register(saturn.C, saturn.C, saturn.B, op2);
			break;
		case 0xe:	/* A=A!C */
			saturn.PC += 4;
			or_register(saturn.A, saturn.A, saturn.C, op2);
			break;
		case 0xf:	/* C=C!D */
			saturn.PC += 4;
			or_register(saturn.C, saturn.C, saturn.D, op2);
			break;
		default:
			return 1;
			break;
		}
		break;
	case 0xf:		/* RTI */
		do_return_interupt();
		break;
	default:
		return 1;
		break;
	}
	return 0;
}

int decode_group_1()
{
	int op, op2, op3, op4;

	op2 = read_nibble(saturn.PC + 1);
	switch (op2) {
	case 0:
		op3 = read_nibble(saturn.PC + 2);
		switch (op3) {
		case 0:	/* saturn.R0=A */
			saturn.PC += 3;
			copy_register(saturn.R0, saturn.A, W_FIELD);
			return 0;
		case 1:	/* saturn.R1=A */
		case 5:
			saturn.PC += 3;
			copy_register(saturn.R1, saturn.A, W_FIELD);
			return 0;
		case 2:	/* saturn.R2=A */
		case 6:
			saturn.PC += 3;
			copy_register(saturn.R2, saturn.A, W_FIELD);
			return 0;
		case 3:	/* saturn.R3=A */
		case 7:
			saturn.PC += 3;
			copy_register(saturn.R3, saturn.A, W_FIELD);
			return 0;
		case 4:	/* saturn.R4=A */
			saturn.PC += 3;
			copy_register(saturn.R4, saturn.A, W_FIELD);
			return 0;
		case 8:	/* saturn.R0=C */
			saturn.PC += 3;
			copy_register(saturn.R0, saturn.C, W_FIELD);
			return 0;
		case 9:	/* saturn.R1=C */
		case 0xd:
			saturn.PC += 3;
			copy_register(saturn.R1, saturn.C, W_FIELD);
			return 0;
		case 0xa:	/* saturn.R2=C */
		case 0xe:
			saturn.PC += 3;
			copy_register(saturn.R2, saturn.C, W_FIELD);
			return 0;
		case 0xb:	/* saturn.R3=C */
		case 0xf:
			saturn.PC += 3;
			copy_register(saturn.R3, saturn.C, W_FIELD);
			return 0;
		case 0xc:	/* saturn.R4=C */
			saturn.PC += 3;
			copy_register(saturn.R4, saturn.C, W_FIELD);
			return 0;
		default:
			return 1;
		}
	case 1:
		op3 = read_nibble(saturn.PC + 2);
		switch (op3) {
		case 0:	/* A=R0 */
			saturn.PC += 3;
			copy_register(saturn.A, saturn.R0, W_FIELD);
			return 0;
		case 1:	/* A=R1 */
		case 5:
			saturn.PC += 3;
			copy_register(saturn.A, saturn.R1, W_FIELD);
			return 0;
		case 2:	/* A=R2 */
		case 6:
			saturn.PC += 3;
			copy_register(saturn.A, saturn.R2, W_FIELD);
			return 0;
		case 3:	/* A=R3 */
		case 7:
			saturn.PC += 3;
			copy_register(saturn.A, saturn.R3, W_FIELD);
			return 0;
		case 4:	/* A=R4 */
			saturn.PC += 3;
			copy_register(saturn.A, saturn.R4, W_FIELD);
			return 0;
		case 8:	/* C=R0 */
			saturn.PC += 3;
			copy_register(saturn.C, saturn.R0, W_FIELD);
			return 0;
		case 9:	/* C=R1 */
		case 0xd:
			saturn.PC += 3;
			copy_register(saturn.C, saturn.R1, W_FIELD);
			return 0;
		case 0xa:	/* C=R2 */
		case 0xe:
			saturn.PC += 3;
			copy_register(saturn.C, saturn.R2, W_FIELD);
			return 0;
		case 0xb:	/* C=R3 */
		case 0xf:
			saturn.PC += 3;
			copy_register(saturn.C, saturn.R3, W_FIELD);
			return 0;
		case 0xc:	/* C=R4 */
			saturn.PC += 3;
			copy_register(saturn.C, saturn.R4, W_FIELD);
			return 0;
		default:
			return 1;
		}
	case 2:
		op3 = read_nibble(saturn.PC + 2);
		switch (op3) {
		case 0:	/* AR0EX */
			saturn.PC += 3;
			exchange_register(saturn.A, saturn.R0, W_FIELD);
			return 0;
		case 1:	/* AR1EX */
		case 5:
			saturn.PC += 3;
			exchange_register(saturn.A, saturn.R1, W_FIELD);
			return 0;
		case 2:	/* AR2EX */
		case 6:
			saturn.PC += 3;
			exchange_register(saturn.A, saturn.R2, W_FIELD);
			return 0;
		case 3:	/* AR3EX */
		case 7:
			saturn.PC += 3;
			exchange_register(saturn.A, saturn.R3, W_FIELD);
			return 0;
		case 4:	/* AR4EX */
			saturn.PC += 3;
			exchange_register(saturn.A, saturn.R4, W_FIELD);
			return 0;
		case 8:	/* CR0EX */
			saturn.PC += 3;
			exchange_register(saturn.C, saturn.R0, W_FIELD);
			return 0;
		case 9:	/* CR1EX */
		case 0xd:
			saturn.PC += 3;
			exchange_register(saturn.C, saturn.R1, W_FIELD);
			return 0;
		case 0xa:	/* CR2EX */
		case 0xe:
			saturn.PC += 3;
			exchange_register(saturn.C, saturn.R2, W_FIELD);
			return 0;
		case 0xb:	/* CR3EX */
		case 0xf:
			saturn.PC += 3;
			exchange_register(saturn.C, saturn.R3, W_FIELD);
			return 0;
		case 0xc:	/* CR4EX */
			saturn.PC += 3;
			exchange_register(saturn.C, saturn.R4, W_FIELD);
			return 0;
		default:
			return 1;
		}
	case 3:
		op3 = read_nibble(saturn.PC + 2);
		switch (op3) {
		case 0:	/* D0=A */
			saturn.PC += 3;
			register_to_address(saturn.A, &saturn.D0, 0);
			return 0;
		case 1:	/* D1=A */
			saturn.PC += 3;
			register_to_address(saturn.A, &saturn.D1, 0);
			return 0;
		case 2:	/* AD0EX */
			saturn.PC += 3;
			exchange_reg(saturn.A, &saturn.D0, A_FIELD);
			return 0;
		case 3:	/* AD1EX */
			saturn.PC += 3;
			exchange_reg(saturn.A, &saturn.D1, A_FIELD);
			return 0;
		case 4:	/* D0=C */
			saturn.PC += 3;
			register_to_address(saturn.C, &saturn.D0, 0);
			return 0;
		case 5:	/* D1=C */
			saturn.PC += 3;
			register_to_address(saturn.C, &saturn.D1, 0);
			return 0;
		case 6:	/* CD0EX */
			saturn.PC += 3;
			exchange_reg(saturn.C, &saturn.D0, A_FIELD);
			return 0;
		case 7:	/* CD1EX */
			saturn.PC += 3;
			exchange_reg(saturn.C, &saturn.D1, A_FIELD);
			return 0;
		case 8:	/* D0=AS */
			saturn.PC += 3;
			register_to_address(saturn.A, &saturn.D0, 1);
			return 0;
		case 9:	/* saturn.D1=AS */
			saturn.PC += 3;
			register_to_address(saturn.A, &saturn.D1, 1);
			return 0;
		case 0xa:	/* AD0XS */
			saturn.PC += 3;
			exchange_reg(saturn.A, &saturn.D0, IN_FIELD);
			return 0;
		case 0xb:	/* AD1XS */
			saturn.PC += 3;
			exchange_reg(saturn.A, &saturn.D1, IN_FIELD);
			return 0;
		case 0xc:	/* D0=CS */
			saturn.PC += 3;
			register_to_address(saturn.C, &saturn.D0, 1);
			return 0;
		case 0xd:	/* D1=CS */
			saturn.PC += 3;
			register_to_address(saturn.C, &saturn.D1, 1);
			return 0;
		case 0xe:	/* CD0XS */
			saturn.PC += 3;
			exchange_reg(saturn.C, &saturn.D0, IN_FIELD);
			return 0;
		case 0xf:	/* CD1XS */
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
		case 0:	/* DAT0=A */
			saturn.PC += 3;
			store(saturn.D0, saturn.A, op);
			return 0;
		case 1:	/* DAT1=A */
			saturn.PC += 3;
			store(saturn.D1, saturn.A, op);
			return 0;
		case 2:	/* A=DAT0 */
			saturn.PC += 3;
			recall(saturn.A, saturn.D0, op);
			return 0;
		case 3:	/* A=DAT1 */
			saturn.PC += 3;
			recall(saturn.A, saturn.D1, op);
			return 0;
		case 4:	/* DAT0=C */
			saturn.PC += 3;
			store(saturn.D0, saturn.C, op);
			return 0;
		case 5:	/* DAT1=C */
			saturn.PC += 3;
			store(saturn.D1, saturn.C, op);
			return 0;
		case 6:	/* C=DAT0 */
			saturn.PC += 3;
			recall(saturn.C, saturn.D0, op);
			return 0;
		case 7:	/* C=DAT1 */
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
			case 0:	/* DAT0=A */
				saturn.PC += 4;
				store_n(saturn.D0, saturn.A, op4 + 1);
				return 0;
			case 1:	/* DAT1=A */
				saturn.PC += 4;
				store_n(saturn.D1, saturn.A, op4 + 1);
				return 0;
			case 2:	/* A=DAT0 */
				saturn.PC += 4;
				recall_n(saturn.A, saturn.D0, op4 + 1);
				return 0;
			case 3:	/* A=DAT1 */
				saturn.PC += 4;
				recall_n(saturn.A, saturn.D1, op4 + 1);
				return 0;
			case 4:	/* DAT0=C */
				saturn.PC += 4;
				store_n(saturn.D0, saturn.C, op4 + 1);
				return 0;
			case 5:	/* DAT1=C */
				saturn.PC += 4;
				store_n(saturn.D1, saturn.C, op4 + 1);
				return 0;
			case 6:	/* C=DAT0 */
				saturn.PC += 4;
				recall_n(saturn.C, saturn.D0, op4 + 1);
				return 0;
			case 7:	/* C=DAT1 */
				saturn.PC += 4;
				recall_n(saturn.C, saturn.D1, op4 + 1);
				return 0;
			default:
				return 1;
			}
		} else {
			switch (op3) {
			case 0:	/* DAT0=A */
				saturn.PC += 4;
				store(saturn.D0, saturn.A, op4);
				return 0;
			case 1:	/* DAT1=A */
				saturn.PC += 4;
				store(saturn.D1, saturn.A, op4);
				return 0;
			case 2:	/* A=DAT0 */
				saturn.PC += 4;
				recall(saturn.A, saturn.D0, op4);
				return 0;
			case 3:	/* A=DAT1 */
				saturn.PC += 4;
				recall(saturn.A, saturn.D1, op4);
				return 0;
			case 4:	/* DAT0=C */
				saturn.PC += 4;
				store(saturn.D0, saturn.C, op4);
				return 0;
			case 5:	/* DAT1=C */
				saturn.PC += 4;
				store(saturn.D1, saturn.C, op4);
				return 0;
			case 6:	/* C=DAT0 */
				saturn.PC += 4;
				recall(saturn.C, saturn.D0, op4);
				return 0;
			case 7:	/* C=DAT1 */
				saturn.PC += 4;
				recall(saturn.C, saturn.D1, op4);
				return 0;
			default:
				return 1;
			}
		}
	case 6:
		op3 = read_nibble(saturn.PC + 2);
		saturn.PC += 3;
		add_address(&saturn.D0, op3 + 1);
		return 0;
	case 7:
		op3 = read_nibble(saturn.PC + 2);
		saturn.PC += 3;
		add_address(&saturn.D1, op3 + 1);
		return 0;
	case 8:
		op3 = read_nibble(saturn.PC + 2);
		saturn.PC += 3;
		add_address(&saturn.D0, -(op3 + 1));
		return 0;
	case 9:
		load_addr(&saturn.D0, saturn.PC + 2, 2);
		saturn.PC += 4;
		return 0;
	case 0xa:
		load_addr(&saturn.D0, saturn.PC + 2, 4);
		saturn.PC += 6;
		return 0;
	case 0xb:
		load_addr(&saturn.D0, saturn.PC + 2, 5);
		saturn.PC += 7;
		return 0;
	case 0xc:
		op3 = read_nibble(saturn.PC + 2);
		saturn.PC += 3;
		add_address(&saturn.D1, -(op3 + 1));
		return 0;
	case 0xd:
		load_addr(&saturn.D1, saturn.PC + 2, 2);
		saturn.PC += 4;
		return 0;
	case 0xe:
		load_addr(&saturn.D1, saturn.PC + 2, 4);
		saturn.PC += 6;
		return 0;
	case 0xf:
		load_addr(&saturn.D1, saturn.PC + 2, 5);
		saturn.PC += 7;
		return 0;
	default:
		return 1;
	}
}

int decode_group_80(void)
{
	int t, op3, op4, op5, op6;
	unsigned char *REG;
	long addr;
	op3 = read_nibble(saturn.PC + 2);
	switch (op3) {
	case 0:		/* OUT=CS */
		saturn.PC += 3;
		copy_register(saturn.OUT, saturn.C, OUTS_FIELD);
#if 0
		check_out_register();
#endif
		return 0;
	case 1:		/* OUT=C */
		saturn.PC += 3;
		copy_register(saturn.OUT, saturn.C, OUT_FIELD);
#if 0
		check_out_register();
#endif
		return 0;
	case 2:		/* A=IN */
		saturn.PC += 3;
		do_in();
		copy_register(saturn.A, saturn.IN, IN_FIELD);
		return 0;
	case 3:		/* C=IN */
		saturn.PC += 3;
		do_in();
		copy_register(saturn.C, saturn.IN, IN_FIELD);
		return 0;
	case 4:		/* UNCNFG */
		saturn.PC += 3;
		do_unconfigure();
		return 0;
	case 5:		/* CONFIG */
		saturn.PC += 3;
		do_configure();
		return 0;
	case 6:		/* C=ID */
		saturn.PC += 3;
		return get_identification();
	case 7:		/* SHUTDN */
		saturn.PC += 3;
		do_shutdown();
		return 0;
	case 8:
		op4 = read_nibble(saturn.PC + 3);
		switch (op4) {
		case 0:	/* INTON */
			saturn.PC += 4;
			do_inton();
			return 0;
		case 1:	/* RSI... */
			op5 = read_nibble(saturn.PC + 4);
			saturn.PC += 5;
			do_reset_interrupt_system();
			return 0;
		case 2:	/* LA... */
			op5 = read_nibble(saturn.PC + 4);
			load_constant(saturn.A, op5 + 1, saturn.PC + 5);
			saturn.PC += 6 + op5;
			return 0;
		case 3:	/* BUSCB */
			saturn.PC += 4;
			return 0;
		case 4:	/* ABIT=0 */
			op5 = read_nibble(saturn.PC + 4);
			saturn.PC += 5;
			clear_register_bit(saturn.A, op5);
			return 0;
		case 5:	/* ABIT=1 */
			op5 = read_nibble(saturn.PC + 4);
			saturn.PC += 5;
			set_register_bit(saturn.A, op5);
			return 0;
		case 8:	/* CBIT=0 */
			op5 = read_nibble(saturn.PC + 4);
			saturn.PC += 5;
			clear_register_bit(saturn.C, op5);
			return 0;
		case 9:	/* CBIT=1 */
			op5 = read_nibble(saturn.PC + 4);
			saturn.PC += 5;
			set_register_bit(saturn.C, op5);
			return 0;
		case 6:	/* ?ABIT=0 */
		case 7:	/* ?ABIT=1 */
		case 0xa:	/* ?CBIT=0 */
		case 0xb:	/* ?CBIT=1 */
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
		case 0xc:	/* PC=(A) */
			addr = dat_to_addr(saturn.A);
			jumpaddr = read_nibbles(addr, 5);
			saturn.PC = jumpaddr;
			return 0;
		case 0xd:	/* BUSCD */
			saturn.PC += 4;
			return 0;
		case 0xe:	/* PC=(C) */
			addr = dat_to_addr(saturn.C);
			jumpaddr = read_nibbles(addr, 5);
			saturn.PC = jumpaddr;
			return 0;
		case 0xf:	/* INTOFF */
			saturn.PC += 4;
			do_intoff();
			return 0;
		default:
			return 1;
		}
	case 9:		/* C+P+1 */
		saturn.PC += 3;
		add_p_plus_one(saturn.C);
		return 0;
	case 0xa:		/* RESET */
		saturn.PC += 3;
		do_reset();
		return 0;
	case 0xb:		/* BUSCC */
		saturn.PC += 3;
		return 0;
	case 0xc:		/* C=P n */
		op4 = read_nibble(saturn.PC + 3);
		saturn.PC += 4;
		set_register_nibble(saturn.C, op4, saturn.P);
		return 0;
	case 0xd:		/* P=C n */
		op4 = read_nibble(saturn.PC + 3);
		saturn.PC += 4;
		saturn.P = get_register_nibble(saturn.C, op4);
		return 0;
	case 0xe:		/* SREQ? */
		saturn.PC += 3;
		saturn.C[0] = 0;
		saturn.SR = 0;
		return 0;
	case 0xf:		/* CPEX n */
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

int decode_8_thru_f(int op1)
{
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
			case 0:	/* ASLC */
				saturn.PC += 3;
				shift_left_circ_register(saturn.A, W_FIELD);
				return 0;
			case 1:	/* BSLC */
				saturn.PC += 3;
				shift_left_circ_register(saturn.B, W_FIELD);
				return 0;
			case 2:	/* CSLC */
				saturn.PC += 3;
				shift_left_circ_register(saturn.C, W_FIELD);
				return 0;
			case 3:	/* DSLC */
				saturn.PC += 3;
				shift_left_circ_register(saturn.D, W_FIELD);
				return 0;
			case 4:	/* ASRC */
				saturn.PC += 3;
				shift_right_circ_register(saturn.A, W_FIELD);
				return 0;
			case 5:	/* BSRC */
				saturn.PC += 3;
				shift_right_circ_register(saturn.B, W_FIELD);
				return 0;
			case 6:	/* CSRC */
				saturn.PC += 3;
				shift_right_circ_register(saturn.C, W_FIELD);
				return 0;
			case 7:	/* DSRC */
				saturn.PC += 3;
				shift_right_circ_register(saturn.D, W_FIELD);
				return 0;
			case 8:	/* R = R +/- CON */
				op4 = read_nibble(saturn.PC + 3);
				op5 = read_nibble(saturn.PC + 4);
				op6 = read_nibble(saturn.PC + 5);
				if (op5 < 8) {	/* PLUS */
					switch (op5 & 3) {
					case 0:	/* A=A+CON */
						saturn.PC += 6;
						add_register_constant(saturn.A, op4, op6 + 1);
						return 0;
					case 1:	/* B=B+CON */
						saturn.PC += 6;
						add_register_constant(saturn.B, op4, op6 + 1);
						return 0;
					case 2:	/* C=C+CON */
						saturn.PC += 6;
						add_register_constant(saturn.C, op4, op6 + 1);
						return 0;
					case 3:	/* D=D+CON */
						saturn.PC += 6;
						add_register_constant(saturn.D, op4, op6 + 1);
						return 0;
					default:
						return 1;
					}
				} else {	/* MINUS */
					switch (op5 & 3) {
					case 0:	/* A=A-CON */
						saturn.PC += 6;
						sub_register_constant(saturn.A, op4, op6 + 1);
						return 0;
					case 1:	/* B=B-CON */
						saturn.PC += 6;
						sub_register_constant(saturn.B, op4, op6 + 1);
						return 0;
					case 2:	/* C=C-CON */
						saturn.PC += 6;
						sub_register_constant(saturn.C, op4, op6 + 1);
						return 0;
					case 3:	/* D=D-CON */
						saturn.PC += 6;
						sub_register_constant(saturn.D, op4, op6 + 1);
						return 0;
					default:
						return 1;
					}
				}
			case 9:	/* R SRB FIELD */
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
			case 0xa:	/* R = R FIELD, etc. */
				op4 = read_nibble(saturn.PC + 3);
				op5 = read_nibble(saturn.PC + 4);
				op6 = read_nibble(saturn.PC + 5);
				switch (op5) {
				case 0:
					switch (op6) {
					case 0:	/* saturn.R0=A */
						saturn.PC += 6;
						copy_register(saturn.R0, saturn.A, op4);
						return 0;
					case 1:	/* saturn.R1=A */
					case 5:
						saturn.PC += 6;
						copy_register(saturn.R1, saturn.A, op4);
						return 0;
					case 2:	/* saturn.R2=A */
					case 6:
						saturn.PC += 6;
						copy_register(saturn.R2, saturn.A, op4);
						return 0;
					case 3:	/* saturn.R3=A */
					case 7:
						saturn.PC += 6;
						copy_register(saturn.R3, saturn.A, op4);
						return 0;
					case 4:	/* saturn.R4=A */
						saturn.PC += 6;
						copy_register(saturn.R4, saturn.A, op4);
						return 0;
					case 8:	/* saturn.R0=C */
						saturn.PC += 6;
						copy_register(saturn.R0, saturn.C, op4);
						return 0;
					case 9:	/* saturn.R1=C */
					case 0xd:
						saturn.PC += 6;
						copy_register(saturn.R1, saturn.C, op4);
						return 0;
					case 0xa:	/* saturn.R2=C */
					case 0xe:
						saturn.PC += 6;
						copy_register(saturn.R2, saturn.C, op4);
						return 0;
					case 0xb:	/* saturn.R3=C */
					case 0xf:
						saturn.PC += 6;
						copy_register(saturn.R3, saturn.C, op4);
						return 0;
					case 0xc:	/* saturn.R4=C */
						saturn.PC += 6;
						copy_register(saturn.R4, saturn.C, op4);
						return 0;
					default:
						return 1;
					}
				case 1:
					switch (op6) {
					case 0:	/* A=R0 */
						saturn.PC += 6;
						copy_register(saturn.A, saturn.R0, op4);
						return 0;
					case 1:	/* A=R1 */
					case 5:
						saturn.PC += 6;
						copy_register(saturn.A, saturn.R1, op4);
						return 0;
					case 2:	/* A=R2 */
					case 6:
						saturn.PC += 6;
						copy_register(saturn.A, saturn.R2, op4);
						return 0;
					case 3:	/* A=R3 */
					case 7:
						saturn.PC += 6;
						copy_register(saturn.A, saturn.R3, op4);
						return 0;
					case 4:	/* A=R4 */
						saturn.PC += 6;
						copy_register(saturn.A, saturn.R4, op4);
						return 0;
					case 8:	/* C=R0 */
						saturn.PC += 6;
						copy_register(saturn.C, saturn.R0, op4);
						return 0;
					case 9:	/* C=R1 */
					case 0xd:
						saturn.PC += 6;
						copy_register(saturn.C, saturn.R1, op4);
						return 0;
					case 0xa:	/* C=R2 */
					case 0xe:
						saturn.PC += 6;
						copy_register(saturn.C, saturn.R2, op4);
						return 0;
					case 0xb:	/* C=R3 */
					case 0xf:
						saturn.PC += 6;
						copy_register(saturn.C, saturn.R3, op4);
						return 0;
					case 0xc:	/* C=R4 */
						saturn.PC += 6;
						copy_register(saturn.C, saturn.R4, op4);
						return 0;
					default:
						return 1;
					}
				case 2:
					switch (op6) {
					case 0:	/* AR0EX */
						saturn.PC += 6;
						exchange_register(saturn.A, saturn.R0, op4);
						return 0;
					case 1:	/* AR1EX */
					case 5:
						saturn.PC += 6;
						exchange_register(saturn.A, saturn.R1, op4);
						return 0;
					case 2:	/* AR2EX */
					case 6:
						saturn.PC += 6;
						exchange_register(saturn.A, saturn.R2, op4);
						return 0;
					case 3:	/* AR3EX */
					case 7:
						saturn.PC += 6;
						exchange_register(saturn.A, saturn.R3, op4);
						return 0;
					case 4:	/* AR4EX */
						saturn.PC += 6;
						exchange_register(saturn.A, saturn.R4, op4);
						return 0;
					case 8:	/* CR0EX */
						saturn.PC += 6;
						exchange_register(saturn.C, saturn.R0, op4);
						return 0;
					case 9:	/* CR1EX */
					case 0xd:
						saturn.PC += 6;
						exchange_register(saturn.C, saturn.R1, op4);
						return 0;
					case 0xa:	/* CR2EX */
					case 0xe:
						saturn.PC += 6;
						exchange_register(saturn.C, saturn.R2, op4);
						return 0;
					case 0xb:	/* CR3EX */
					case 0xf:
						saturn.PC += 6;
						exchange_register(saturn.C, saturn.R3, op4);
						return 0;
					case 0xc:	/* CR4EX */
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
				case 2:	/* PC=A */
					jumpaddr = dat_to_addr(saturn.A);
					saturn.PC = jumpaddr;
					return 0;
				case 3:	/* PC=C */
					jumpaddr = dat_to_addr(saturn.C);
					saturn.PC = jumpaddr;
					return 0;
				case 4:	/* A=PC */
					saturn.PC += 4;
					addr_to_dat(saturn.PC, saturn.A);
					return 0;
				case 5:	/* C=PC */
					saturn.PC += 4;
					addr_to_dat(saturn.PC, saturn.C);
					return 0;
				case 6:	/* APCEX */
					saturn.PC += 4;
					jumpaddr = dat_to_addr(saturn.A);
					addr_to_dat(saturn.PC, saturn.A);
					saturn.PC = jumpaddr;
					return 0;
				case 7:	/* CPCEX */
					saturn.PC += 4;
					jumpaddr = dat_to_addr(saturn.C);
					addr_to_dat(saturn.PC, saturn.C);
					saturn.PC = jumpaddr;
					return 0;
				default:
					return 1;
				}
			case 0xc:	/* ASRB */
				saturn.PC += 3;
				shift_right_bit_register(saturn.A, W_FIELD);
				return 0;
			case 0xd:	/* BSRB */
				saturn.PC += 3;
				shift_right_bit_register(saturn.B, W_FIELD);
				return 0;
			case 0xe:	/* CSRB */
				saturn.PC += 3;
				shift_right_bit_register(saturn.C, W_FIELD);
				return 0;
			case 0xf:	/* DSRB */
				saturn.PC += 3;
				shift_right_bit_register(saturn.D, W_FIELD);
				return 0;
			default:
				return 1;
			}
		case 2:
			op3 = read_nibble(saturn.PC + 2);
			saturn.PC += 3;
			clear_hardware_stat(op3);
			return 0;
		case 3:
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
			if (op2 == 4) {
				saturn.PC += 3;
				clear_program_stat(op3);
			} else {
				saturn.PC += 3;
				set_program_stat(op3);
			}
			return 0;
		case 6:
		case 7:
			op3 = read_nibble(saturn.PC + 2);
			if (op2 == 6)
				saturn.CARRY = (get_program_stat(op3) == 0) ? 1 : 0;
			else
				saturn.CARRY = (get_program_stat(op3) != 0) ? 1 : 0;
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
			if (op2 == 8)
				saturn.CARRY = (saturn.P != op3) ? 1 : 0;
			else
				saturn.CARRY = (saturn.P == op3) ? 1 : 0;
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
			case 0:	/* ?A=B */
				saturn.CARRY = is_equal_register(saturn.A, saturn.B, A_FIELD);
				break;
			case 1:	/* ?B=C */
				saturn.CARRY = is_equal_register(saturn.B, saturn.C, A_FIELD);
				break;
			case 2:	/* ?A=C */
				saturn.CARRY = is_equal_register(saturn.A, saturn.C, A_FIELD);
				break;
			case 3:	/* ?C=D */
				saturn.CARRY = is_equal_register(saturn.C, saturn.D, A_FIELD);
				break;
			case 4:	/* ?A#B */
				saturn.CARRY = is_not_equal_register(saturn.A, saturn.B, A_FIELD);
				break;
			case 5:	/* ?B#C */
				saturn.CARRY = is_not_equal_register(saturn.B, saturn.C, A_FIELD);
				break;
			case 6:	/* ?A#C */
				saturn.CARRY = is_not_equal_register(saturn.A, saturn.C, A_FIELD);
				break;
			case 7:	/* ?C#D */
				saturn.CARRY = is_not_equal_register(saturn.C, saturn.D, A_FIELD);
				break;
			case 8:	/* ?A=0 */
				saturn.CARRY = is_zero_register(saturn.A, A_FIELD);
				break;
			case 9:	/* ?B=0 */
				saturn.CARRY = is_zero_register(saturn.B, A_FIELD);
				break;
			case 0xa:	/* ?C=0 */
				saturn.CARRY = is_zero_register(saturn.C, A_FIELD);
				break;
			case 0xb:	/* ?D=0 */
				saturn.CARRY = is_zero_register(saturn.D, A_FIELD);
				break;
			case 0xc:	/* ?A#0 */
				saturn.CARRY = is_not_zero_register(saturn.A, A_FIELD);
				break;
			case 0xd:	/* ?B#0 */
				saturn.CARRY = is_not_zero_register(saturn.B, A_FIELD);
				break;
			case 0xe:	/* ?C#0 */
				saturn.CARRY = is_not_zero_register(saturn.C, A_FIELD);
				break;
			case 0xf:	/* ?D#0 */
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
			case 0:	/* ?A>B */
				saturn.CARRY = is_greater_register(saturn.A, saturn.B, A_FIELD);
				break;
			case 1:	/* ?B>C */
				saturn.CARRY = is_greater_register(saturn.B, saturn.C, A_FIELD);
				break;
			case 2:	/* ?C>A */
				saturn.CARRY = is_greater_register(saturn.C, saturn.A, A_FIELD);
				break;
			case 3:	/* ?D>C */
				saturn.CARRY = is_greater_register(saturn.D, saturn.C, A_FIELD);
				break;
			case 4:	/* ?A<B */
				saturn.CARRY = is_less_register(saturn.A, saturn.B, A_FIELD);
				break;
			case 5:	/* ?B<C */
				saturn.CARRY = is_less_register(saturn.B, saturn.C, A_FIELD);
				break;
			case 6:	/* ?C<A */
				saturn.CARRY = is_less_register(saturn.C, saturn.A, A_FIELD);
				break;
			case 7:	/* ?D<C */
				saturn.CARRY = is_less_register(saturn.D, saturn.C, A_FIELD);
				break;
			case 8:	/* ?A>=B */
				saturn.CARRY = is_greater_or_equal_register(saturn.A, saturn.B, A_FIELD);
				break;
			case 9:	/* ?B>=C */
				saturn.CARRY = is_greater_or_equal_register(saturn.B, saturn.C, A_FIELD);
				break;
			case 0xa:	/* ?C>=A */
				saturn.CARRY = is_greater_or_equal_register(saturn.C, saturn.A, A_FIELD);
				break;
			case 0xb:	/* ?D>=C */
				saturn.CARRY = is_greater_or_equal_register(saturn.D, saturn.C, A_FIELD);
				break;
			case 0xc:	/* ?A<=B */
				saturn.CARRY = is_less_or_equal_register(saturn.A, saturn.B, A_FIELD);
				break;
			case 0xd:	/* ?B<=C */
				saturn.CARRY = is_less_or_equal_register(saturn.B, saturn.C, A_FIELD);
				break;
			case 0xe:	/* ?C<=A */
				saturn.CARRY = is_less_or_equal_register(saturn.C, saturn.A, A_FIELD);
				break;
			case 0xf:	/* ?D<=C */
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
			case 0:	/* ?A=B */
				saturn.CARRY = is_equal_register(saturn.A, saturn.B, op2);
				break;
			case 1:	/* ?B=C */
				saturn.CARRY = is_equal_register(saturn.B, saturn.C, op2);
				break;
			case 2:	/* ?A=C */
				saturn.CARRY = is_equal_register(saturn.A, saturn.C, op2);
				break;
			case 3:	/* ?C=D */
				saturn.CARRY = is_equal_register(saturn.C, saturn.D, op2);
				break;
			case 4:	/* ?A#B */
				saturn.CARRY = is_not_equal_register(saturn.A, saturn.B, op2);
				break;
			case 5:	/* ?B#C */
				saturn.CARRY = is_not_equal_register(saturn.B, saturn.C, op2);
				break;
			case 6:	/* ?A#C */
				saturn.CARRY = is_not_equal_register(saturn.A, saturn.C, op2);
				break;
			case 7:	/* ?C#D */
				saturn.CARRY = is_not_equal_register(saturn.C, saturn.D, op2);
				break;
			case 8:	/* ?A=0 */
				saturn.CARRY = is_zero_register(saturn.A, op2);
				break;
			case 9:	/* ?B=0 */
				saturn.CARRY = is_zero_register(saturn.B, op2);
				break;
			case 0xa:	/* ?C=0 */
				saturn.CARRY = is_zero_register(saturn.C, op2);
				break;
			case 0xb:	/* ?D=0 */
				saturn.CARRY = is_zero_register(saturn.D, op2);
				break;
			case 0xc:	/* ?A#0 */
				saturn.CARRY = is_not_zero_register(saturn.A, op2);
				break;
			case 0xd:	/* ?B#0 */
				saturn.CARRY = is_not_zero_register(saturn.B, op2);
				break;
			case 0xe:	/* ?C#0 */
				saturn.CARRY = is_not_zero_register(saturn.C, op2);
				break;
			case 0xf:	/* ?D#0 */
				saturn.CARRY = is_not_zero_register(saturn.D, op2);
				break;
			default:
				return 1;
			}
		} else {
			op2 &= 7;
			switch (op3) {
			case 0:	/* ?A>B */
				saturn.CARRY = is_greater_register(saturn.A, saturn.B, op2);
				break;
			case 1:	/* ?B>C */
				saturn.CARRY = is_greater_register(saturn.B, saturn.C, op2);
				break;
			case 2:	/* ?C>A */
				saturn.CARRY = is_greater_register(saturn.C, saturn.A, op2);
				break;
			case 3:	/* ?D>C */
				saturn.CARRY = is_greater_register(saturn.D, saturn.C, op2);
				break;
			case 4:	/* ?A<B */
				saturn.CARRY = is_less_register(saturn.A, saturn.B, op2);
				break;
			case 5:	/* ?B<C */
				saturn.CARRY = is_less_register(saturn.B, saturn.C, op2);
				break;
			case 6:	/* ?C<A */
				saturn.CARRY = is_less_register(saturn.C, saturn.A, op2);
				break;
			case 7:	/* ?D<C */
				saturn.CARRY = is_less_register(saturn.D, saturn.C, op2);
				break;
			case 8:	/* ?A>=B */
				saturn.CARRY = is_greater_or_equal_register(saturn.A, saturn.B, op2);
				break;
			case 9:	/* ?B>=C */
				saturn.CARRY = is_greater_or_equal_register(saturn.B, saturn.C, op2);
				break;
			case 0xa:	/* ?C>=A */
				saturn.CARRY = is_greater_or_equal_register(saturn.C, saturn.A, op2);
				break;
			case 0xb:	/* ?D>=C */
				saturn.CARRY = is_greater_or_equal_register(saturn.D, saturn.C, op2);
				break;
			case 0xc:	/* ?A<=B */
				saturn.CARRY = is_less_or_equal_register(saturn.A, saturn.B, op2);
				break;
			case 0xd:	/* ?B<=C */
				saturn.CARRY = is_less_or_equal_register(saturn.B, saturn.C, op2);
				break;
			case 0xe:	/* ?C<=A */
				saturn.CARRY = is_less_or_equal_register(saturn.C, saturn.A, op2);
				break;
			case 0xf:	/* ?D<=C */
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
			case 0:	/* A=A+B */
				saturn.PC += 3;
				add_register(saturn.A, saturn.A, saturn.B, op2);
				return 0;
			case 1:	/* B=B+C */
				saturn.PC += 3;
				add_register(saturn.B, saturn.B, saturn.C, op2);
				return 0;
			case 2:	/* C=C+A */
				saturn.PC += 3;
				add_register(saturn.C, saturn.C, saturn.A, op2);
				return 0;
			case 3:	/* D=D+C */
				saturn.PC += 3;
				add_register(saturn.D, saturn.D, saturn.C, op2);
				return 0;
			case 4:	/* A=A+A */
				saturn.PC += 3;
				add_register(saturn.A, saturn.A, saturn.A, op2);
				return 0;
			case 5:	/* B=B+B */
				saturn.PC += 3;
				add_register(saturn.B, saturn.B, saturn.B, op2);
				return 0;
			case 6:	/* C=C+C */
				saturn.PC += 3;
				add_register(saturn.C, saturn.C, saturn.C, op2);
				return 0;
			case 7:	/* D=D+D */
				saturn.PC += 3;
				add_register(saturn.D, saturn.D, saturn.D, op2);
				return 0;
			case 8:	/* B=B+A */
				saturn.PC += 3;
				add_register(saturn.B, saturn.B, saturn.A, op2);
				return 0;
			case 9:	/* C=C+B */
				saturn.PC += 3;
				add_register(saturn.C, saturn.C, saturn.B, op2);
				return 0;
			case 0xa:	/* A=A+C */
				saturn.PC += 3;
				add_register(saturn.A, saturn.A, saturn.C, op2);
				return 0;
			case 0xb:	/* C=C+D */
				saturn.PC += 3;
				add_register(saturn.C, saturn.C, saturn.D, op2);
				return 0;
			case 0xc:	/* A=A-1 */
				saturn.PC += 3;
				dec_register(saturn.A, op2);
				return 0;
			case 0xd:	/* B=B-1 */
				saturn.PC += 3;
				dec_register(saturn.B, op2);
				return 0;
			case 0xe:	/* C=C-1 */
				saturn.PC += 3;
				dec_register(saturn.C, op2);
				return 0;
			case 0xf:	/* D=D-1 */
				saturn.PC += 3;
				dec_register(saturn.D, op2);
				return 0;
			default:
				return 1;
			}
		} else {
			op2 &= 7;
			switch (op3) {
			case 0:	/* A=0 */
				saturn.PC += 3;
				zero_register(saturn.A, op2);
				return 0;
			case 1:	/* B=0 */
				saturn.PC += 3;
				zero_register(saturn.B, op2);
				return 0;
			case 2:	/* C=0 */
				saturn.PC += 3;
				zero_register(saturn.C, op2);
				return 0;
			case 3:	/* D=0 */
				saturn.PC += 3;
				zero_register(saturn.D, op2);
				return 0;
			case 4:	/* A=B */
				saturn.PC += 3;
				copy_register(saturn.A, saturn.B, op2);
				return 0;
			case 5:	/* B=C */
				saturn.PC += 3;
				copy_register(saturn.B, saturn.C, op2);
				return 0;
			case 6:	/* C=A */
				saturn.PC += 3;
				copy_register(saturn.C, saturn.A, op2);
				return 0;
			case 7:	/* D=C */
				saturn.PC += 3;
				copy_register(saturn.D, saturn.C, op2);
				return 0;
			case 8:	/* B=A */
				saturn.PC += 3;
				copy_register(saturn.B, saturn.A, op2);
				return 0;
			case 9:	/* C=B */
				saturn.PC += 3;
				copy_register(saturn.C, saturn.B, op2);
				return 0;
			case 0xa:	/* A=C */
				saturn.PC += 3;
				copy_register(saturn.A, saturn.C, op2);
				return 0;
			case 0xb:	/* C=D */
				saturn.PC += 3;
				copy_register(saturn.C, saturn.D, op2);
				return 0;
			case 0xc:	/* ABEX */
				saturn.PC += 3;
				exchange_register(saturn.A, saturn.B, op2);
				return 0;
			case 0xd:	/* BCEX */
				saturn.PC += 3;
				exchange_register(saturn.B, saturn.C, op2);
				return 0;
			case 0xe:	/* ACEX */
				saturn.PC += 3;
				exchange_register(saturn.A, saturn.C, op2);
				return 0;
			case 0xf:	/* CDEX */
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
			case 0:	/* A=A-B */
				saturn.PC += 3;
				sub_register(saturn.A, saturn.A, saturn.B, op2);
				return 0;
			case 1:	/* B=B-C */
				saturn.PC += 3;
				sub_register(saturn.B, saturn.B, saturn.C, op2);
				return 0;
			case 2:	/* C=C-A */
				saturn.PC += 3;
				sub_register(saturn.C, saturn.C, saturn.A, op2);
				return 0;
			case 3:	/* D=D-C */
				saturn.PC += 3;
				sub_register(saturn.D, saturn.D, saturn.C, op2);
				return 0;
			case 4:	/* A=A+1 */
				saturn.PC += 3;
				inc_register(saturn.A, op2);
				return 0;
			case 5:	/* B=B+1 */
				saturn.PC += 3;
				inc_register(saturn.B, op2);
				return 0;
			case 6:	/* C=C+1 */
				saturn.PC += 3;
				inc_register(saturn.C, op2);
				return 0;
			case 7:	/* D=D+1 */
				saturn.PC += 3;
				inc_register(saturn.D, op2);
				return 0;
			case 8:	/* B=B-A */
				saturn.PC += 3;
				sub_register(saturn.B, saturn.B, saturn.A, op2);
				return 0;
			case 9:	/* C=C-B */
				saturn.PC += 3;
				sub_register(saturn.C, saturn.C, saturn.B, op2);
				return 0;
			case 0xa:	/* A=A-C */
				saturn.PC += 3;
				sub_register(saturn.A, saturn.A, saturn.C, op2);
				return 0;
			case 0xb:	/* C=C-D */
				saturn.PC += 3;
				sub_register(saturn.C, saturn.C, saturn.D, op2);
				return 0;
			case 0xc:	/* A=B-A */
				saturn.PC += 3;
				sub_register(saturn.A, saturn.B, saturn.A, op2);
				return 0;
			case 0xd:	/* B=C-B */
				saturn.PC += 3;
				sub_register(saturn.B, saturn.C, saturn.B, op2);
				return 0;
			case 0xe:	/* C=A-C */
				saturn.PC += 3;
				sub_register(saturn.C, saturn.A, saturn.C, op2);
				return 0;
			case 0xf:	/* D=C-D */
				saturn.PC += 3;
				sub_register(saturn.D, saturn.C, saturn.D, op2);
				return 0;
			default:
				return 1;
			}
		} else {
			op2 &= 7;
			switch (op3) {
			case 0:	/* ASL */
				saturn.PC += 3;
				shift_left_register(saturn.A, op2);
				return 0;
			case 1:	/* BSL */
				saturn.PC += 3;
				shift_left_register(saturn.B, op2);
				return 0;
			case 2:	/* CSL */
				saturn.PC += 3;
				shift_left_register(saturn.C, op2);
				return 0;
			case 3:	/* DSL */
				saturn.PC += 3;
				shift_left_register(saturn.D, op2);
				return 0;
			case 4:	/* ASR */
				saturn.PC += 3;
				shift_right_register(saturn.A, op2);
				return 0;
			case 5:	/* BSR */
				saturn.PC += 3;
				shift_right_register(saturn.B, op2);
				return 0;
			case 6:	/* CSR */
				saturn.PC += 3;
				shift_right_register(saturn.C, op2);
				return 0;
			case 7:	/* DSR */
				saturn.PC += 3;
				shift_right_register(saturn.D, op2);
				return 0;
			case 8:	/* A=-A */
				saturn.PC += 3;
				complement_2_register(saturn.A, op2);
				return 0;
			case 9:	/* B=-B */
				saturn.PC += 3;
				complement_2_register(saturn.B, op2);
				return 0;
			case 0xa:	/* C=-C */
				saturn.PC += 3;
				complement_2_register(saturn.C, op2);
				return 0;
			case 0xb:	/* D=-D */
				saturn.PC += 3;
				complement_2_register(saturn.D, op2);
				return 0;
			case 0xc:	/* A=-A-1 */
				saturn.PC += 3;
				complement_1_register(saturn.A, op2);
				return 0;
			case 0xd:	/* B=-B-1 */
				saturn.PC += 3;
				complement_1_register(saturn.B, op2);
				return 0;
			case 0xe:	/* C=-C-1 */
				saturn.PC += 3;
				complement_1_register(saturn.C, op2);
				return 0;
			case 0xf:	/* D=-D-1 */
				saturn.PC += 3;
				complement_1_register(saturn.D, op2);
				return 0;
			default:
				return 1;
			}
		}
	case 0xc:
		switch (op2) {
		case 0:	/* A=A+B */
			saturn.PC += 2;
			add_register(saturn.A, saturn.A, saturn.B, A_FIELD);
			return 0;
		case 1:	/* B=B+C */
			saturn.PC += 2;
			add_register(saturn.B, saturn.B, saturn.C, A_FIELD);
			return 0;
		case 2:	/* C=C+A */
			saturn.PC += 2;
			add_register(saturn.C, saturn.C, saturn.A, A_FIELD);
			return 0;
		case 3:	/* D=D+C */
			saturn.PC += 2;
			add_register(saturn.D, saturn.D, saturn.C, A_FIELD);
			return 0;
		case 4:	/* A=A+A */
			saturn.PC += 2;
			add_register(saturn.A, saturn.A, saturn.A, A_FIELD);
			return 0;
		case 5:	/* B=B+B */
			saturn.PC += 2;
			add_register(saturn.B, saturn.B, saturn.B, A_FIELD);
			return 0;
		case 6:	/* C=C+C */
			saturn.PC += 2;
			add_register(saturn.C, saturn.C, saturn.C, A_FIELD);
			return 0;
		case 7:	/* D=D+D */
			saturn.PC += 2;
			add_register(saturn.D, saturn.D, saturn.D, A_FIELD);
			return 0;
		case 8:	/* B=B+A */
			saturn.PC += 2;
			add_register(saturn.B, saturn.B, saturn.A, A_FIELD);
			return 0;
		case 9:	/* C=C+B */
			saturn.PC += 2;
			add_register(saturn.C, saturn.C, saturn.B, A_FIELD);
			return 0;
		case 0xa:	/* A=A+C */
			saturn.PC += 2;
			add_register(saturn.A, saturn.A, saturn.C, A_FIELD);
			return 0;
		case 0xb:	/* C=C+D */
			saturn.PC += 2;
			add_register(saturn.C, saturn.C, saturn.D, A_FIELD);
			return 0;
		case 0xc:	/* A=A-1 */
			saturn.PC += 2;
			dec_register(saturn.A, A_FIELD);
			return 0;
		case 0xd:	/* B=B-1 */
			saturn.PC += 2;
			dec_register(saturn.B, A_FIELD);
			return 0;
		case 0xe:	/* C=C-1 */
			saturn.PC += 2;
			dec_register(saturn.C, A_FIELD);
			return 0;
		case 0xf:	/* D=D-1 */
			saturn.PC += 2;
			dec_register(saturn.D, A_FIELD);
			return 0;
		default:
			return 1;
		}
	case 0xd:
		switch (op2) {
		case 0:	/* A=0 */
			saturn.PC += 2;
			zero_register(saturn.A, A_FIELD);
			return 0;
		case 1:	/* B=0 */
			saturn.PC += 2;
			zero_register(saturn.B, A_FIELD);
			return 0;
		case 2:	/* C=0 */
			saturn.PC += 2;
			zero_register(saturn.C, A_FIELD);
			return 0;
		case 3:	/* D=0 */
			saturn.PC += 2;
			zero_register(saturn.D, A_FIELD);
			return 0;
		case 4:	/* A=B */
			saturn.PC += 2;
			copy_register(saturn.A, saturn.B, A_FIELD);
			return 0;
		case 5:	/* B=C */
			saturn.PC += 2;
			copy_register(saturn.B, saturn.C, A_FIELD);
			return 0;
		case 6:	/* C=A */
			saturn.PC += 2;
			copy_register(saturn.C, saturn.A, A_FIELD);
			return 0;
		case 7:	/* D=C */
			saturn.PC += 2;
			copy_register(saturn.D, saturn.C, A_FIELD);
			return 0;
		case 8:	/* B=A */
			saturn.PC += 2;
			copy_register(saturn.B, saturn.A, A_FIELD);
			return 0;
		case 9:	/* C=B */
			saturn.PC += 2;
			copy_register(saturn.C, saturn.B, A_FIELD);
			return 0;
		case 0xa:	/* A=C */
			saturn.PC += 2;
			copy_register(saturn.A, saturn.C, A_FIELD);
			return 0;
		case 0xb:	/* C=D */
			saturn.PC += 2;
			copy_register(saturn.C, saturn.D, A_FIELD);
			return 0;
		case 0xc:	/* ABEX */
			saturn.PC += 2;
			exchange_register(saturn.A, saturn.B, A_FIELD);
			return 0;
		case 0xd:	/* BCEX */
			saturn.PC += 2;
			exchange_register(saturn.B, saturn.C, A_FIELD);
			return 0;
		case 0xe:	/* ACEX */
			saturn.PC += 2;
			exchange_register(saturn.A, saturn.C, A_FIELD);
			return 0;
		case 0xf:	/* CDEX */
			saturn.PC += 2;
			exchange_register(saturn.C, saturn.D, A_FIELD);
			return 0;
		default:
			return 1;
		}
	case 0xe:
		switch (op2) {
		case 0:	/* A=A-B */
			saturn.PC += 2;
			sub_register(saturn.A, saturn.A, saturn.B, A_FIELD);
			return 0;
		case 1:	/* B=B-C */
			saturn.PC += 2;
			sub_register(saturn.B, saturn.B, saturn.C, A_FIELD);
			return 0;
		case 2:	/* C=C-A */
			saturn.PC += 2;
			sub_register(saturn.C, saturn.C, saturn.A, A_FIELD);
			return 0;
		case 3:	/* D=D-C */
			saturn.PC += 2;
			sub_register(saturn.D, saturn.D, saturn.C, A_FIELD);
			return 0;
		case 4:	/* A=A+1 */
			saturn.PC += 2;
			inc_register(saturn.A, A_FIELD);
			return 0;
		case 5:	/* B=B+1 */
			saturn.PC += 2;
			inc_register(saturn.B, A_FIELD);
			return 0;
		case 6:	/* C=C+1 */
			saturn.PC += 2;
			inc_register(saturn.C, A_FIELD);
			return 0;
		case 7:	/* D=D+1 */
			saturn.PC += 2;
			inc_register(saturn.D, A_FIELD);
			return 0;
		case 8:	/* B=B-A */
			saturn.PC += 2;
			sub_register(saturn.B, saturn.B, saturn.A, A_FIELD);
			return 0;
		case 9:	/* C=C-B */
			saturn.PC += 2;
			sub_register(saturn.C, saturn.C, saturn.B, A_FIELD);
			return 0;
		case 0xa:	/* A=A-C */
			saturn.PC += 2;
			sub_register(saturn.A, saturn.A, saturn.C, A_FIELD);
			return 0;
		case 0xb:	/* C=C-D */
			saturn.PC += 2;
			sub_register(saturn.C, saturn.C, saturn.D, A_FIELD);
			return 0;
		case 0xc:	/* A=B-A */
			saturn.PC += 2;
			sub_register(saturn.A, saturn.B, saturn.A, A_FIELD);
			return 0;
		case 0xd:	/* B=C-B */
			saturn.PC += 2;
			sub_register(saturn.B, saturn.C, saturn.B, A_FIELD);
			return 0;
		case 0xe:	/* C=A-C */
			saturn.PC += 2;
			sub_register(saturn.C, saturn.A, saturn.C, A_FIELD);
			return 0;
		case 0xf:	/* D=C-D */
			saturn.PC += 2;
			sub_register(saturn.D, saturn.C, saturn.D, A_FIELD);
			return 0;
		default:
			return 1;
		}
	case 0xf:
		switch (op2) {
		case 0:	/* ASL */
			saturn.PC += 2;
			shift_left_register(saturn.A, A_FIELD);
			return 0;
		case 1:	/* BSL */
			saturn.PC += 2;
			shift_left_register(saturn.B, A_FIELD);
			return 0;
		case 2:	/* CSL */
			saturn.PC += 2;
			shift_left_register(saturn.C, A_FIELD);
			return 0;
		case 3:	/* DSL */
			saturn.PC += 2;
			shift_left_register(saturn.D, A_FIELD);
			return 0;
		case 4:	/* ASR */
			saturn.PC += 2;
			shift_right_register(saturn.A, A_FIELD);
			return 0;
		case 5:	/* BSR */
			saturn.PC += 2;
			shift_right_register(saturn.B, A_FIELD);
			return 0;
		case 6:	/* CSR */
			saturn.PC += 2;
			shift_right_register(saturn.C, A_FIELD);
			return 0;
		case 7:	/* DSR */
			saturn.PC += 2;
			shift_right_register(saturn.D, A_FIELD);
			return 0;
		case 8:	/* A=-A */
			saturn.PC += 2;
			complement_2_register(saturn.A, A_FIELD);
			return 0;
		case 9:	/* B=-B */
			saturn.PC += 2;
			complement_2_register(saturn.B, A_FIELD);
			return 0;
		case 0xa:	/* C=-C */
			saturn.PC += 2;
			complement_2_register(saturn.C, A_FIELD);
			return 0;
		case 0xb:	/* D=-D */
			saturn.PC += 2;
			complement_2_register(saturn.D, A_FIELD);
			return 0;
		case 0xc:	/* A=-A-1 */
			saturn.PC += 2;
			complement_1_register(saturn.A, A_FIELD);
			return 0;
		case 0xd:	/* B=-B-1 */
			saturn.PC += 2;
			complement_1_register(saturn.B, A_FIELD);
			return 0;
		case 0xe:	/* C=-C-1 */
			saturn.PC += 2;
			complement_1_register(saturn.C, A_FIELD);
			return 0;
		case 0xf:	/* D=-D-1 */
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

int step_instruction()
{
	int op0, op2, op3;
	int stop = 0;

	jumpaddr = 0;

	op0 = read_nibble(saturn.PC);
	switch (op0) {
	case 0:
		stop = decode_group_0();
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
//                              enter_debugger |= TRAP_INSTRUCTION;
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
