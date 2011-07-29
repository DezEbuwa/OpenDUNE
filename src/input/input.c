/* $Id$ */

/** @file src/input/input.c Input routines. */

#include <stdio.h>
#include <string.h>
#include "types.h"
#include "libemu.h"
#include "../global.h"
#include "../os/common.h"
#include "../os/sleep.h"

#include "input.h"

#include "../file.h"
#include "../gfx.h"
#include "../gui/gui.h"
#include "../interrupt.h"
#include "../mouse.h"
#include "../opendune.h"
#include "../timer.h"

static InputLocalData *s_input_local = NULL; /*!< Pointer to input data. */

/**
 * Initialize the input system.
 *
 * @init System_Init_Input
 */
void System_Init_Input()
{
	s_input_local = (InputLocalData *)&emu_get_memory8(0x29E8, 0x0, 0x0);
}

void Input_Init()
{
	emu_ax = 0x3509;
	emu_pushf(); emu_flags.inf = 0; emu_push(emu_cs); emu_cs = 0x0070; emu_push(0x09E2); Interrupt_DOS();
	s_input_local->interruptVector09.s.cs = emu_es;
	s_input_local->interruptVector09.s.ip = emu_bx;

	emu_ds = 0x29E8; emu_dx = 0x0D47;
	emu_ax = 0x2509;
	emu_pushf(); emu_flags.inf = 0; emu_push(emu_cs); emu_cs = 0x0070; emu_push(0x09F8); Interrupt_DOS();
}

void Input_Uninit()
{
	emu_ds = s_input_local->interruptVector09.s.cs;
	emu_dx = s_input_local->interruptVector09.s.ip;
	emu_ax = 0x2509;
	emu_pushf(); emu_flags.inf = 0; emu_push(emu_cs); emu_cs = 0x0070; emu_push(0x0F91); Interrupt_DOS();
}

void Input_EventHandler()
{
	uint8 key;
	uint8 state;
	uint8 i;

	s_input_local->flags = g_global->inputFlags;

	state = 0;
	emu_inb(&key, 0x60); /* Pop key from buffer */

	if (key == 0xE0) {
		s_input_local->extendedKey = 1;
		goto event_exit;
	}

	/* Key up */
	if ((key & 0x80) != 0) {
		key &= 0x7F;
		state |= 0x08;
	}

	if (s_input_local->extendedKey != 0) {
		s_input_local->extendedKey = 0;

		for (i = 0; i < 16; i++) {
			if (s_input_local->translateExtendedMap[i] == key) {
				key = s_input_local->translateMap[i];
				break;
			}
		}
		if (i == 16) goto event_exit;
	} else if (key == 0x7A) {
		key = 0x80;
	} else {
		key = s_input_local->variable_01B9[key & 0x7F];
	}

	if ((s_input_local->activeInputMap[7] & 0x4) != 0) goto event_exit;
	if ((s_input_local->activeInputMap[7] & 0x50) != 0) state |= 0x04;

	key = Input_Keyboard_Translate(key) & 0xFF;

	if ((s_input_local->activeInputMap[7] & 0x2) != 0) state |= 0x01;

	if (state == 0x06 && key == 0x68) goto event_exit;
	if (state == 0x06 && key == 0x4C) goto event_exit;

	Input_HandleInput((state << 8) | key);

	for (i = 0; i < 10; i++) {
		if (s_input_local->keymap_ignore[i] == key) goto event_exit;
	}
	for (i = 0; i < 17; i++) {
		if (s_input_local->variable_0036[i] == key) {
			if ((s_input_local->variable_0058[i] & s_input_local->flags) != 0) goto event_exit;
			break;
		}
	}

event_exit:
	emu_outb(0x20, 0x20);

	/* Return from this function */
	emu_pop(&emu_ip);
	emu_pop(&emu_cs);
	emu_popf();
	return;
}

/**
 * Clears the given bits in input flags.
 *
 * @param bits The bits to clear.
 * @return The new value of input flags.
 */
uint16 Input_Flags_ClearBits(uint16 bits)
{
	g_global->inputFlags &= ~bits;
	return g_global->inputFlags;
}

/**
 * Sets the given bits in input flags.
 *
 * @param bits The bits to set.
 * @return The new value of input flags.
 */
uint16 Input_Flags_SetBits(uint16 bits)
{
	g_global->inputFlags |= bits;

	if ((g_global->inputFlags & INPUT_FLAG_KEY_RELEASE) != 0) {
		uint8 i;
		for (i = 0; i < 16; i++) s_input_local->activeInputMap[i] = 0;
	}

	return g_global->inputFlags;
}

