#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "cpu.h"
#include "instruction.h"
#include "roms/hp48_rom.h"

saturn_t saturn;

static unsigned char hp48_ram[32 * 1024 * 2];

static char nibbles_trace[32];
static int  nibbles_traced;
void trace_nibble_rom(int nibble) {
    assert(nibble < 0x10);
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

        printf("%05lx: %s \n", pc, nibbles_trace);
        memset(nibbles_trace, 0, sizeof(nibbles_trace));
        nibbles_traced = 0;
    }
}
