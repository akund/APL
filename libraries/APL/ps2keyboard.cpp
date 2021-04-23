/*
  PS2Keyboard.cpp - PS2Keyboard library
  Copyright (c) 2007 Free Software Foundation.  All right reserved.
  Written by Christian Weichel <info@32leaves.net>

  ** Mostly rewritten Paul Stoffregen <paul@pjrc.com> 2010, 2011
  ** Modified for use beginning with Arduino 13 by L. Abraham Smith, <n3bah@microcompdesign.com> * 
  ** Modified for easy interrup pin assignement on method begin(datapin,irq_pin). Cuningan <cuninganreset@gmail.com> **

  for more information you can read the original wiki in arduino.cc
  at http://www.arduino.cc/playground/Main/PS2Keyboard
  or http://www.pjrc.com/teensy/td_libs_PS2Keyboard.html

  Version 2.3-Ctrl-Enter (September 2012)
  - Add Ctrl+Enter (Ctrl+J) as PS2_LINEFEED, code 10
  - Move PS2_DOWNARROW to code 12

  Version 2.3-Ctrl (June 2012)
  - Reintroduce Ctrl

  Version 2.3 (October 2011)
  - Minor bugs fixed

  Version 2.2 (August 2011)
  - Support non-US keyboards - thanks to Rainer Bruch for a German keyboard :)

  Version 2.1 (May 2011)
  - timeout to recover from misaligned input
  - compatibility with Arduino "new-extension" branch
  - TODO: send function, proposed by Scott Penrose, scooterda at me dot com

  Version 2.0 (June 2010)
  - Buffering added, many scan codes can be captured without data loss
    if your sketch is busy doing other work
  - Shift keys supported, completely rewritten scan code to ascii
  - Slow linear search replaced with fast indexed table lookups
  - Support for Teensy, Arduino Mega, and Sanguino added

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifdef ATMEL_STUDIO
	#include <avr/pgmspace.h>
#endif
#include "ps2keyboard.h"

#pragma GCC optimize ("-O3") // speed optimization

// ringbuffer is not critical safe. we assume the read out is faster than the max filling speed is 1 char / 60ms
#define BUFFER_SIZE 8
static volatile unsigned char buffer[BUFFER_SIZE];
static volatile unsigned char head, tail;

static unsigned char CharBuffer=0;
static unsigned char UTF8next=0;
static const PS2Keymap_t *keymap=0;

extern const PS2Keymap_t PS2Keymap_SwissGerman;

PS2Keyboard::PS2Keyboard() {
  
  keymap = &PS2Keymap_SwissGerman;
  head = tail = 0;
}

void PS2Keyboard::addbit(unsigned char val)
{
	static uint8_t bitcount=0;
	static uint8_t incoming=0;
	uint8_t n;

	n = bitcount - 1;
	if (n <= 7) {
		//incoming |= (val << n);
		incoming = (incoming >> 1) | ((val != 0) ? 0x80:0);
		//incoming = (incoming << 1) | val; // inverted sequence		
	}
	bitcount++;
	if (bitcount >= 11) {
		unsigned char i = head + 1;
		if (i >= BUFFER_SIZE) i = 0;
		if (i != tail) {
			buffer[i] = incoming;
			head = i;
		}
		bitcount = 0;
		incoming = 0;
	}
}

static inline unsigned char get_scan_code(void)
{
	uint8_t c, i;

	i = tail;
	if (i == head) return 0;
	i++;
	if (i >= BUFFER_SIZE) i = 0;
	c = buffer[i];
	tail = i;
	return c;
	/*i = 0;
	for(uint8_t n=0; n<8; n++) {	// revert the 8 bits
		i = (i << 1) | ((c & 1) ? 1:0);
		c = c >> 1;
	}
	return i;*/	
}

// http://www.quadibloc.com/comp/scan.htm
// http://www.computer-engineering.org/ps2keyboard/scancodes2.html

// These arrays provide a simple key map, to turn scan codes into ISO8859
// output.  If a non-US keyboard is used, these may need to be modified
// for the desired output.
//

