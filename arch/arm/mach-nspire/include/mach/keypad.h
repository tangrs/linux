/*
 *  linux/arch/arm/mach-nspire/include/mach/keypad.h
 *
 *  Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#ifndef NSPIRE_KEYPAD_H
#define NSPIRE_KEYPAD_H

#define KEYPAD_BITMASK_COLS	11
#define KEYPAD_BITMASK_ROWS	8

struct nspire_keypad_data {
	unsigned int (*evtcodes)[KEYPAD_BITMASK_COLS];
	bool active_low;
};

extern unsigned int nspire_touchpad_evtcode_map[][KEYPAD_BITMASK_COLS];
extern unsigned int nspire_clickpad_evtcode_map[][KEYPAD_BITMASK_COLS];

#endif