/** Clear the history buffer. */
void Input_History_Clear()
{
	s_input_local->historyTail = s_input_local->historyHead;
}

/**
 * Get an input from the history buffer.
 * @param index Current index in the history.
 * @return Read input.
 * @note Provided \a index gets updated and written to #historyHead
 */
uint16 Input_ReadHistory(uint16 index)
{
	uint16 value;

	value = g_global->variable_7013 = (g_global->mouseMode == INPUT_MOUSE_MODE_PLAY) ? g_global->variable_7013 : s_input_local->history[index / 2];
	index = (index + 2) & 0xFF;

	if ((value & 0xFF) >= 0x41) {
		if ((value & 0xFF) <= 0x42) {
			g_mouseClickX = g_global->variable_7017 = (g_global->mouseMode == INPUT_MOUSE_MODE_PLAY) ? g_global->variable_7017 : s_input_local->history[index / 2];
			index = (index + 2) & 0xFF;

			g_mouseClickY = g_global->variable_7019 = (g_global->mouseMode == INPUT_MOUSE_MODE_PLAY) ? g_global->variable_7019 : s_input_local->history[index / 2];
			index = (index + 2) & 0xFF;
		} else if ((value & 0xFF) <= 0x44) {
			g_global->variable_7017 = (g_global->mouseMode == INPUT_MOUSE_MODE_PLAY) ? g_global->variable_7017 : s_input_local->history[index / 2];
			index = (index + 2) & 0xFF;

			g_global->variable_7019 = (g_global->mouseMode == INPUT_MOUSE_MODE_PLAY) ? g_global->variable_7019 : s_input_local->history[index / 2];
			index = (index + 2) & 0xFF;
		}
	}
	if (g_global->mouseMode != INPUT_MOUSE_MODE_PLAY) s_input_local->historyHead = index;
	return value;
}

/**
 * Translate keyboard input.
 * @param keyValue Entered key value.
 * @return Translated key value.
 */
uint16 Input_Keyboard_Translate(uint16 keyValue)
{
	uint16 i;

	if ((g_global->inputFlags & 0x2) == 0) {
		for (i = 0; i < lengthof(s_input_local->translateMap); i++) {
			if (s_input_local->translateMap[i] == (uint8)(keyValue & 0xFF)) {
				keyValue = s_input_local->translateTo[i] | (keyValue & 0xFF00);
				break;
			}
		}
	}
	return keyValue;
}

/**
 * Add a value to the history buffer.
 * @param value New value to add.
 * @return \c 1 if adding fails, \c 0 if successful.
 */
uint16 Input_History_Add(uint16 value)
{
	uint16 index;

	index = (s_input_local->historyTail + 2) & 0xFF;
	if (index == s_input_local->historyHead) return 1;

	s_input_local->history[s_input_local->historyTail / 2] = value;
	s_input_local->historyTail = index;
	return 0;
}

/**
 * Get input, and add it to the history.
 * @param value Old value.
 * @return Added input value.
 */
uint16 Input_AddHistory(uint16 value)
{
	if (g_global->mouseMode == INPUT_MOUSE_MODE_NORMAL || g_global->mouseMode == INPUT_MOUSE_MODE_RECORD) return value;

	if (g_global->variable_701B != 0) {
		value = 0;
	} else if (g_timerInput < g_global->variable_7015) {
		value = 0;
	} else if (g_global->variable_7013 == 0x2D) {
		Input_ReadInputFromFile();
		value = 0;
	} else {
		value = g_global->variable_7013;
	}

	s_input_local->history[s_input_local->historyHead / 2] = value;
	return value;
}

/** X and Y coordinate. */
typedef struct XY {
	uint16 x; /*!< X coordinate. */
	uint16 y; /*!< Y coordinate. */
} XYPosition;

/**
 * Handle input.
 * @param input New input.
 */
