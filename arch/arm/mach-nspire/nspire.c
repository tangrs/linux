/*
 *  linux/arch/arm/mach-nspire/nspire.c
 *
 *  Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
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

#include <mach/nspire_mmio.h>
#include <mach/irqs.h>
#include <mach/clkdev.h>

#include <asm/mach/time.h>
#include <asm/hardware/timer-sp.h>
#include <asm/hardware/vic.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>


/**************** MAPIO ****************/
static struct map_desc nspire_io_regs[] __initdata = {
	{
		.virtual	= NSPIRE_APB_VIRT_BASE,
		.pfn		= __phys_to_pfn(NSPIRE_APB_PHYS_BASE),
		.length		= NSPIRE_APB_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= NSPIRE_VIC_VIRT_BASE,
		.pfn		= __phys_to_pfn(NSPIRE_VIC_PHYS_BASE),
		.length		= NSPIRE_VIC_SIZE,
		.type		= MT_DEVICE,
	}, /*{
		.virtual	= NSPIRE_LCD_VIRT_BASE,
		.pfn		= __phys_to_pfn(NSPIRE_LCD_PHYS_BASE),
		.length		= NSPIRE_LCD_SIZE,
		.type		= MT_DEVICE,
	}*/
};

void __init nspire_map_io(void)
{
	iotable_init(nspire_io_regs, ARRAY_SIZE(nspire_io_regs));
}


/**************** IRQ ****************/
void __init nspire_init_irq(void)
{
	vic_init(IOMEM(NSPIRE_VIC_VIRT_BASE), 0, NSPIRE_IRQ_MASK, 0);
}

/**************** UART **************/

static AMBA_APB_DEVICE(uart, "uart0", 0, NSPIRE_APB_PHYS(NSPIRE_APB_UART), { NSPIRE_IRQ_UART }, NULL);

/**************** TIMER ****************/
static struct clk sp804_clk = {
	.rate	= 32000,
};

static struct clk apb_clk = {
	.rate	= 33000000,
};
static struct clk ahb_clk = {
	.rate	= 66000000,
};

static struct clk_lookup lookup[] = {
    {
        .dev_id = "sp804",
        .con_id = "timer2",
        .clk = &sp804_clk
    },
    {
        .dev_id = "uart0",
        .clk = &apb_clk
    },
    {
        .dev_id = "fb",
        .clk = &ahb_clk
    },
};

void __init nspire_timer_init(void)
{
	sp804_clockevents_init(NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2), NSPIRE_IRQ_TIMER2, "timer2");
}

static struct sys_timer nspire_sys_timer = {
	.init		= nspire_timer_init,
};

/************** FRAMEBUFFER **************/
static struct clcd_panel lcd_panel = {
	    .mode		= {
		.name		= "calculator",
		.refresh	= 60,
		.xres		= 320,
		.yres		= 240,
		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
		.pixclock	= 187617,
		.hsync_len	= 6,
		.vsync_len	= 1,
	},
	.width		= -1,
	.height		= -1,
	.tim2		= (TIM2_IVS | TIM2_IHS),
	.cntl		= (CNTL_BGR | CNTL_LCDTFT | CNTL_LCDVCOMP(1) |
				CNTL_LCDBPP16_565),
	.bpp		= 16,
};
#define PANEL_SIZE (3 * SZ_64K)

static int nspire_clcd_setup(struct clcd_fb *fb)
{
	dma_addr_t dma;

	fb->fb.screen_base = dma_alloc_writecombine(&fb->dev->dev,
		PANEL_SIZE, &dma, GFP_KERNEL);
	if (!fb->fb.screen_base) {
		printk(KERN_ERR "CLCD: unable to map framebuffer\n");
		return -ENOMEM;
	}

	fb->fb.fix.smem_start = dma;
	fb->fb.fix.smem_len = PANEL_SIZE;
	fb->panel = &lcd_panel;

	return 0;
}

static int nspire_clcd_mmap(struct clcd_fb *fb, struct vm_area_struct *vma)
{
	return dma_mmap_writecombine(&fb->dev->dev, vma,
		fb->fb.screen_base, fb->fb.fix.smem_start,
		fb->fb.fix.smem_len);
}

static void nspire_clcd_remove(struct clcd_fb *fb)
{
	dma_free_writecombine(&fb->dev->dev, fb->fb.fix.smem_len,
		fb->fb.screen_base, fb->fb.fix.smem_start);
}

static void clcd_disable(struct clcd_fb *fb)
{
}

static void clcd_enable(struct clcd_fb *fb)
{
}

static struct clcd_board nspire_clcd_data = {
	.name		= "nspire lcd controller",
	.check		= clcdfb_check,
	.decode		= clcdfb_decode,
	.disable	= clcd_disable,
	.enable		= clcd_enable,
	.setup		= nspire_clcd_setup,
	.mmap		= nspire_clcd_mmap,
	.remove		= nspire_clcd_remove,
};

static AMBA_AHB_DEVICE(fb, "fb", 0, NSPIRE_LCD_PHYS_BASE, { NSPIRE_IRQ_LCD }, &nspire_clcd_data);


/************** INIT ***************/

void __init nspire_init_early(void){
    clkdev_add_table(lookup, ARRAY_SIZE(lookup));
}

void __init nspire_init(void)
{
    amba_device_register(&fb_device, &iomem_resource);
    amba_device_register(&uart_device, &iomem_resource);
}

void __init nspire_restart(char mode, const char *cmd)
{
	while (1) {
	    writel(2, NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x8));
	}
}

void __init nspire_init_late(void)
{
}

MACHINE_START(NSPIRE, "TI-NSPIRE CX Calculator")
	.map_io		= nspire_map_io,
	.init_irq	= nspire_init_irq,
	.handle_irq	= vic_handle_irq,
	.timer		= &nspire_sys_timer,
	.init_early     = nspire_init_early,
	.init_machine	= nspire_init,
	.init_late	= nspire_init_late,
	.restart	= nspire_restart,
MACHINE_END