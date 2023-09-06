#include <stdlib.h>
#include <stdio.h>

#include "cpu.h"
#include "instruction.h"
#include "roms/hp48_rom.h"

saturn_t saturn;

unsigned char hp48_ram[32 * 1024 * 2];

int main() {
    saturn.ram = hp48_ram;
    saturn.rom = hp48_rom;

    while(1) {
        printf("%05lx: \t", saturn.PC);
        step_instruction();
        printf("\n");
    }
}
