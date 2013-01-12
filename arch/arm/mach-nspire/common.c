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
#include <linux/usb/chipidea.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/dma-mapping.h>

#include <mach/nspire_mmio.h>
#include <mach/irqs.h>
#include <mach/clkdev.h>
#include <mach/keypad.h>
#include <mach/sram.h>

#include <asm/mach/time.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>

#include "adc.h"
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

#ifdef CONFIG_MACH_NSPIRECX
static struct clk i2c_clk = {
	.rate	= 250000,
};
#endif

static struct clk_lookup nspire_clk_lookup[] = {
	{
		.dev_id = "uart",
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
	{
		.dev_id = "i2c_designware.0",
		.clk = &i2c_clk
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


/* GPIO */
static struct resource nspire_gpio_resources[] = {
	{
		.start	= NSPIRE_APB_PHYS(NSPIRE_APB_GPIO),
		.end	= NSPIRE_APB_PHYS(NSPIRE_APB_GPIO + SZ_4K - 1),
		.flags	= IORESOURCE_MEM,
	},
	RESOURCE_ENTRY_IRQ(GPIO)
};

static struct platform_device nspire_gpio_device = {
	.name		= "gpio-nspire",
	.resource	= nspire_gpio_resources,
	.num_resources	= ARRAY_SIZE(nspire_gpio_resources),
};

/* Framebuffer */
int nspire_clcd_setup(struct clcd_fb *fb, unsigned panel_size,
	struct clcd_panel *panel)
{
	dma_addr_t dma;

	fb->fb.screen_base = dma_alloc_writecombine(&fb->dev->dev,
		panel_size, &dma, GFP_KERNEL);
	if (!fb->fb.screen_base) {
		pr_err("CLCD: unable to map framebuffer\n");
		return -ENOMEM;
	}

	fb->fb.fix.smem_start = dma;
	fb->fb.fix.smem_len = panel_size;
	fb->panel = panel;

	return 0;
}

int nspire_clcd_mmap(struct clcd_fb *fb, struct vm_area_struct *vma)
{
	return dma_mmap_writecombine(&fb->dev->dev, vma,
		fb->fb.screen_base, fb->fb.fix.smem_start,
		fb->fb.fix.smem_len);
}

void nspire_clcd_remove(struct clcd_fb *fb)
{
	dma_free_writecombine(&fb->dev->dev, fb->fb.fix.smem_len,
		fb->fb.screen_base, fb->fb.fix.smem_start);
}

/* USB */

static u64 usb_dma_mask = ~(u32)0;


/* CX USB host */

static struct usb_ehci_pdata cxusbhost_pdata = {
	.has_tt = 1,
	.caps_offset = 0x100
};

static struct resource cxusbhost_resources_pdata[] = {
	RESOURCE_ENTRY_MEM(OTG),
	RESOURCE_ENTRY_IRQ(OTG)
};

struct platform_device nspire_cxusbhost_device = {
	.name		= "ehci-platform",
	.id		= 0,
	.dev = {
		.platform_data = &cxusbhost_pdata,
		.coherent_dma_mask = ~0,
		.dma_mask = &usb_dma_mask
	},
	.resource = cxusbhost_resources_pdata,
	.num_resources = ARRAY_SIZE(cxusbhost_resources_pdata)
};

/* Generic OTG */

static struct resource otg_resources[] = {
	RESOURCE_ENTRY_MEM(OTG),
	RESOURCE_ENTRY_IRQ(OTG)
};

static struct ci13xxx_platform_data otg_pdata = {
	.name = "nspire_usb",
	.capoffset = 0x100,
	.flags = CI13XXX_REGS_SHARED,
};


static struct platform_device otg_device = {
	.name		= "ci_hdrc",
	.id		= 0,
	.dev = {
		.platform_data = &otg_pdata,
		.coherent_dma_mask = ~0,
		.dma_mask = &usb_dma_mask
	},
	.resource = otg_resources,
	.num_resources = ARRAY_SIZE(otg_resources)
};

static struct platform_device usb_nop_xceiver = {
	.name		= "nop_usb_xceiv",
};

bool cx_use_otg;
static int __init set_cx_otg(char *dummy __attribute__((unused)))
{
	cx_use_otg = 1;
	return 0;
}
early_param("cx_use_otg", set_cx_otg);

/* RTC */
static struct resource nspire_rtc_resources[] = {
	{
		.start	= NSPIRE_APB_PHYS(NSPIRE_APB_RTC),
		.end	= NSPIRE_APB_PHYS(NSPIRE_APB_RTC + SZ_4K - 1),
		.flags	= IORESOURCE_MEM,
	},
	RESOURCE_ENTRY_IRQ(RTC)
};

static struct platform_device nspire_rtc_device = {
	.name		= "nspire-rtc",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(nspire_rtc_resources),
	.resource	= nspire_rtc_resources,
};

/* Memory mapped IO */
struct map_desc nspire_io_regs[] __initdata = {
	IOTABLE_ENTRY(ADC),
	IOTABLE_ENTRY(APB),
	IOTABLE_ENTRY(BOOT1),
	IOTABLE_ENTRY(INTERRUPT),
};

void __init nspire_map_io(void)
{
	iotable_init(nspire_io_regs, ARRAY_SIZE(nspire_io_regs));
}

/* Clocks */
void __init nspire_init_early(void)
{
	clkdev_add_table(nspire_clk_lookup, ARRAY_SIZE(nspire_clk_lookup));

	/* Renable bus access to everything in case the OS disabled them */
	writel(1 << 7, NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x18));
	writel(0, NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x20));
}

/* Common init */
void __init nspire_init(void)
{
	adc_init();
	sram_init(NSPIRE_SRAM_PHYS_BASE, NSPIRE_SRAM_SIZE);
	platform_device_register(&nspire_gpio_device);
	platform_device_register(&nspire_rtc_device);

	if (!machine_is_nspirecx() || cx_use_otg) {
		platform_device_register(&otg_device);
		platform_device_register(&usb_nop_xceiver);
	}
}

void __init nspire_init_late(void)
{
	adc_procfs_init();
	boot1_procfs_init();
	contrast_procfs_init();
}

/* Restart */
void nspire_restart(char mode, const char *cmd)
{
	writel(2, NSPIRE_APB_VIRTIO(NSPIRE_APB_MISC + 0x8));
}
