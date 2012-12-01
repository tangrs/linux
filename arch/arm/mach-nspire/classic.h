/*
 *	linux/arch/arm/mach-nspire/classic.h
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <asm/exception.h>

void __init nspire_classic_init_irq(void);
void __init nspire_classic_init(void);
void __init nspire_classic_init_late(void);
asmlinkage void __exception_irq_entry
    nspire_classic_handle_irq(struct pt_regs *regs);

extern struct irq_chip nspire_classic_irq_chip;
extern struct sys_timer nspire_classic_sys_timer;