void Input_HandleInput(uint16 input)
{
	uint16 oldTail;
	uint16 saveSize = 0;

	uint16 index;
	uint16 value;
	uint8  bit_value;

	emu_pushf();
	emu_cli();

	s_input_local->flags = g_global->inputFlags;
	s_input_local->mouseX = g_mouseX;
	s_input_local->mouseY = g_mouseY;

	if (g_global->mouseMode == INPUT_MOUSE_MODE_RECORD) {
		saveSize = 4;
		if (g_global->ignoreInput != 0) {
			emu_popf();
			return;
		}
	}

	if (input == 0) {
		emu_popf();
		return;
	}

	value = input & 0xFF;
	if ((s_input_local->flags & 0x1000) != 0 && (input & 0x400) == 0) {

		if (((s_input_local->flags & 0x2000) != 0 && (value == 0x2B || value == 0x3D || value == 0x6C)) || value == 0x63) {
			input = 0x41 | (input & 0xFF00);
			g_prevButtonState |= 1;
			if ((input & 0x800) != 0) {
				g_prevButtonState &= 0xFE; /* ~1 */
			}
		} else if (value == 0x68) {
			input = 0x42 | (input & 0xFF00);
			g_prevButtonState |= 2;
			if ((input & 0x800) != 0) {
				g_prevButtonState &= 0xFD; /* ~2 */
			}
		} else if ((input & 0x800) == 0 && (value == 0x61 || (value >= 0x5B && value <= 0x67 &&
				(value <= 0x5D || value >= 0x65 || value == 0x60 || value == 0x62)))) {

			/* Copied from 29E8:0A9C - 29E8:0AB6 */
			static const int8 data_0A9C[13][2] =
			{
				{-1, -1}, {-1,  0}, {-1,  1}, { 0,  0},
				{ 0,  0}, { 0, -1}, { 0,  0}, { 0,  1},
				{ 0,  0}, { 0,  0}, { 1, -1}, { 1,  0},
				{ 1,  1},
			};
			int8 dx, dy;

			dx = data_0A9C[value - 0x5B][0];
			dy = data_0A9C[value - 0x5B][1];

			if ((input & 0x200) != 0) {
				/* Copied from 29E8:0AB6 - 29E8:0AD6 */
				static const uint16 data_0AB6[16] = {8, 2, 8, 6, 4, 3, 8, 5,  8, 8, 8, 8, 0, 1, 8, 7};
				/* Copied from 29E8:000A - 29E8:002E */
				static const XYPosition mousePos[9] = {
					{0x0a0, 0x000}, {0x13f, 0x000}, {0x13f, 0x045}, {0x13f, 0x089},
					{0x0a0, 0x089}, {0x000, 0x089}, {0x000, 0x045}, {0x000, 0x000},
					{0x0a0, 0x045}
				};
				const XYPosition *xy;

				xy = mousePos + data_0AB6[((dy & 3) << 2) | (dx & 3)];
				s_input_local->mouseX = xy->x;
				s_input_local->mouseY = xy->y;
			} else {
				static const int8 offsetSmall[3] = { -1, 0,  1};
				static const int8 offsetBig[3]   = {-16, 0, 16};
				const int8 *change;

				change = ((input & 0x100) == 0) ? &offsetSmall[1] : &offsetBig[1];

				s_input_local->mouseX += change[dx];
				s_input_local->mouseY += change[dy];
				if (s_input_local->mouseX >= 0x8000) s_input_local->mouseX = 0;
				if (s_input_local->mouseY >= 0x8000) s_input_local->mouseY = 0;
				if ((int16)s_input_local->mouseX > SCREEN_WIDTH - 1)  s_input_local->mouseX = SCREEN_WIDTH - 1;
				if ((int16)s_input_local->mouseY > SCREEN_HEIGHT - 1) s_input_local->mouseY = SCREEN_HEIGHT - 1;
			}

			g_mouseX = s_input_local->mouseX;
			g_mouseY = s_input_local->mouseY;
			if (g_mouseLock == 0) {
				GUI_Mouse_Hide();
				GUI_Mouse_Show();
			}
			input = 0x2D;
		}
	}

	oldTail = s_input_local->historyTail;

	if (Input_History_Add(input) != 0) {
		s_input_local->historyTail = oldTail;
		emu_popf();
		return;
	}

	value = input & 0xFF;
	if (value == 0x2D || value == 0x41 || value == 0x42) {

		if (Input_History_Add(s_input_local->mouseX) != 0) {
			s_input_local->historyTail = oldTail;
			emu_popf();
			return;
		}
		saveSize += 2;

		if (Input_History_Add(s_input_local->mouseY) != 0) {
			s_input_local->historyTail = oldTail;
			emu_popf();
			return;
		}
		saveSize += 2;
	}

	bit_value = 1;
	value = input & 0xFF;
	if (value != 0x2D && value != 0x7F && (input & 0x800) != 0) bit_value = 0;

	if (value == 0x2D || value == 0x7F ||
			((input & 0x800) != 0 && (s_input_local->flags & 0x800) == 0 && value != 0x41 && value != 0x42)) {
		s_input_local->historyTail = oldTail;
	}

	index = (value & 0x7F) >> 3;
	bit_value <<= (value & 7);
	if ((bit_value & s_input_local->activeInputMap[index]) != 0 && (s_input_local->flags & 1) == 0) {
		s_input_local->historyTail = oldTail;
	}
	s_input_local->activeInputMap[index] &= (1 << (value & 7)) ^ 0xFF;
	s_input_local->activeInputMap[index] |= bit_value;

	if (g_global->mouseMode != INPUT_MOUSE_MODE_RECORD || value == 0x7D) {
		emu_popf();
		return;
	}
	s_input_local->variable_0A94 = input;
	s_input_local->variable_0A96 = g_timerInput;

	File_Write(g_global->mouseFileID, &s_input_local->variable_0A94, saveSize);
	g_timerInput = 0;

	emu_popf();
}