const PS2Keymap_t PS2Keymap_US PROGMEM = {
  // without shift
	{0, F9, 0, F5, F3, F1, F2, F12,
	0, F10, F8, F6, F4, TAB, '`', 0,
	0, 0 /*Lalt*/, 0 /*Lshift*/, 0, 0 /*Lctrl*/, 'q', '1', 0,
	0, 0, 'z', 's', 'a', 'w', '2', 0,
	0, 'c', 'x', 'd', 'e', '4', '3', 0,
	0, ' ', 'v', 'f', 't', 'r', '5', 0,
	0, 'n', 'b', 'h', 'g', 'y', '6', 0,
	0, 0, 'm', 'j', 'u', '7', '8', 0,
	0, ',', 'k', 'i', 'o', '0', '9', 0,
	0, '.', '/', 'l', ';', 'p', '-', 0,
	0, 0, '\'', 0, '[', '=', 0, 0,
	0 /*CapsLock*/, 0 /*Rshift*/, ENTER /*Enter*/, ']', 0, '\\', 0, 0,
	0, 0, 0, 0, 0, 0, BACKSPACE, 0,
	0, '1', 0, '4', '7', 0, 0, 0,
	'0', '.', '2', '5', '6', '8', ESC, 0 /*NumLock*/,
	F11, '+', '3', '-', '*', '9', SCROLL, 0,
	0, 0, 0, F7 },
  // with shift
	{0, F9, 0, F5, F3, F1, F2, F12,
	0, F10, F8, F6, F4, TAB, '~', 0,
	0, 0 /*Lalt*/, 0 /*Lshift*/, 0, 0 /*Lctrl*/, 'Q', '!', 0,
	0, 0, 'Z', 'S', 'A', 'W', '@', 0,
	0, 'C', 'X', 'D', 'E', '$', '#', 0,
	0, ' ', 'V', 'F', 'T', 'R', '%', 0,
	0, 'N', 'B', 'H', 'G', 'Y', '^', 0,
	0, 0, 'M', 'J', 'U', '&', '*', 0,
	0, '<', 'K', 'I', 'O', ')', '(', 0,
	0, '>', '?', 'L', ':', 'P', '_', 0,
	0, 0, '"', 0, '{', '+', 0, 0,
	0 /*CapsLock*/, 0 /*Rshift*/, ENTER /*Enter*/, '}', 0, '|', 0, 0,
	0, 0, 0, 0, 0, 0, BACKSPACE, 0,
	0, '1', 0, '4', '7', 0, 0, 0,
	'0', '.', '2', '5', '6', '8', ESC, 0 /*NumLock*/,
	F11, '+', '3', '-', '*', '9', SCROLL, 0,
	0, 0, 0, F7 },
	0
};

const PS2Keymap_t PS2Keymap_German PROGMEM = {
  // without shift
	{0, F9, 0, F5, F3, F1, F2, F12,
	0, F10, F8, F6, F4, TAB, '^', 0,
	0, 0 /*Lalt*/, 0 /*Lshift*/, 0, 0 /*Lctrl*/, 'q', '1', 0,
	0, 0, 'y', 's', 'a', 'w', '2', 0,
	0, 'c', 'x', 'd', 'e', '4', '3', 0,
	0, ' ', 'v', 'f', 't', 'r', '5', 0,
	0, 'n', 'b', 'h', 'g', 'z', '6', 0,
	0, 0, 'm', 'j', 'u', '7', '8', 0,
	0, ',', 'k', 'i', 'o', '0', '9', 0,
	0, '.', '-', 'l', 0, 'p', 0, 0,
	0, 0, 0, 0, 0, '\'', 0, 0,
	0 /*CapsLock*/, 0 /*Rshift*/, ENTER /*Enter*/, '+', 0, '#', 0, 0,
	0, '<', 0, 0, 0, 0, BACKSPACE, 0,
	0, '1', 0, '4', '7', 0, 0, 0,
	'0', '.', '2', '5', '6', '8', ESC, 0 /*NumLock*/,
	F11, '+', '3', '-', '*', '9', SCROLL, 0,
	0, 0, 0, F7 },
  // with shift
	{0, F9, 0, F5, F3, F1, F2, F12,
	0, F10, F8, F6, F4, TAB, 0, 0,
	0, 0 /*Lalt*/, 0 /*Lshift*/, 0, 0 /*Lctrl*/, 'Q', '!', 0,
	0, 0, 'Y', 'S', 'A', 'W', '"', 0,
	0, 'C', 'X', 'D', 'E', '$', 0, 0,
	0, ' ', 'V', 'F', 'T', 'R', '%', 0,
	0, 'N', 'B', 'H', 'G', 'Z', '&', 0,
	0, 0, 'M', 'J', 'U', '/', '(', 0,
	0, ';', 'K', 'I', 'O', '=', ')', 0,
	0, ':', '_', 'L', 0, 'P', '?', 0,
	0, 0, 0, 0, 0, '`', 0, 0,
	0 /*CapsLock*/, 0 /*Rshift*/, ENTER /*Enter*/, '*', 0, '\'', 0, 0,
	0, '>', 0, 0, 0, 0, BACKSPACE, 0,
	0, '1', 0, '4', '7', 0, 0, 0,
	'0', '.', '2', '5', '6', '8', ESC, 0 /*NumLock*/,
	F11, '+', '3', '-', '*', '9', SCROLL, 0,
	0, 0, 0, F7 },
	1,
  // with altgr
	{0, F9, 0, F5, F3, F1, F2, F12,
	0, F10, F8, F6, F4, TAB, 0, 0,
	0, 0 /*Lalt*/, 0 /*Lshift*/, 0, 0 /*Lctrl*/, '@', 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, '{', '[', 0,
	0, 0, 0, 0, 0, '}', ']', 0,
	0, 0, 0, 0, 0, 0, '\\', 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0 /*CapsLock*/, 0 /*Rshift*/, ENTER /*Enter*/, '~', 0, '#', 0, 0,
	0, '|', 0, 0, 0, 0, BACKSPACE, 0,
	0, '1', 0, '4', '7', 0, 0, 0,
	'0', '.', '2', '5', '6', '8', ESC, 0 /*NumLock*/,
	F11, '+', '3', '-', '*', '9', SCROLL, 0,
	0, 0, 0, F7 }
};

