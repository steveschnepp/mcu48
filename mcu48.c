#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "cpu.h"
#include "instruction.h"
#include "roms/hp48_rom.h"

saturn_t saturn;

static unsigned char hp48_ram[32 * 1024 * 2];

char ntoa(int nibble) {
    char nibble_as_char = (nibble < 10) ? '0' + nibble : 'A' + nibble - 10;
    return nibble_as_char;
}

static char nibbles_trace[10];
static int  nibbles_traced;
void trace_nibble_rom(int nibble) {
    assert(nibble < 0x10);
    assert(nibbles_traced < sizeof(nibbles_trace)-1);
    char nibble_as_char = ntoa(nibble);
    nibbles_trace[nibbles_traced] = nibble_as_char;
    nibbles_traced ++;
}

uint64_t get_register_value(unsigned char *r, int size)
{
    long value = 0;
    for (int i = 0; i < size; i ++) {
        long nibble = r[i] & 0x0F;
        long nibble_value = nibble << (i * 4);
        value |= nibble_value;
    }
    return value;
}

uint16_t get_status_value(unsigned char *r)
{
    long value = 0;
    for (int i = 0; i < 16; i ++) {
        long bit = r[i] & 0x01;
        long bit_value = bit << i;
        value |= bit_value;
    }
    return value;
}

void dump_cpu_state() {
    printf("\n");

    printf("A   %016" PRIx64 "    RSTK0: %05lx\n", get_register_value(saturn.A, sizeof(saturn.A)), saturn.rstk[0]);
    printf("B   %016" PRIx64 "    RSTK1: %05lx\n", get_register_value(saturn.B, sizeof(saturn.B)), saturn.rstk[1]);
    printf("C   %016" PRIx64 "    RSTK2: %05lx\n", get_register_value(saturn.C, sizeof(saturn.C)), saturn.rstk[2]);
    printf("D   %016" PRIx64 "    RSTK3: %05lx\n", get_register_value(saturn.D, sizeof(saturn.D)), saturn.rstk[3]);
    printf("                        RSTK4: %05lx\n", /*                                           */ saturn.rstk[4]);
    printf("R0  %016" PRIx64 "    RSTK5: %05lx\n", get_register_value(saturn.R0, sizeof(saturn.R0)), saturn.rstk[5]);
    printf("R1  %016" PRIx64 "    RSTK6: %05lx\n", get_register_value(saturn.R1, sizeof(saturn.R1)), saturn.rstk[6]);
    printf("R2  %016" PRIx64 "    RSTK7: %05lx\n", get_register_value(saturn.R2, sizeof(saturn.R2)), saturn.rstk[7]);
    printf("R3  %016" PRIx64 "       D0: %05lx\n", get_register_value(saturn.R3, sizeof(saturn.R3)), saturn.D0);
    printf("R4  %016" PRIx64 "       D1: %05lx\n", get_register_value(saturn.R4, sizeof(saturn.R4)), saturn.D1);
    printf("\n");
    printf("IN  %04" PRIx64  "    ST: %04x        P: %x\n", get_register_value(saturn.IN, sizeof(saturn.IN)), get_status_value(saturn.PSTAT), saturn.P);
    printf("OUT %03" PRIx64 "    HST: %d CARRY: %d PC: %05lx\n", get_register_value(saturn.OUT, sizeof(saturn.OUT)), 0, saturn.CARRY, saturn.PC);

    printf("\n");
    usleep(100 * 1000);
}

int main() {
    saturn.ram = hp48_ram;
    saturn.rom = hp48_rom;

    while(1) {
        word_20 pc = saturn.PC;

        step_instruction();

        printf("%05lx: %s\n", pc, nibbles_trace);
        dump_cpu_state();
        memset(nibbles_trace, ' ', sizeof(nibbles_trace)-1);
        nibbles_traced = 0;
    }
}
