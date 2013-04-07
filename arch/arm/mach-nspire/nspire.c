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
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/dma-mapping.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>

#include <asm/hardware/timer-sp.h>

#include "mmio.h"

static const char *nspire_dt_match[] __initconst = {
	"arm,nspire",
	"arm,nspire-cx",
	"arm,nspire-tp",
	"arm,nspire-clp",
	NULL,
};

static struct map_desc nspire_io_desc[] __initdata = {
	{
		.virtual	=  NSPIRE_EARLY_UART_VIRT_BASE,
		.pfn		= __phys_to_pfn(NSPIRE_EARLY_UART_PHYS_BASE),
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

static struct clcd_panel nspire_cx_lcd_panel = {
	.mode		= {
		.name		= "Color LCD",
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

static struct clcd_panel nspire_classic_lcd_panel = {
	.mode		= {
		.name		= "Grayscale LCD",
		.refresh	= 60,
		.xres		= 320,
		.yres		= 240,
		.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		.vmode		= FB_VMODE_NONINTERLACED,
		.pixclock	= 1,
		.hsync_len	= 6,
		.vsync_len	= 1,
		.right_margin	= 6,
		.left_margin	= 6,
	},
	.width		= 71, /* 7.11cm */
	.height		= 53, /* 5.33cm */
	.tim2		= 0x80007d0,
	.cntl		= CNTL_LCDBPP8 | CNTL_LCDMONO8,
	.bpp		= 8,
	.grayscale	= 1
};

static int nspire_clcd_setup(struct clcd_fb *fb)
{
	struct clcd_panel *panel;
	size_t panel_size;
	const char *type;
	dma_addr_t dma;
	int err;

	BUG_ON(!fb->dev->dev.of_node);

	err = of_property_read_string(fb->dev->dev.of_node, "lcd-type", &type);
	if (err) {
		pr_err("CLCD: Could not find lcd-type property\n");
		return err;
	}

	if (!strcmp(type, "cx"))
	{
		panel = &nspire_cx_lcd_panel;
	}
	else if (!strcmp(type, "classic"))
	{
		panel = &nspire_classic_lcd_panel;
	}
	else
	{
		pr_err("CLCD: Unknown lcd-type %s\n", type);
		return -EINVAL;
	}

	panel_size = ((panel->mode.xres * panel->mode.yres) * panel->bpp) / 8;
	panel_size = ALIGN(panel_size, PAGE_SIZE);

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

static struct clcd_board nspire_clcd_data = {
	.name		= "LCD",
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

static void __init nspire_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
			nspire_auxdata, NULL);
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
