/*
 *	linux/arch/arm/mach-nspire/nspire_cx.c
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/clkdev.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/input.h>
#include <linux/usb/ehci_pdriver.h>
#include <linux/mtd/nand.h>
#include <linux/platform_data/i2c-designware.h>

#include <mach/nspire_mmio.h>
#include <mach/nspire_clock.h>
#include <mach/irqs.h>
#include <mach/clkdev.h>
#include <mach/sram.h>
#include <mach/keypad.h>

#include <asm/mach/time.h>
#include <asm/hardware/timer-sp.h>
#include <asm/hardware/vic.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "common.h"
#include "touchpad.h"

/* Clock */

union reg_clk_speed {
	unsigned long raw;
	struct {
		unsigned long __padding0:1;
		unsigned long base_cpu_ratio:7;
		unsigned long is_base_48mhz:1;
		unsigned long __padding1:3;
		unsigned long cpu_ahb_ratio:3;
		unsigned long base_val:6;
		unsigned long unknown:2;
	} val;
};

static struct nspire_clk_speeds cx_io_to_clocks(unsigned long val)
{
	struct nspire_clk_speeds clks;
	union reg_clk_speed reg;

	reg.raw = val;
	reg.val.base_cpu_ratio *= reg.val.unknown;
	reg.val.cpu_ahb_ratio++;

	BUG_ON(reg.val.base_cpu_ratio == 0);

	clks.base = reg.val.is_base_48mhz ? 48 : 6*reg.val.base_val;
	clks.base *= 1000000; /* Convert to Hz */

	clks.div.base_cpu = reg.val.base_cpu_ratio;
	clks.div.cpu_ahb = reg.val.cpu_ahb_ratio;

	return clks;
}

static unsigned long cx_clocks_to_io(struct nspire_clk_speeds *clks)
{
	union reg_clk_speed reg;

	BUG_ON(clks->div.base_cpu < 1);
	BUG_ON(clks->div.cpu_ahb < 1);

	reg.raw = 0;
	reg.val.unknown = (clks->div.base_cpu & 0x1) ? 0b01 : 0b10;
	reg.val.base_cpu_ratio = clks->div.base_cpu / reg.val.unknown;
	reg.val.cpu_ahb_ratio = clks->div.cpu_ahb - 1;
	reg.val.is_base_48mhz = (clks->base <= 48000000);
	reg.val.base_val = (clks->base / 6000000);

	return reg.raw;
}

/* IRQ */
static void __init cx_init_irq(void)
{
	vic_init(IOMEM(NSPIRE_INTERRUPT_VIRT_BASE), 0, NSPIRE_IRQ_MASK, 0);
}

/* UART */

static AMBA_APB_DEVICE(uart, "uart", 0, NSPIRE_APB_PHYS(NSPIRE_APB_UART),
	{ NSPIRE_IRQ_UART }, NULL);

/* TIMER */

void __init cx_timer_init(void)
{
	sp804_clockevents_init(NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2),
		NSPIRE_IRQ_TIMER2, "timer2");
}

static struct sys_timer cx_sys_timer = {
	.init = cx_timer_init,
};

/* FRAMEBUFFER */
static struct clcd_panel cx_lcd_panel = {
	.mode		= {
		.name		= "color lcd",
		.refresh	= 60,
		.xres		= 320,
		.yres		= 240,
		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
		.pixclock	= 1,
		.hsync_len	= 6,
		.vsync_len	= 1,
		.right_margin	= 50,
		.left_margin	= 38,
		.lower_margin	= 3,
		.upper_margin	= 17,
	},
	.width		= 65, /* ~6.50 cm */
	.height		= 49, /* ~4.87 cm */
	.tim2		= TIM2_IPC,
	.cntl		= (CNTL_BGR | CNTL_LCDTFT | CNTL_LCDVCOMP(1) |
				CNTL_LCDBPP16_565),
	.bpp		= 16,
};
#define PANEL_SIZE (38 * SZ_4K)

static int cx_clcd_setup(struct clcd_fb *fb)
{
	return nspire_clcd_setup(fb, PANEL_SIZE, &cx_lcd_panel);
}

static struct clcd_board cx_clcd_data = {
	.name		= "lcd controller",
	.check		= clcdfb_check,
	.decode		= clcdfb_decode,
	.setup		= cx_clcd_setup,
	.mmap		= nspire_clcd_mmap,
	.remove		= nspire_clcd_remove,
};

static AMBA_AHB_DEVICE(fb, "fb", 0, NSPIRE_LCD_PHYS_BASE,
	{ NSPIRE_IRQ_LCD }, &cx_clcd_data);

/* USB HOST */

