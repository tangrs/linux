/*
 *	linux/arch/arm/mach-nspire/keypad.c
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/input.h>

#include <mach/keypad.h>

unsigned int nspire_touchpad_evtcode_map[][KEYPAD_BITMASK_COLS] = {
	{
		KEY_ENTER,	KEY_ENTER,	0,		0,
		KEY_SPACE,	KEY_Z,		KEY_Y,		KEY_0,
		KEY_TAB,	0,		0,
	},
	{
		KEY_X,		KEY_W,		KEY_V,		KEY_3,
		KEY_U,		KEY_T,		KEY_S,		KEY_1,
		0,		0,		KEY_RIGHT
	},
	{
		KEY_R,		KEY_Q,		KEY_P,		KEY_6,
		KEY_O,		KEY_N,		KEY_M,		KEY_4,
		KEY_APOSTROPHE, KEY_DOWN,	0
	},
	{
		KEY_L,		KEY_K,		KEY_J,		KEY_9,
		KEY_I,		KEY_H,		KEY_G,		KEY_7,
		KEY_SLASH,	KEY_LEFT,	0
	},
	{
		KEY_F,		KEY_E,		KEY_D,		0,
		KEY_C,		KEY_B,		KEY_A,		KEY_EQUAL,
		KEY_KPASTERISK, KEY_UP,		0
	},
	{
		0,		KEY_LEFTALT,	KEY_MINUS,	KEY_RIGHTBRACE,
		KEY_DOT,	KEY_LEFTBRACE,	KEY_5,		0,
		KEY_SEMICOLON,	KEY_BACKSPACE,	KEY_DELETE
	},
	{
		KEY_BACKSLASH,	0,		KEY_KPPLUS,	KEY_PAGEUP,
		KEY_2,		KEY_PAGEDOWN,	KEY_8,		KEY_ESC,
		0,		KEY_TAB,	0
	},
	{
		0,		0,		0,		0,
		0,		0,		0,		0,
		KEY_LEFTSHIFT,	KEY_LEFTCTRL,	KEY_COMMA
	},
};

unsigned int nspire_clickpad_evtcode_map[][KEYPAD_BITMASK_COLS] = {
	{
		KEY_ENTER,	KEY_ENTER,	KEY_SPACE,	0,
		KEY_Z,		KEY_DOT,	KEY_Y,		KEY_0,
		KEY_X,		0,		0,
	},
	{
		KEY_COMMA,	KEY_KPPLUS,	KEY_W,		KEY_3,
		KEY_V,		KEY_2,		KEY_U,		KEY_1,
		KEY_T,		0,		0
	},
	{
		KEY_KPSLASH,	KEY_MINUS,	KEY_S,		KEY_6,
		KEY_R,		KEY_5,		KEY_Q,		KEY_4,
		KEY_P,		0,		0
	},
	{
		KEY_SEMICOLON,	KEY_KPASTERISK, KEY_O,		KEY_9,
		KEY_N,		KEY_8,		KEY_M,		KEY_7,
		KEY_L,		0,		0
	},
	{
		KEY_APOSTROPHE, KEY_SLASH,	KEY_K,		0,
		KEY_J,		0,		KEY_I,		0,
		KEY_H,		0,		0
	},
	{
		KEY_APOSTROPHE, 0,		KEY_G,		KEY_RIGHTBRACE,
		KEY_F,		KEY_LEFTBRACE,	KEY_E,		KEY_DELETE,
		KEY_D,		KEY_LEFTSHIFT,	0
	},
	{
		0,		KEY_ENTER,	KEY_C,		KEY_PAGEUP,
		KEY_B,		KEY_PAGEDOWN,	KEY_A,		KEY_ESC,
		KEY_BACKSLASH,	KEY_TAB,	0
	},
	{
		KEY_UP,		0,		KEY_RIGHT,	0,
		KEY_DOWN,	0,		KEY_LEFT,	KEY_BACKSPACE,
		KEY_LEFTCTRL,	0,		KEY_EQUAL
	},
};
