/*
 *	linux/arch/arm/mach-nspire/include/mach/uncompress.h
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#ifndef NSPIRE_UNCOMPRESS_H
#define NSPIRE_UNCOMPRESS_H

#include <mach/nspire_mmio.h>


#define OFFSET_VAL(var, offset) ((var)[(offset)>>2])
static inline void putc(int c)
{
	volatile unsigned *serial_base = (volatile unsigned *)
		NSPIRE_APB_PHYS(NSPIRE_APB_UART);

#ifdef CONFIG_NSPIRE_EARLYPRINTK_CLASSIC
	OFFSET_VAL(serial_base, 0x00) = (unsigned char)c;
	while (! (OFFSET_VAL(serial_base, 0x14) & (1<<5)) ) barrier();
#endif

#ifdef CONFIG_NSPIRE_EARLYPRINTK_CX
	OFFSET_VAL(serial_base, 0x00) = (unsigned char)c;
	while (OFFSET_VAL(serial_base, 0x18) & (1<<5)) barrier();
#endif

}
#undef OFFSET_VAL

#define arch_decomp_setup()
#define flush()

#endif
