/*
 *	linux/arch/arm/mach-nspire/include/mach/nspire_mmio.h
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#ifndef NSPIRE_MMIO_H
#define NSPIRE_MMIO_H

#include <asm-generic/sizes.h>

/*
	Memory map:
	0xfee00000 - 0xff000000			0x90000000 - 0x90200000		(APB)
	0xfedff000 - 0xfee00000			0xdc000000 - 0xdc001000		(Interrupt Controller)
	0xfed7f000 - 0xfedff000			0x00000000 - 0x00080000		(Boot1 ROM)
*/
#define NSPIRE_APB_PHYS_BASE		0x90000000
#define NSPIRE_APB_SIZE				SZ_2M
#define NSPIRE_APB_VIRT_BASE		0xfee00000

#define NSPIRE_APB_PHYS(x)			((NSPIRE_APB_PHYS_BASE) + (x))
#define NSPIRE_APB_VIRT(x)			((NSPIRE_APB_VIRT_BASE) + (x))
#define NSPIRE_APB_VIRTIO(x)		IOMEM(NSPIRE_APB_VIRT(x))


#define NSPIRE_INTERRUPT_PHYS_BASE	0xDC000000
#define NSPIRE_INTERRUPT_SIZE		SZ_4K
#define NSPIRE_INTERRUPT_VIRT_BASE	0xfedff000

#define NSPIRE_LCD_PHYS_BASE		0xC0000000

#define NSPIRE_HOSTUSB_PHYS_BASE	0xB0000000
#define NSPIRE_HOSTUSB_SIZE			SZ_8K

#define NSPIRE_NAND_PHYS_BASE		0x81000000
#define NSPIRE_NAND_SIZE			SZ_16M

#define NSPIRE_BOOT1_PHYS_BASE		0x00000000
#define NSPIRE_BOOT1_SIZE			0x00080000
#define NSPIRE_BOOT1_VIRT_BASE		0xfed7f000

#define NSPIRE_APB_TIMER2			0xD0000
#define NSPIRE_APB_MISC				0xA0000
#define NSPIRE_APB_POWER			0xB0000
#define NSPIRE_APB_UART				0x20000
#define NSPIRE_APB_KEYPAD			0xE0000
#define NSPIRE_APB_CONTRAST			0xF0000

#define NSPIRE_SRAM_PHYS_BASE		0xa4000000
#define NSPIRE_SRAM_SIZE			0x00020000

#endif
