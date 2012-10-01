#include <mach/nspire-regs.h>

static inline void __attribute__((naked)) putc(int c) {
    asm volatile ("mov r1, #0x90000000\n"
    "add r1, r1, #0x20000\n"
    "strb r0, [r1, #0]\n"
    "1: ldr  r0, [r1, #0x18]\n"
    "tst r0, #(1<<5)\n"
    "bne 1b\n"
    "bx lr");
}
#define arch_decomp_setup()
#define flush()