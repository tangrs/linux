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
#include <linux/input.h>
#include <linux/usb/ehci_pdriver.h>
#include <linux/platform_data/usb-ehci-mxc.h>

#include <mach/nspire_mmio.h>
#include <mach/irqs.h>
#include <mach/clkdev.h>
#include <mach/sram.h>

#include <asm/mach/time.h>
#include <asm/hardware/timer-sp.h>
#include <asm/hardware/vic.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <mach/keypad.h>

#include "boot1.h"
#include "contrast.h"

/**************** MAPIO ****************/
static struct map_desc nspire_io_regs[] __initdata = {
	{
		.virtual	= NSPIRE_APB_VIRT_BASE,
		.pfn		= __phys_to_pfn(NSPIRE_APB_PHYS_BASE),
		.length		= NSPIRE_APB_SIZE,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= NSPIRE_VIC_VIRT_BASE,
		.pfn		= __phys_to_pfn(NSPIRE_VIC_PHYS_BASE),
		.length		= NSPIRE_VIC_SIZE,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= NSPIRE_BOOT1_VIRT_BASE,
		.pfn		= __phys_to_pfn(NSPIRE_BOOT1_PHYS_BASE),
		.length		= NSPIRE_BOOT1_SIZE,
		.type		= MT_DEVICE,
	},
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
	.rate	= 32768,
};

static struct clk uart_clk = {
	.rate	= 12000000,
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
	{
		.con_id = "ipg",
		.clk = &ahb_clk
	}
};

void __init nspire_timer_init(void)
{
	sp804_clockevents_init(NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2), NSPIRE_IRQ_TIMER2, "timer2");
}

static struct sys_timer nspire_sys_timer = {
	.init = nspire_timer_init,
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
		pr_err("CLCD: unable to map framebuffer\n");
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


/************** Keypad *************/
static unsigned int nspire_cx_evtcode_map[][11] = {
	{ KEY_ENTER, KEY_ENTER, 0, 0, KEY_SPACE, KEY_Z, KEY_Y, KEY_0, KEY_TAB, 0, 0 },
	{ KEY_X, KEY_W, KEY_V, KEY_3, KEY_U, KEY_T, KEY_S, KEY_1, 0, 0, KEY_RIGHT },
	{ KEY_R, KEY_Q, KEY_P, KEY_6, KEY_O, KEY_N, KEY_M, KEY_4, KEY_APOSTROPHE, KEY_DOWN, 0 },
	{ KEY_L, KEY_K, KEY_J, KEY_9, KEY_I, KEY_H, KEY_G, KEY_7, KEY_SLASH, KEY_LEFT, 0 },
	{ KEY_F, KEY_E, KEY_D, 0, KEY_C, KEY_B, KEY_A, KEY_EQUAL, KEY_KPASTERISK, KEY_UP, 0},
	{ 0, KEY_LEFTALT, KEY_MINUS, KEY_RIGHTBRACE, KEY_DOT, KEY_LEFTBRACE, KEY_5, 0, KEY_SEMICOLON, KEY_BACKSPACE, KEY_DELETE },
	{ KEY_BACKSLASH, 0, KEY_KPPLUS, KEY_PAGEUP, KEY_2, KEY_PAGEDOWN, KEY_8, KEY_ESC, 0, KEY_TAB, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_COMMA }
};

static struct resource keypad_resources[] = {
	{
		.start	= NSPIRE_APB_PHYS(NSPIRE_APB_KEYPAD),
		.end	= NSPIRE_APB_PHYS(NSPIRE_APB_KEYPAD + SZ_4K),
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= NSPIRE_IRQ_KEYPAD,
		.end	= NSPIRE_IRQ_KEYPAD,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct nspire_keypad_data keypad_data = {
	.evtcodes = nspire_cx_evtcode_map
};

static struct platform_device keypad_device = {
	.name		= "nspire-keypad",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(keypad_resources),
	.resource	= keypad_resources,
	.dev = {
		.platform_data = &keypad_data
	}
};

/************** USB HOST *************/

static struct usb_ehci_pdata hostusb_pdata = {
	.has_tt = 1,
	.caps_offset = 0x100
};

static struct resource hostusb_resources[] = {
	{
		.start	= NSPIRE_HOSTUSB_PHYS_BASE,
		.end	= NSPIRE_HOSTUSB_PHYS_BASE + NSPIRE_HOSTUSB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= NSPIRE_IRQ_HOSTUSB,
		.end	= NSPIRE_IRQ_HOSTUSB,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 usb_dma_mask = ~(u32)0;

static struct platform_device hostusb_device = {
	.name		= "ehci-platform",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(hostusb_resources),
	.resource	= hostusb_resources,
	.dev = {
	    .platform_data = &hostusb_pdata,
	    .coherent_dma_mask = ~0,
	    .dma_mask = &usb_dma_mask
	}
};

static __init int nspire_usb_init(void)
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
	val  = readl(hostusb_addr + 0x1a4);
	val &= ~(0x7f<<24);
	writel(val, hostusb_addr + 0x1a4);

	iounmap(hostusb_addr);

	pr_info("Adding USB controller as platform device\n");
	err = platform_device_register(&hostusb_device);
out:

	return err;
}

static __init int nspire_usb_workaround(void)
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
	val  = readl(hostusb_addr + 0x184);
	val |= (1<<24);
	writel(val, hostusb_addr + 0x184);

	iounmap(hostusb_addr);
out:

	return err;
}

/************** INIT ***************/

void __init nspire_init_early(void)
{
	clkdev_add_table(lookup, ARRAY_SIZE(lookup));
}

void __init nspire_init(void)
{
	sram_init(NSPIRE_SRAM_PHYS_BASE, NSPIRE_SRAM_SIZE);
	amba_device_register(&fb_device, &iomem_resource);
	amba_device_register(&uart_device, &iomem_resource);
	platform_device_register(&keypad_device);
	nspire_usb_init();
}

void nspire_restart(char mode, const char *cmd)
{
	writel(2, NSPIRE_APB_VIRTIO(NSPIRE_APB_MISC + 0x8));
}

void __init nspire_init_late(void)
{
	boot1_procfs_init();
	contrast_procfs_init();
	nspire_usb_workaround();
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
