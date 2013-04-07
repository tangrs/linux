/*
 *  linux/include/linux/platform_data/nspire-keypad.h
 *
 *  Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#ifndef _LINUX_NSPIRE_KEYPAD_H
#define _LINUX_NSPIRE_KEYPAD_H

#define KEYPAD_BITMASK_COLS	11
#define KEYPAD_BITMASK_ROWS	8

struct nspire_keypad_data {
	struct matrix_keymap_data *keymap;

	/* Maximum delay estimated assuming 33MHz APB */
	u32 scan_interval;	/* In microseconds (~2000us max) */
	u32 row_delay;		/* In microseconds (~500us max) */

	bool active_low;
};

#endif