const PS2Keymap_t PS2Keymap_SwissGerman PROGMEM = {
  // without shift
	{0, F9, 0, F5, F3, F1, F2, F12,
	0, F10, F8, F6, F4, TAB, 0, 0,
	0, 0 /*Lalt*/, 0 /*Lshift*/, 0, 0 /*Lctrl*/, 'q', '1', 0,
	0, 0, 'y', 's', 'a', 'w', '2', 0,
	0, 'c', 'x', 'd', 'e', '4', '3', 0,
	0, ' ', 'v', 'f', 't', 'r', '5', 0,
	0, 'n', 'b', 'h', 'g', 'z', '6', 0,
	0, 0, 'm', 'j', 'u', '7', '8', 0,
	0, ',', 'k', 'i', 'o', '0', '9', 0,
	0, '.', '-', 'l', 0, 'p', 39, 0,
	0, 0, 0, 0, 0, '^', 0, 0,
	0 /*CapsLock*/, 0 /*Rshift*/, ENTER /*Enter*/, 0, 0, '$', 0, 0,
	0, '<', 0, 0, 0, 0, BACKSPACE, 0,
	0, '1', 0, '4', '7', 0, 0, 0,
	'0', '.', '2', '5', '6', '8', ESC, 0 /*NumLock*/,
	F11, '+', '3', '-', '*', '9', SCROLL, 0,
	0, 0, 0, F7 },
  // with shift
	{0, F9, 0, F5, F3, F1, F2, F12,
	0, F10, F8, F6, F4, TAB, 0, 0,
	0, 0 /*Lalt*/, 0 /*Lshift*/, 0, 0 /*Lctrl*/, 'Q', '+', 0,
	0, 0, 'Y', 'S', 'A', 'W', '"', 0,
	0, 'C', 'X', 'D', 'E', 0, '*', 0,
	0, ' ', 'V', 'F', 'T', 'R', '%', 0,
	0, 'N', 'B', 'H', 'G', 'Z', '&', 0,
	0, 0, 'M', 'J', 'U', '/', '(', 0,
	0, ';', 'K', 'I', 'O', '=', ')', 0,
	0, ':', '_', 'L', 0, 'P', '?', 0,
	0, 0, 0, 0, 0, '`', 0, 0,
	0 /*CapsLock*/, 0 /*Rshift*/, ENTER /*Enter*/, '!', 0, '£', 0, 0,
	0, '>', 0, 0, 0, 0, BACKSPACE, 0,
	0, '1', 0, '4', '7', 0, 0, 0,
	'0', '.', '2', '5', '6', '8', ESC, 0 /*NumLock*/,
	F11, '+', '3', '-', '*', '9', SCROLL, 0,
	0, 0, 0, F7 },
	1,
  // with altgr
	{0, F9, 0, F5, F3, F1, F2, F12,
	0, F10, F8, F6, F4, TAB, 0, 0,
	0, 0 /*Lalt*/, 0 /*Lshift*/, 0, 0 /*Lctrl*/, 0, '¦', 0,
	0, 0, 0, 0, 0, 0, '@', 0,
	0, 0, 0, 0, 0, 0, '#', 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, '|', 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, '{', 0, 0, 0,
	0, 0, 0, 0, '[', '~', 0, 0,
	0 /*CapsLock*/, 0 /*Rshift*/, ENTER /*Enter*/, ']', 0, '}', 0, 0,
	0, 92, 0, 0, 0, 0, BACKSPACE, 0,
	0, '1', 0, '4', '7', 0, 0, 0,
	'0', '.', '2', '5', '6', '8', ESC, 0 /*NumLock*/,
	F11, '+', '3', '-', '*', '9', SCROLL, 0,
	0, 0, 0, F7 }
};



