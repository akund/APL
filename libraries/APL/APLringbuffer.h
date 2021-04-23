/***************************************************************************************************/
/*                                                                                                 */
/* file:          Ringbuffer.h                                                                     */
/*                                                                                                 */
/* source:        2015-2021, written by Adrian Kundert (adrian.kundert@gmail.com)                  */
/*                                                                                                 */
/* description:   Simple ring buffer for a sequenced serial.print()                                */
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

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#ifndef ATMEL_STUDIO
	#include "Arduino.h"
#endif

#pragma GCC optimize ("-O3") // speed optimization

class RingBuffer16 {
public:
    RingBuffer16() {
        Head = Tail = 0;        
    }   
 	
	inline uint8_t count() {
      char Count = Head - Tail;
		if (Count < 0) Count = BufferSize - (Tail - Head);
		return Count;
    }
 	
	bool available() {
		return (Head != Tail) ? true: false;
    } 
	
	uint8_t write(char* data) {
        uint8_t i=0;
		uint8_t h = (Head+1) & (BufferSize-1);
        while((data[i] != 0) && (h != Tail)) {
			Buffer[Head] = data[i]; 
            Head = h;
			h = (Head+1) & (BufferSize-1);
			i++;						
        }
        return i; // return the written count
    }
 	
	uint8_t writeF(const uint8_t* data) {
        uint8_t i=0;
		uint8_t h = (Head+1) & (BufferSize-1);
		char* ptr = (char*)data;
        while((data[i] != 0) && (h != Tail)) {
			Buffer[Head] = pgm_read_byte(ptr++); 
            Head = h;
			h = (Head+1) & (BufferSize-1);
            i++;						
        }
        return i; // return the written count
    }
    
	uint8_t write(char data) {
        uint8_t i=0;
		uint8_t h = (Head+1) & (BufferSize-1);
        if(h != Tail) {  // avoid to full completely for the subtraction Head - Tail
            Buffer[Head] = data;
            Head = h;
		  i++;          
        }
        return i; // return the written count
    }
	
	char read() {
        char retVal = 0;
        if (Tail != Head) {
			retVal = Buffer[Tail];
			Tail = (Tail+1) & (BufferSize-1);
        }        
        return retVal;
    }
 	
	char readfast() {
        char retVal = Buffer[Tail];
		Tail = (Tail+1) & (BufferSize-1);
        return retVal;
	}    
	
	char peek() {
        char retVal = 0;
		if (Tail != Head) {
			retVal = Buffer[Tail];
        }            
        return retVal;
    }

    static const uint8_t BufferSize = 16; // 2^n base required
private:
    volatile uint8_t Head;
    volatile uint8_t Tail;    
    volatile char Buffer[BufferSize];
};

class RingBuffer32 {
public:
    RingBuffer32() {
        Head = Tail = 0;        
    }   
 	
	inline uint8_t count() {
      char Count = Head - Tail;
		if (Count < 0) Count = BufferSize - (Tail - Head);
		return Count;
    }
 	
	bool available() {
		return (Head != Tail) ? true: false;
    } 
	
	uint8_t write(char* data) {
        uint8_t i=0;
		uint8_t h = (Head+1) & (BufferSize-1);
        while((data[i] != 0) && (h != Tail)) {
			Buffer[Head] = data[i]; 
            Head = h;
			h = (Head+1) & (BufferSize-1);
			i++;						
        }
        return i; // return the written count
    }
 	
	uint8_t writeF(const uint8_t* data) {
        uint8_t i=0;
		uint8_t h = (Head+1) & (BufferSize-1);
		char* ptr = (char*)data;
        while((data[i] != 0) && (h != Tail)) {
			Buffer[Head] = pgm_read_byte(ptr++); 
            Head = h;
			h = (Head+1) & (BufferSize-1);
            i++;						
        }
        return i; // return the written count
    }
    
	uint8_t write(char data) {
        uint8_t i=0;
		uint8_t h = (Head+1) & (BufferSize-1);
        if(h != Tail) {  // avoid to full completely for the subtraction Head - Tail
            Buffer[Head] = data;
            Head = h;
			i++;          
        }
        return i; // return the written count
    }
	
	char read() {
        char retVal = 0;
        if (Tail != Head) {
			retVal = Buffer[Tail];
			Tail = (Tail+1) & (BufferSize-1);
        }        
        return retVal;
    }
 	
	char readfast() {
        char retVal = Buffer[Tail];
		Tail = (Tail+1) & (BufferSize-1);
        return retVal;
	}    
	
	char peek() {
        char retVal = 0;
		if (Tail != Head) {
			retVal = Buffer[Tail];
        }            
        return retVal;
    }

    static const uint8_t BufferSize = 32;  // 2^n base required
private:
    volatile uint8_t Head;
    volatile uint8_t Tail;    
    volatile char Buffer[BufferSize];
};

#endif //RINGBUFFER_H