/** Read input event from file. */
void Input_ReadInputFromFile()
{
	uint16 value;

	if (g_global->mouseMode == INPUT_MOUSE_MODE_NORMAL || g_global->mouseMode != INPUT_MOUSE_MODE_PLAY) return;

	File_Read(g_global->mouseFileID, s_input_local->variable_063B[0], 4); /* Read failure not translated. */

	g_global->variable_7015 = s_input_local->variable_063B[0][1];
	value = g_global->variable_7013 = s_input_local->variable_063B[0][0];

	if ((value & 0xFF) != 0x2D) {
		uint8 idx, bit;

		idx = (value & 0xFF) >> 3;
		bit = 1 << (value & 7);

		s_input_local->activeInputMap[idx] &= ~bit;
		if ((value & 0x800) == 0) s_input_local->activeInputMap[idx] |= bit;

		if ((value & 0xFF) < 0x41 || (value & 0xFF) > 0x44) {
			g_timerInput = 0;
			return;
		}

		value -= 0x41;
		if ((value & 0xFF) <= 0x2) {
			g_prevButtonState &= ~(1 << (value & 0xFF));
			g_prevButtonState |= (((value & 0x800) >> (3+8)) ^ 1) << (value & 0xFF);
		}
	}

	File_Read(g_global->mouseFileID, s_input_local->variable_063B[1], 4); /* Read failure not translated. */

	g_mouseX = g_global->variable_7017 = s_input_local->variable_063B[1][0];
	value = g_mouseY = g_global->variable_7019 = s_input_local->variable_063B[1][1];

	Mouse_HandleMovementIfMoved(value);
	g_timerInput = 0;
}

/**
 * Is input available?
 * @return \c 0 if no input, else a value.
 */
uint16 Input_IsInputAvailable()
{
	uint16 value;

	emu_pushf();
	emu_cli();
	value = s_input_local->historyHead ^ s_input_local->historyTail;
	emu_popf();

	return Input_AddHistory(value);
}

/**
 * Wait for input, and return the read event.
 * @return New input.
 */
uint16 Input_Wait()
{
	uint16 value = 0;

	for (;;) {
		emu_cli();
		if (g_global->mouseMode == INPUT_MOUSE_MODE_PLAY) break;

		value = s_input_local->historyHead;
		if (value != s_input_local->historyTail) break;

		emu_sti();
		sleep(0); /* Spin-lock */
	}

	value = Input_ReadHistory(value);
	emu_sti();

	Input_ReadInputFromFile();
	return value;
}

/**
 * Test whether \a value is available in the input.
 * @param value Input value.
 * @return \c 0 means not available.
 */
uint16 Input_Test(uint16 value)
{
	Input_AddHistory(value); /* 'value' seems to be the best approximation to emu_ax. */
	value = Input_Keyboard_Translate(value);

	return s_input_local->activeInputMap[value >> 3] & (1 << (value & 7));
}

/**
 * Handle keyboard input.
 * @param value Combined keycode and modifier flags.
 * @return key value and modifier flags.
 * @todo Most users seem to ignore the returned high byte, perhaps make it a
 *      uint8 at some time in the future?
 */
