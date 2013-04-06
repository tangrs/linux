/*
 *	linux/arch/arm/mach-nspire/nspire.c
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */
#include <linux/init.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/irqchip.h>
#include <linux/irqchip/arm-vic.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>

#include <asm/hardware/timer-sp.h>


static const char *nspire_dt_match[] __initconst = {
	"arm,nspire",
	"arm,nspire-cx",
	"arm,nspire-tp",
	"arm,nspire-clp",
	NULL,
};

static struct map_desc nspire_io_desc[] __initdata = {
	{
		.virtual	=  0xfee20000,
		.pfn		= __phys_to_pfn(0x90020000),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	}
};

static void __init nspire_init_timer(void)
{
	struct device_node *timer;
	void __iomem *base;
	const char *path;
	struct clk *clk;
	int irq, err;

	of_clk_init(NULL);

	err = of_property_read_string(of_aliases, "timer0", &path);
	if (WARN_ON(err))
		return;

	timer = of_find_node_by_path(path);
	base = of_iomap(timer, 0);
	if (WARN_ON(!base))
		return;

	clk = of_clk_get_by_name(timer, NULL);
	clk_register_clkdev(clk, timer->name, "sp804");

	sp804_clocksource_init(base, timer->name);

	err = of_property_read_string(of_aliases, "timer1", &path);
	if (WARN_ON(err))
		return;

	timer = of_find_node_by_path(path);
	base = of_iomap(timer, 0);
	if (WARN_ON(!base))
		return;

	clk = of_clk_get_by_name(timer, NULL);
	clk_register_clkdev(clk, timer->name, "sp804");

	irq = irq_of_parse_and_map(timer, 0);
	sp804_clockevents_init(base, irq, timer->name);
}

static void __init nspire_map_io(void)
{
	iotable_init(nspire_io_desc, ARRAY_SIZE(nspire_io_desc));
}

static void __init nspire_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
			NULL, NULL);
}

static void nspire_restart(char mode, const char *cmd)
{
}

DT_MACHINE_START(NSPIRE, "TI-NSPIRE")
	.map_io		= nspire_map_io,
	.init_irq	= irqchip_init,
	.init_time	= nspire_init_timer,
	.init_machine	= nspire_init,
	.dt_compat	= nspire_dt_match,
	.restart	= nspire_restart,
MACHINE_END
