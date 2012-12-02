/*
 *	linux/arch/arm/mach-nspire/nspire_clp.c
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
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "common.h"
#include "classic.h"

static void __init clp_init(void)
{
	nspire_keypad_data.evtcodes = nspire_clickpad_evtcode_map;
	nspire_classic_init();
}

MACHINE_START(NSPIRECLP, "TI-NSPIRE Clickpad Calculator")
	.map_io		= nspire_map_io,
	.init_irq	= nspire_classic_init_irq,
	.timer		= &nspire_classic_sys_timer,
	.handle_irq = nspire_classic_handle_irq,
	.init_early		= nspire_init_early,
	.init_machine	= clp_init,
	.init_late	= nspire_classic_init_late,
	.restart	= nspire_restart,
MACHINE_END
