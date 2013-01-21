/*
 *  linux/arch/arm/mach-nspire/include/mach/irqs.h
 *
 *  Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#ifndef NSPIRE_IRQS_H
#define NSPIRE_IRQS_H

#define NSPIRE_IRQ_MASK         0x007FEB9A

enum {
	NSPIRE_IRQ_UART = 1,
	NSPIRE_IRQ_WATCHDOG = 3,
	NSPIRE_IRQ_RTC = 4,
	NSPIRE_IRQ_GPIO = 7,
	NSPIRE_IRQ_OTG = 8,
	NSPIRE_IRQ_HOSTUSB = 9,
	NSPIRE_IRQ_ADC = 11,
	NSPIRE_IRQ_PWR = 15,
	NSPIRE_IRQ_KEYPAD = 16,
	NSPIRE_IRQ_TIMER2 = 19,
	NSPIRE_IRQ_I2C = 20,
	NSPIRE_IRQ_LCD = 21
};

#define NR_IRQS 32

#endif
