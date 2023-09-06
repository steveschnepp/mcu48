#include <stdlib.h>
#include <stdio.h>

#include "cpu.h"
#include "instruction.h"
#include "roms/hp48_rom.h"

saturn_t saturn;

unsigned char hp48_ram[32 * 1024 * 2];

int main() {
    saturn.ram = hp48_ram;

    // Convert to nibbles
    saturn.rom = malloc(hp48_rom_length * 2);
    for (int i = hp48_rom_start; i < hp48_rom_finish; i ++) {
        saturn.rom[i*2+1] = hp48_rom[i] & 0xF;
        saturn.rom[i*2] = hp48_rom[i] >> 4;
    }

    while(1) {
        printf("%05lx: \t", saturn.PC);
        step_instruction();
        printf("\n");
    }
}
