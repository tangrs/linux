/*
 *	linux/arch/arm/mach-nspire/nspire.c
 *
 *	Copyright (C) 2013 Daniel Tang <tangrs@tangrs.id.au>
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
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/clocksource.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>

#include <asm/hardware/timer-sp.h>

#include "mmio.h"
#include "clcd.h"

static const char *nspire_dt_match[] __initconst = {
	"ti,nspire",
	"ti,nspire-cx",
	"ti,nspire-tp",
	"ti,nspire-clp",
	NULL,
};

static struct map_desc nspire_io_desc[] __initdata = {
	{
		.virtual	=  NSPIRE_EARLY_UART_VIRT_BASE,
		.pfn		= __phys_to_pfn(NSPIRE_EARLY_UART_PHYS_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  NSPIRE_PWR_VIRT_BASE,
		.pfn		= __phys_to_pfn(NSPIRE_PWR_PHYS_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	}
};

static void __init nspire_map_io(void)
{
	iotable_init(nspire_io_desc, ARRAY_SIZE(nspire_io_desc));
}

static struct clcd_board nspire_clcd_data = {
	.name		= "LCD",
	.caps		= CLCD_CAP_444 | CLCD_CAP_888 | CLCD_CAP_565,
	.check		= clcdfb_check,
	.decode		= clcdfb_decode,
	.setup		= nspire_clcd_setup,
	.mmap		= nspire_clcd_mmap,
	.remove		= nspire_clcd_remove,
};


static struct of_dev_auxdata nspire_auxdata[] __initdata = {
	OF_DEV_AUXDATA("arm,pl111", NSPIRE_LCD_PHYS_BASE,
			NULL, &nspire_clcd_data),
	{ }
};

static void __init nspire_early_init(void)
{
	void __iomem *pwr = IOMEM(NSPIRE_PWR_VIRT_BASE);

	/* Re-enable bus access to all peripherals */
	writel(0, pwr + NSPIRE_PWR_BUS_DISABLE1);
	writel(0, pwr + NSPIRE_PWR_BUS_DISABLE2);

	pr_info("Re-enabled bus access to all peripherals\n");
}

static void __init nspire_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
			nspire_auxdata, NULL);
}

static void __init nspire_init_time(void)
{
	of_clk_init(NULL);
	clocksource_of_init();
}

static void nspire_restart(char mode, const char *cmd)
{
	void __iomem *base = ioremap(NSPIRE_MISC_PHYS_BASE, SZ_4K);
	if (!base)
		return;

	writel(2, base + NSPIRE_MISC_HWRESET);
}

DT_MACHINE_START(NSPIRE, "TI-NSPIRE")
	.map_io		= nspire_map_io,
	.init_irq	= irqchip_init,
	.init_time	= nspire_init_time,
	.init_machine	= nspire_init,
	.init_early	= nspire_early_init,
	.dt_compat	= nspire_dt_match,
	.restart	= nspire_restart,
MACHINE_END