static __init int cx_usb_init(void)
{
	int err = 0;
	unsigned val;
	void __iomem *hostusb_addr =
		ioremap(NSPIRE_OTG_PHYS_BASE, NSPIRE_OTG_SIZE);

	if (!hostusb_addr) {
		pr_warn("Could not allocate enough memory to initialize NSPIRE host USB\n");
		err = -ENOMEM;
		goto out;
	}

	/* Disable OTG interrupts */
	pr_info("Disable OTG interrupts\n");
	val	 = readl(hostusb_addr + 0x1a4);
	val &= ~(0x7f<<24);
	writel(val, hostusb_addr + 0x1a4);

	iounmap(hostusb_addr);

	pr_info("Adding USB controller as platform device\n");
	err = platform_device_register(&nspire_cxusbhost_device);
out:

	return err;
}

static __init int cx_usb_workaround(void)
{
	int err = 0;
	unsigned val;
	void __iomem *hostusb_addr =
		ioremap(NSPIRE_OTG_PHYS_BASE, NSPIRE_OTG_SIZE);

	if (!hostusb_addr) {
		pr_warn("Could not do USB workaround\n");
		err = -ENOMEM;
		goto out;
	}

	pr_info("Temporary USB hack to force USB to connect as fullspeed\n");
	val	 = readl(hostusb_addr + 0x184);
	val |= (1<<24);
	writel(val, hostusb_addr + 0x184);

	iounmap(hostusb_addr);
out:

	return err;
}

/* I2C */

static struct resource i2c_resources[] = {
	{
		.start	= NSPIRE_APB_PHYS(NSPIRE_APB_I2C),
		.end	= NSPIRE_APB_PHYS(NSPIRE_APB_I2C + SZ_4K - 1),
		.flags	= IORESOURCE_MEM,
	},
	RESOURCE_ENTRY_IRQ(I2C)
};

static struct i2c_dw_platdata i2c_platdata = {
	/* OS defaults */
	.ss_hcnt = 0x9c,
	.ss_lcnt = 0xea,
	.fs_hcnt = 0x3b,
	.fs_lcnt = 0x2b
};

static struct platform_device i2c_device = {
	.name		= "i2c_designware",
	.id		= 0,
	.resource	= i2c_resources,
	.num_resources	= ARRAY_SIZE(i2c_resources),
	.dev = {
		.platform_data = &i2c_platdata
	}
};

/* Backlight driver */

#define CX_BACKLIGHT_UPPER	0x1d0
#define CX_BACKLIGHT_LOWER	0x100 /* Should be (around about) off */

static void cx_set_backlight(int val)
{
	val += CX_BACKLIGHT_LOWER;

	if (val <= CX_BACKLIGHT_UPPER)
		writel(val, NSPIRE_APB_VIRTIO(NSPIRE_APB_CONTRAST + 0x20));
}

static struct generic_bl_info cx_bl = {
	.name		= "nspire_backlight",
	.max_intensity	= CX_BACKLIGHT_UPPER - CX_BACKLIGHT_LOWER,
	.default_intensity = (CX_BACKLIGHT_UPPER - CX_BACKLIGHT_LOWER) / 2,
	.set_bl_intensity = cx_set_backlight
};

static struct platform_device bl_device = {
	.name		= "generic-bl",
	.id		= 0,
	.dev = {
		.platform_data = &cx_bl,
	}
};

/* NAND */
static struct resource nand_resources[] = {
	{
		.start	= 0x81000000,
		.end	= 0x81ffffff,
		.flags	= IORESOURCE_MEM,
	}
};

static struct platform_device nand_device = {
	.name		= "nspire_cx_nand",
	.id		= 0,
	.resource	= nand_resources,
	.num_resources	= ARRAY_SIZE(nand_resources),
};

/* Init */

extern bool cx_use_otg;

static void __init cx_early_init(void)
{
	nspire_io_to_clocks = cx_io_to_clocks;
	nspire_clocks_to_io = cx_clocks_to_io;

	nspire_init_early();
}

static void __init cx_init(void)
{
	nspire_init();
	amba_device_register(&fb_device, &iomem_resource);
	amba_device_register(&uart_device, &iomem_resource);

	nspire_keypad_data.evtcodes = nspire_touchpad_evtcode_map;
	platform_device_register(&nspire_keypad_device);
	platform_device_register(&nand_device);
	platform_device_register(&i2c_device);
	platform_device_register(&bl_device);
	nspire_touchpad_init();

	if (!cx_use_otg) {
		pr_info("Selecting USB host only driver for CX\n");
		cx_usb_init();
	} else {
		pr_info("Selecting USB OTG driver for CX\n");
	}
}

static void __init cx_init_late(void)
{
	if (!cx_use_otg)
		cx_usb_workaround();
	nspire_init_late();
}

MACHINE_START(NSPIRECX, "TI-NSPIRE CX Calculator")
	.nr_irqs	= NR_IRQS,
	.map_io		= nspire_map_io,
	.init_irq	= cx_init_irq,
	.handle_irq	= vic_handle_irq,
	.timer		= &cx_sys_timer,
	.init_early	= cx_early_init,
	.init_machine	= cx_init,
	.init_late	= cx_init_late,
	.restart	= nspire_restart,
MACHINE_END