uint16 Input_Keyboard_HandleKeys(uint16 value)
{
	uint8 keyValue;
	uint16 keyFlags;

	keyValue = value & 0x00FF;
	keyFlags = value & 0xFF00;

	if ((keyFlags & 0x8000) != 0 || (keyFlags & 0x800) != 0) {
		return 0;
	}

	s_input_local->flags = 0x1100; /* cs:700E instead of ds:700E (value obtained at run-time). */

	if (keyValue == 0x6E) {
		return keyFlags | 0x1B;
	}

	if (keyValue < 0x3E) {
		uint8 keySave = keyValue & 0x3F;

		if ((keyFlags & 0x100) != 0) {
			keyValue = s_input_local->keymap_shift[keySave];
		} else {
			keyValue = s_input_local->keymap_normal[keySave];
		}

		if ((keyFlags & 0x200) != 0) {
			if ((s_input_local->bitmask[keySave & 7] & s_input_local->keymap_special_mask[keySave >> 3]) != 0) {
				keyValue &= 0x1F;
			}
		}
		return keyFlags | keyValue;
	}

	if (keyValue < 0x4B) {
		if (keyValue >= 0x41) {
			return keyFlags | (keyValue + 0x85);
		}
		return keyFlags | keyValue | 0x80;
	}

	if (keyValue < 0x6E) {
		uint8 keySave = keyValue - 0x4B;

		if ((s_input_local->flags & INPUT_FLAG_UNKNOWN_0200) == 0 && (s_input_local->controlKeys2 & 0x2) != 0) {
			keyValue = s_input_local->keymap_numlock[keySave - 0xF];
		} else {
			keyValue = s_input_local->keymap_numpad[keySave];
		}
		return keyValue;
	}

	if (keyValue < 0x70 || keyValue > 0x79) {
		return keyFlags | keyValue | 0x80;
	}
	keyValue -= 0x70;
	if ((keyFlags & 0x700) != 0) {
		if ((keyFlags & 0x400) == 0) {
			if ((keyFlags & 0x200) == 0) {
				return keyFlags | (0xAC - keyValue);
			}
			return keyFlags | (0xA2 - keyValue);
		}
		return keyFlags | (0x98 - keyValue);
	}
	return keyFlags | (0xC5 - keyValue);
}

/**
 * Wait for valid input.
 * @return Read input.
 */
uint16 Input_WaitForValidInput()
{
	uint16 index = 0;
	uint16 value, i;

	do {
		for (;;) {
			emu_cli();
			if (g_global->mouseMode == INPUT_MOUSE_MODE_PLAY) break;

			index = s_input_local->historyHead;
			if (index != s_input_local->historyTail) break;

			emu_sti();
			sleep(0); /* Spin-lock */
		}

		value = Input_ReadHistory(index);
		emu_sti();
		for (i = 0; i < lengthof(s_input_local->keymap_ignore); i++) {
			if ((value & 0xFF) == s_input_local->keymap_ignore[i]) break;
		}
	} while (i < lengthof(s_input_local->keymap_ignore) || (value & 0x800) != 0 || (value & 0xFF) >= 0x7A);

	value = Input_Keyboard_HandleKeys(value);
	Input_ReadInputFromFile();
	return value & 0xFF;
}

/**
 * Get the next key.
 * @return Next key.
 */
uint16 Input_Keyboard_NextKey()
{
	uint16 i;
	uint16 value;

	emu_pushf();

	Input_AddHistory(0);

	for (;;) {
		uint16 index;

		emu_cli();

		index = s_input_local->historyHead;
		if (g_global->mouseMode != INPUT_MOUSE_MODE_PLAY && index == s_input_local->historyTail) {
			value = 0;
			break;
		}

		value = s_input_local->history[index / 2];
		if (g_global->mouseMode == INPUT_MOUSE_MODE_PLAY && value == 0) break;

		for (i = 0; i < lengthof(s_input_local->keymap_ignore); i++) {
			if (s_input_local->keymap_ignore[i] == (value & 0xFF)) break;
		}

		if (i == lengthof(s_input_local->keymap_ignore) && (value & 0x800) == 0 && (value & 0xFF) < 0x7A) break;

		if ((value & 0xFF) >= 0x41 && (value & 0xFF) <= 0x44) index += 4;

		s_input_local->historyHead = index + 2;

		emu_sti();
		sleep(0); /* Spin-lock */
	}

	s_input_local->controlKeys2 = s_input_local->controlKeys;
	emu_sti();

	if (value != 0) {
		value = Input_Keyboard_HandleKeys(value) & 0xFF;
	}

	emu_popf();
	return value;
}

