/*
 *	linux/arch/arm/mach-nspire/common.c
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/init.h>
#include <linux/clkdev.h>
#include <linux/platform_device.h>
#include <linux/usb/ehci_pdriver.h>

#include <mach/nspire_mmio.h>
#include <mach/irqs.h>
#include <mach/clkdev.h>
#include <mach/keypad.h>
#include <mach/sram.h>

#include <asm/mach/time.h>
#include <asm/mach/map.h>


#include "common.h"
#include "boot1.h"
#include "contrast.h"
#include "clock.h"

/* Clocks */
static struct clk systimer_clk = {
	.rate	= 32768,
};

static struct clk uart_clk = {
	.rate	= 12000000,
};

static struct clk ahb_clk = {
	.rate	= 66000000,
};

static struct clk_lookup nspire_clk_lookup[] = {
	{
		.dev_id = "uart0",
		.clk = &uart_clk
	},
	{
		.dev_id = "fb",
		.clk = &ahb_clk
	},
	{
		.con_id = "ahb",
		.clk = &ahb_clk
	},
#ifdef CONFIG_MACH_NSPIRECX
	{
		.dev_id = "sp804",
		.con_id = "timer2",
		.clk = &systimer_clk
	},
#endif
#if defined(CONFIG_MACH_NSPIRECLP) || defined(CONFIG_MACH_NSPIRETP)
	{
		.dev_id = NULL,
		.con_id = "timer2",
		.clk = &systimer_clk
	},
#endif
};

/* Keypad */
static struct resource nspire_keypad_resources[] = {
	{
		.start	= NSPIRE_APB_PHYS(NSPIRE_APB_KEYPAD),
		.end	= NSPIRE_APB_PHYS(NSPIRE_APB_KEYPAD + SZ_4K - 1),
		.flags	= IORESOURCE_MEM,
	},
	RESOURCE_ENTRY_IRQ(KEYPAD)
};

struct nspire_keypad_data nspire_keypad_data;

struct platform_device nspire_keypad_device = {
	.name		= "nspire-keypad",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(nspire_keypad_resources),
	.resource	= nspire_keypad_resources,
	.dev = {
		.platform_data = &nspire_keypad_data
	}
};

/* USB HOST */
static struct usb_ehci_pdata nspire_hostusb_pdata = {
	.has_tt = 1,
	.caps_offset = 0x100
};

static struct resource nspire_hostusb_resources[] = {
	RESOURCE_ENTRY_MEM(HOSTUSB),
	RESOURCE_ENTRY_IRQ(HOSTUSB)
};
static u64 nspire_usb_dma_mask = ~(u32)0;

struct platform_device nspire_hostusb_device = {
	.name		= "ehci-platform",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(nspire_hostusb_resources),
	.resource	= nspire_hostusb_resources,
	.dev = {
		.platform_data = &nspire_hostusb_pdata,
		.coherent_dma_mask = ~0,
		.dma_mask = &nspire_usb_dma_mask
	}
};

/* Memory mapped IO */
struct map_desc nspire_io_regs[] __initdata = {
	IOTABLE_ENTRY(APB),
	IOTABLE_ENTRY(BOOT1),
	IOTABLE_ENTRY(INTTERUPT),
};

void __init nspire_map_io(void)
{
	iotable_init(nspire_io_regs, ARRAY_SIZE(nspire_io_regs));
}

/* Clocks */
void __init nspire_init_early(void)
{
	clkdev_add_table(nspire_clk_lookup, ARRAY_SIZE(nspire_clk_lookup));
}

/* Common init */
void __init nspire_init(void) {
	sram_init(NSPIRE_SRAM_PHYS_BASE, NSPIRE_SRAM_SIZE);
}

void __init nspire_init_late(void)
{
	boot1_procfs_init();
	contrast_procfs_init();
}

/* Restart */
void nspire_restart(char mode, const char *cmd)
{
	writel(2, NSPIRE_APB_VIRTIO(NSPIRE_APB_MISC + 0x8));
}
