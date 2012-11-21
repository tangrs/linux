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

unsigned int nspire_touchpad_evtcode_map[][11] = {
	{ KEY_ENTER, KEY_ENTER, 0, 0, KEY_SPACE, KEY_Z, KEY_Y, KEY_0, KEY_TAB, 0, 0 },
	{ KEY_X, KEY_W, KEY_V, KEY_3, KEY_U, KEY_T, KEY_S, KEY_1, 0, 0, KEY_RIGHT },
	{ KEY_R, KEY_Q, KEY_P, KEY_6, KEY_O, KEY_N, KEY_M, KEY_4, KEY_APOSTROPHE, KEY_DOWN, 0 },
	{ KEY_L, KEY_K, KEY_J, KEY_9, KEY_I, KEY_H, KEY_G, KEY_7, KEY_SLASH, KEY_LEFT, 0 },
	{ KEY_F, KEY_E, KEY_D, 0, KEY_C, KEY_B, KEY_A, KEY_EQUAL, KEY_KPASTERISK, KEY_UP, 0},
	{ 0, KEY_LEFTALT, KEY_MINUS, KEY_RIGHTBRACE, KEY_DOT, KEY_LEFTBRACE, KEY_5, 0, KEY_SEMICOLON, KEY_BACKSPACE, KEY_DELETE },
	{ KEY_BACKSLASH, 0, KEY_KPPLUS, KEY_PAGEUP, KEY_2, KEY_PAGEDOWN, KEY_8, KEY_ESC, 0, KEY_TAB, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_COMMA }
};
