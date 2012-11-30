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

#include <mach/nspire_mmio.h>
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

/**************** IRQ ****************/
static void __init cx_init_irq(void)
{
	vic_init(IOMEM(NSPIRE_INTTERUPT_VIRT_BASE), 0, NSPIRE_IRQ_MASK, 0);
}

/**************** UART **************/

static AMBA_APB_DEVICE(uart, "uart0", 0, NSPIRE_APB_PHYS(NSPIRE_APB_UART), { NSPIRE_IRQ_UART }, NULL);

/**************** TIMER ****************/

void __init cx_timer_init(void)
{
	sp804_clockevents_init(NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2),
		NSPIRE_IRQ_TIMER2, "timer2");
}

static struct sys_timer cx_sys_timer = {
	.init = cx_timer_init,
};

/************** FRAMEBUFFER **************/
static struct clcd_panel cx_lcd_panel = {
	.mode		= {
		.name		= "color lcd",
		.refresh	= 60,
		.xres		= 320,
		.yres		= 240,
		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
		.pixclock	= 1,
		.hsync_len	= 1,
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

static int cx_clcd_setup(struct clcd_fb *fb)
{
	dma_addr_t dma;

	fb->fb.screen_base = dma_alloc_writecombine(&fb->dev->dev,
		PANEL_SIZE, &dma, GFP_KERNEL);
	if (!fb->fb.screen_base) {
		pr_err("CLCD: unable to map framebuffer\n");
		return -ENOMEM;
	}

	fb->fb.fix.smem_start = dma;
	fb->fb.fix.smem_len = PANEL_SIZE;
	fb->panel = &cx_lcd_panel;

	return 0;
}

static int cx_clcd_mmap(struct clcd_fb *fb, struct vm_area_struct *vma)
{
	return dma_mmap_writecombine(&fb->dev->dev, vma,
		fb->fb.screen_base, fb->fb.fix.smem_start,
		fb->fb.fix.smem_len);
}

static void cx_clcd_remove(struct clcd_fb *fb)
{
	dma_free_writecombine(&fb->dev->dev, fb->fb.fix.smem_len,
		fb->fb.screen_base, fb->fb.fix.smem_start);
}

static struct clcd_board cx_clcd_data = {
	.name		= "lcd controller",
	.check		= clcdfb_check,
	.decode		= clcdfb_decode,
	.setup		= cx_clcd_setup,
	.mmap		= cx_clcd_mmap,
	.remove		= cx_clcd_remove,
};

static AMBA_AHB_DEVICE(fb, "fb", 0, NSPIRE_LCD_PHYS_BASE,
	{ NSPIRE_IRQ_LCD }, &cx_clcd_data);

/************** USB HOST *************/

static __init int cx_usb_init(void)
{
	int err = 0;
	unsigned val;
	void __iomem *hostusb_addr = ioremap(NSPIRE_HOSTUSB_PHYS_BASE, NSPIRE_HOSTUSB_SIZE);

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
	err = platform_device_register(&nspire_hostusb_device);
out:

	return err;
}

static __init int cx_usb_workaround(void)
{
	int err = 0;
	unsigned val;
	void __iomem *hostusb_addr = ioremap(NSPIRE_HOSTUSB_PHYS_BASE, NSPIRE_HOSTUSB_SIZE);

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

/************** INIT ***************/

void __init cx_init(void)
{
	nspire_init();
	amba_device_register(&fb_device, &iomem_resource);
	amba_device_register(&uart_device, &iomem_resource);
	//platform_device_register(&nspire_nand_device);

	nspire_keypad_data.evtcodes = nspire_touchpad_evtcode_map;
	platform_device_register(&nspire_keypad_device);
	cx_usb_init();
}

void __init cx_init_late(void)
{
	nspire_init_late();
	cx_usb_workaround();
}

MACHINE_START(NSPIRECX, "TI-NSPIRE CX Calculator")
	.map_io		= nspire_map_io,
	.init_irq	= cx_init_irq,
	.handle_irq = vic_handle_irq,
	.timer		= &cx_sys_timer,
	.init_early		= nspire_init_early,
	.init_machine	= cx_init,
	.init_late	= cx_init_late,
	.restart	= nspire_restart,
MACHINE_END
