/*
 *	linux/arch/arm/mach-nspire/include/mach/debug-macro.S
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include "../../mmio.h"

.macro	addruart, rp, rv, tmp
	ldr \rp, =(NSPIRE_EARLY_UART_PHYS_BASE)		@ physical base address
	ldr \rv, =(NSPIRE_EARLY_UART_VIRT_BASE)		@ virtual base address
.endm

#include <asm/hardware/debug-pl01x.S>


/*
#define UART_SHIFT 2
#include <asm/hardware/debug-8250.S>
*/
