
/***************************************************************************************************/
/*                                                                                                 */
/* file:          ps2keyboard.h                                                                    */
/*                                                                                                 */
/* source:        2018-2021, written by Adrian Kundert (adrian.kundert@gmail.com)                  */
/*                                                                                                 */
/* description:   reworked code from https://playground.arduino.cc/Main/PS2Keyboard/               */
/*                                                                                                 */
/* This library is free software; you can redistribute it and/or modify it under the terms of the  */
/* GNU Lesser General Public License as published by the Free Software Foundation;                 */
/* either version 2.1 of the License, or (at your option) any later version.                       */
/*                                                                                                 */
/* This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;       */
/* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.       */
/* See the GNU Lesser General Public License for more details.                                     */
/*                                                                                                 */
/***************************************************************************************************/

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

class PS2Keyboard {
public:
  	/**
  	 * The constructor
  	 */
    PS2Keyboard();
    
    void addbit(unsigned char val);
    
	/**
     * Returns true if there is a char to be read, false if not.
     */
    bool available();
    
    /**
     * Returns the char last read from the keyboard.
     * If there is no char available, -1 is returned.
     */
    int read();
	
private:
	unsigned char get_scan_code();
	char get_iso8859_code();
	
	char *keymap;
	unsigned char state;
	unsigned char CharBuffer;
	unsigned char UTF8next;
};

#endif
