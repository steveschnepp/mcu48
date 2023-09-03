#ifndef __CPU_H__
#define __CPU_H__

#include <stdint.h>

// Using unions to enable automatic sign bit expansion if using the .s field
typedef union { unsigned long u : 20; long s : 20; } word20_t;
typedef union { unsigned long u : 16; long s : 16; } word16_t;
typedef union { unsigned long u : 12; long s : 12; } word12_t;
typedef union { unsigned long u :  8; long s :  8; }  word8_t;
typedef union { unsigned long u :  4; long s :  4; }  word4_t;
typedef union { unsigned long u :  1; long s :  1; }  word1_t;

/* Warning this is *NOT* portable C, but it works well in practice */
typedef union
{
    unsigned long long w : 64;
    unsigned long a : 20;

    struct {
        unsigned long b : 8;
        unsigned long xs: 4;
    };

    struct {
        unsigned long x: 12;
        unsigned long long m: 48;
        unsigned long s:  4;
    };
} register_t;

struct cpu {
    // Registers
    register_t A, B, C, D;
    register_t R[5];

    word20_t DPTR[2];

    word4_t P;
    word20_t PC;

    word16_t IN;
    word12_t OUT;
    word1_t FLAG;

    word16_t ST;
    struct {
        unsigned int XM : 1; // e(X)ternal (M)odule missing
        unsigned int SR : 1; // (S)ervice (R)equest
        unsigned int SB : 1; // (S)ticky (B)it
        unsigned int MP : 1; // (M)odule (P)ulled
    } HST;


};

typedef struct cpu Cpu;

extern struct cpu saturn;

extern uint8_t hp48_ram[];
extern uint8_t hp48_rom[];

#endif // __CPU_H__
