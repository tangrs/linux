/*
 *	linux/arch/arm/mach-nspire/include/mach/uncompress.h
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *	Copyright (C) 2013 Lionel Debroux <lionel_debroux@yahoo.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#ifndef NSPIRE_UNCOMPRESS_H
#define NSPIRE_UNCOMPRESS_H

#define OFFSET_VAL(var, offset) ((var)[(offset)>>2])
static inline void putc(int c)
{
}
#undef OFFSET_VAL

#define arch_decomp_setup()
#define flush()

#endif
