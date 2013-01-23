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

struct nspire_clk_divider {
	unsigned char base_cpu, cpu_ahb;
};

struct nspire_clk_speeds {
	unsigned long base;
	struct nspire_clk_divider div;
};

#define CLK_GET_CPU(cs) ((cs)->base / (cs)->div.base_cpu)
#define CLK_GET_AHB(cs) (CLK_GET_CPU(cs) / (cs)->div.cpu_ahb)

extern struct nspire_clk_speeds (*nspire_io_to_clocks)(unsigned long);
extern unsigned long (*nspire_clocks_to_io)(struct nspire_clk_speeds *);

static inline struct nspire_clk_speeds nspire_get_clocks(void)
{
	unsigned long val = readl(NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x00));
	BUG_ON(!nspire_io_to_clocks);
	return nspire_io_to_clocks(val);
}

static inline void nspire_set_clocks(struct nspire_clk_speeds *clks)
{
	unsigned long val;
	BUG_ON(!nspire_io_to_clocks);

	val = nspire_clocks_to_io(clks);

	writel(val, NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x00));
	writel(4,   NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x0c));
}

#endif
