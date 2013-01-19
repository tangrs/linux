/*
 *	linux/arch/arm/mach-nspire/include/mach/nspire_clock.h
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#ifndef NSPIRE_CLOCK_H
#define NSPIRE_CLOCK_H

#include <linux/io.h>

#include <mach/nspire_mmio.h>

struct nspire_clk_speeds {
	unsigned long cpu;
	unsigned long base;
	unsigned long ahb;
};

extern struct nspire_clk_speeds (*nspire_io_to_clocks)(unsigned long);
extern struct nspire_clk_speeds (*nspire_clocks_to_io)(
		struct nspire_clk_speeds *);

static inline struct nspire_clk_speeds nspire_get_clocks(void)
{
	unsigned long val = readl(NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x00));
	BUG_ON(!nspire_io_to_clocks);
	return nspire_io_to_clocks(val);
}

#endif
