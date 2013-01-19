/*
 *	linux/arch/arm/mach-nspire/touchpad.c
 *
 *	Copyright (C) 2012 Fabian Vogt <fabian@ritter-vogt.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/i2c.h>

#include "touchpad.h"

#if defined(CONFIG_MOUSE_SYNAPTICS_I2C) || \
	defined(CONFIG_MOUSE_SYNAPTICS_I2C_MODULE)
static struct i2c_board_info synaptics_i2c = {
	I2C_BOARD_INFO("synaptics_i2c", 0x20),
	.irq    = 0,
};

void __init nspire_touchpad_init()
{
	i2c_register_board_info(0, &synaptics_i2c, 1);
}

#else
inline void nspire_touchpad_init() {}
#endif