#define BREAK     0x01
#define MODIFIER  0x02
#define SHIFT_L   0x04
#define SHIFT_R   0x08
#define ALTGR     0x10
#define CTRL      0x20
#define CAPSLOCK  0x40

static char get_iso8859_code(void)
{
	static unsigned char state=0;
	unsigned char s;
	char c;

	while (1) {
		s = get_scan_code();
		if (!s) return 0;
		if (s == 0xF0) {
			state |= BREAK;
		} else if (s == 0xE0) {
			state |= MODIFIER;
		} else {
			if (state & BREAK) {
				if (s == 0x12) {
					state &= ~SHIFT_L;
				} else if (s == 0x59) {
					state &= ~SHIFT_R;
				} else if (s == 0x14) {
					state &= ~CTRL;
				} else if (s == 0x11 && (state & MODIFIER)) {
					state &= ~ALTGR;
				}
				// CTRL, ALT & WIN keys could be added
				// but is that really worth the overhead?
				state &= ~(BREAK | MODIFIER);
				continue;
			}
			if (s == 0x58) {
				state ^= CAPSLOCK; // toggle the status
				continue;
			} else if (s == 0x12) {
				state |= SHIFT_L;
				continue;
			} else if (s == 0x59) {
				state |= SHIFT_R;
				continue;
			} else if (s == 0x14) {
				state |= CTRL;
				continue;
			} else if (s == 0x11 && (state & MODIFIER)) {
				state |= ALTGR;
			}
			c = 0;
			if (state & MODIFIER) {
				switch (s) {
				  case 0x70: c = INSERT;      break;
				  case 0x6C: c = HOME;        break;
				  case 0x7D: c = PAGEUP;      break;
				  case 0x71: c = DELETE;      break;
				  case 0x69: c = END;         break;
				  case 0x7A: c = PAGEDOWN;    break;
				  case 0x75: c = UPARROW;     break;
				  case 0x6B: c = LEFTARROW;   break;
				  case 0x72: c = DOWNARROW;   break;
				  case 0x74: c = RIGHTARROW;  break;
				  case 0x4A: c = '/';         break;
				  case 0x5A: c = ENTER;       break;
				  default: break;
				}
			} else if ((state & ALTGR) && keymap->uses_altgr) {
				if (s < KEYMAP_SIZE)
					c = pgm_read_byte(keymap->altgr + s);
			} else if (state & (SHIFT_L | SHIFT_R)) {
				if (s < KEYMAP_SIZE)
					c = pgm_read_byte(keymap->shift + s);
			} else {
				if (s < KEYMAP_SIZE)
					c = pgm_read_byte(keymap->noshift + s);
			}			
			if (state & CAPSLOCK) {
				if (c >= 'a' && c <= 'z') c = c - 'a' + 'A'; // ensure CAPITAL letters
			}
			if (state & CTRL) {
				if (c >= 'A' && c <= 'Z')
					c = c - 'A' + 1;
				else if (c >= 'a' && c <= 'z')
					c = c - 'a' + 1;
			}
			state &= ~(BREAK | MODIFIER);
			if (c) return c;
		}
	}
}

bool PS2Keyboard::available() {
	if (CharBuffer || UTF8next) return true;
	CharBuffer = get_iso8859_code();
	if (CharBuffer) return true;
	return false;
}

int PS2Keyboard::read() {
	unsigned char result;

	result = UTF8next;
	if (result) {
		UTF8next = 0;
	} else {
		result = CharBuffer;
		if (result) {
			CharBuffer = 0;
		} else {
			result = get_iso8859_code();
		}
		if (result >= 128) {
			UTF8next = (result & 0x3F) | 0x80;
			result = ((result >> 6) & 0x1F) | 0xC0;
		}
	}
	if (!result) return -1;
	return result;
}

