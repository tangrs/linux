/*
 *  linux/arch/arm/mach-nspire/include/mach/uncompress.h
 *
 *  Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#ifndef NSPIRE_UNCOMPRESS_H
#define NSPIRE_UNCOMPRESS_H

#include <mach/nspire_mmio.h>

static inline void __attribute__((naked)) putc(int c)
{
	asm volatile ("mov r1, #0x90000000\n"
	"add r1, r1, #0x20000\n"
	"strb r0, [r1, #0]\n"
	"1: ldr  r0, [r1, #0x18]\n"
	"tst r0, #(1<<5)\n"
	"bne 1b\n"
	"bx lr");
}

#define arch_decomp_setup()
#define flush()

#endif
