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

struct nspire_keypad_data {
	unsigned int (*evtcodes)[11];
};

extern unsigned int nspire_touchpad_evtcode_map[][11];
extern unsigned int nspire_clickpad_evtcode_map[][11];

#endif
