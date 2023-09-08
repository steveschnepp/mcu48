#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "cpu.h"
#include "instruction.h"
#include "roms/hp48_rom.h"

saturn_t saturn;

static unsigned char hp48_ram[32 * 1024 * 2];

static char nibbles_trace[10];
static int  nibbles_traced;
void trace_nibble_rom(int nibble) {
    assert(nibble < 0x10);
    assert(nibbles_traced < sizeof(nibbles_trace)-1);
    char nibble_as_char = (nibble < 10) ? '0' + nibble : 'A' + nibble - 10;
    nibbles_trace[nibbles_traced] = nibble_as_char;
    nibbles_traced ++;
}

int main() {
    saturn.ram = hp48_ram;
    saturn.rom = hp48_rom;

    while(1) {
        word_20 pc = saturn.PC;

        step_instruction();

        printf("%05lx: %s\n", pc, nibbles_trace);
        memset(nibbles_trace, ' ', sizeof(nibbles_trace)-1);
        nibbles_traced = 0;
    }
}

void write_dev_mem(long addr, int val)
{
    printf("write_dev_mem(%lx)\n", addr);
    assert(0);
}

int read_dev_mem(long addr)
{
    printf("read_dev_mem(%lx)\n", addr);
	assert(0);
    return 0x00;
}
