/* debugging */
#define NSPIRE_EARLY_UART_PHYS_BASE    0x90020000
#define NSPIRE_EARLY_UART_VIRT_BASE    0xfee20000

.macro	addruart, rp, rv, tmp
    ldr	\rp, =(NSPIRE_EARLY_UART_PHYS_BASE)		@ physical base address
    ldr	\rv, =(NSPIRE_EARLY_UART_VIRT_BASE)		@ virtual base address
.endm

#include <asm/hardware/debug-pl01x.S>
