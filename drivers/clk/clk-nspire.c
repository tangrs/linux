/*
 *	linux/drivers/clk/clk-nspire.c
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */
#include <linux/clk-provider.h>
#include <linux/clkdev.h>

void __init nspire_init_clocks(void)
{
	struct clk *clk;

	/* Placeholder */

	clk = clk_register_fixed_rate(NULL, "base_clk", NULL, CLK_IS_ROOT,
					264000000);

	clk = clk_register_fixed_rate(NULL, "ahb_pclk", "base_clk", 0,
					66000000);

	clk = clk_register_fixed_rate(NULL, "apb_pclk", "ahb_pclk", 0,
					33000000);
	clk_register_clkdev(clk, NULL, "fast_timer");

	clk = clk_register_fixed_rate(NULL, "timer_clk", NULL, CLK_IS_ROOT,
					32768);
	clk_register_clkdev(clk, NULL, "timer0");
	clk_register_clkdev(clk, NULL, "timer1");

	clk = clk_register_fixed_rate(NULL, "uart_clk", NULL, CLK_IS_ROOT,
					12000000);
	clk_register_clkdev(clk, NULL, "timer0");

}
