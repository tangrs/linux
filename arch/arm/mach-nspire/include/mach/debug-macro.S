#include <mach/nspire-regs.h>

.macro	addruart, rp, rv
    mov	\rp, #NSPIRE_APB_PHYS_BASE		@ physical base address
    mov	\rv, #NSPIRE_APB_VIRT_BASE		@ virtual base address
    add \rp, \rp, #NSPIRE_UART_OFFSET
    add \rv, \rv, #NSPIRE_UART_OFFSET
.endm

#include <asm/hardware/debug-pl01x.S>
