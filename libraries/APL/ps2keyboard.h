/*
  PS2Keyboard.h - PS2Keyboard library
  Copyright (c) 2007 Free Software Foundation.  All right reserved.
  Written by Christian Weichel <info@32leaves.net>

  ** Mostly rewritten Paul Stoffregen <paul@pjrc.com>, June 2010
  ** Modified for use with Arduino 13 by L. Abraham Smith, <n3bah@microcompdesign.com> * 
  ** Modified for easy interrup pin assignement on method begin(datapin,irq_pin). Cuningan <cuninganreset@gmail.com> **

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

#ifndef ps2keyboard_h
#define ps2keyboard_h

#ifndef ATMEL_STUDIO
	#include "Arduino.h"
#endif

// Every call to read() returns a single byte for each
// keystroke.  These configure what byte will be returned
// for each "special" key.  To ignore a key, use zero.
#define BACKSPACE		8
#define TAB				0
#define ENTER			0x0d
#define ESC				0x03
#define DELETE			0x0c
#define INSERT			0
#define HOME			0
#define END				0
#define PAGEUP			0
#define PAGEDOWN		0
#define UPARROW			0x5e
#define LEFTARROW		0x09
#define DOWNARROW		0x12
#define RIGHTARROW		0x11
#define F1				0x13
#define F2				0
#define F3				0
#define F4				0
#define F5				0
#define F6				0
#define F7				0
#define F8				0
#define F9				0
#define F10				0
#define F11				0
#define F12				0
#define SCROLL			0


#define KEYMAP_SIZE 136

typedef struct {
	unsigned char noshift[KEYMAP_SIZE];
	unsigned char shift[KEYMAP_SIZE];
	unsigned char uses_altgr;
	unsigned char altgr[KEYMAP_SIZE];
} PS2Keymap_t;


/**
 * Purpose: Provides an easy access to PS2 keyboards
 * Author:  Christian Weichel
 */
class PS2Keyboard {
  public:
  	/**
  	 * This constructor does basically nothing. Please call the begin(int,int)
  	 * method before using any other method of this class.
  	 */
    PS2Keyboard();
    
    static void addbit(unsigned char val);
    /**
     * Returns true if there is a char to be read, false if not.
     */
    static bool available();
    
    /**
     * Returns the char last read from the keyboard.
     * If there is no char availble, -1 is returned.
     */
    static int read();
};

#endif
