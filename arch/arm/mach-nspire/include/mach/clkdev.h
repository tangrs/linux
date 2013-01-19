/*
 *  linux/arch/arm/mach-nspire/include/mach/clkdev.h
 *
 *  Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#ifndef NSPIRE_CLKDEV_H
#define NSPIRE_CLKDEV_H

struct clk {
	void (*get_rate)(struct clk *clk);
	int (*set_rate)(struct clk *clk, unsigned long rate);
	unsigned long rate;
};

#define __clk_get(clk) ({ 1; })
#define __clk_put(clk) do { } while (0)

#endif
