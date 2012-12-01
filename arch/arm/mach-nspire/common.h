/*
 *	linux/arch/arm/mach-nspire/common.h
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */


#define IOTABLE_ENTRY(t) \
	{ \
		.virtual	= NSPIRE_##t##_VIRT_BASE, \
		.pfn		= __phys_to_pfn(NSPIRE_##t##_PHYS_BASE), \
		.length		= NSPIRE_##t##_SIZE, \
		.type		= MT_DEVICE \
	}

#define RESOURCE_ENTRY_IRQ(t) \
	{ \
		.start	= NSPIRE_IRQ_##t, \
		.end	= NSPIRE_IRQ_##t, \
		.flags	= IORESOURCE_IRQ \
	}

#define RESOURCE_ENTRY_MEM(t) \
	{ \
		.start	= NSPIRE_##t##_PHYS_BASE, \
		.end	= NSPIRE_##t##_PHYS_BASE + NSPIRE_##t##_SIZE - 1, \
		.flags	= IORESOURCE_MEM \
	}

extern struct platform_device nspire_keypad_device;
extern struct platform_device nspire_hostusb_device;

extern struct nspire_keypad_data nspire_keypad_data;

void __init nspire_map_io(void);
void __init nspire_init_early(void);
void __init nspire_init(void);
void __init nspire_init_late(void);

void nspire_restart(char mode, const char *cmd);

int nspire_clcd_mmap(struct clcd_fb *fb, struct vm_area_struct *vma);
void nspire_clcd_remove(struct clcd_fb *fb);
int nspire_clcd_setup(struct clcd_fb *fb, unsigned panel_size,
	struct clcd_panel * panel);
