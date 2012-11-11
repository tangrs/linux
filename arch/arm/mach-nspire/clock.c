/*
 *  linux/arch/arm/mach-nspire/clock.c
 *
 *  Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <mach/clkdev.h>

void clk_disable(struct clk *clk)
{
}

int clk_enable(struct clk *clk)
{
	return 0;
}

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->rate;
}

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	clk->rate = rate;
	return 0;
}
