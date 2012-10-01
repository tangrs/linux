#include <mach/nspire-regs.h>

.macro	addruart, rp, rv, tmp
    ldr	\rp, =(NSPIRE_UART_PHYS_BASE)		@ physical base address
    ldr	\rv, =(NSPIRE_UART_VIRT_BASE)		@ virtual base address
.endm

#include <asm/hardware/debug-pl01x.S>
