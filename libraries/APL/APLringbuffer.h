/***************************************************************************************************/
/*                                                                                                 */
/* file:          Ringbuffer.h                                                                     */
/*                                                                                                 */
/* source:        2015-2020, written by Adrian Kundert (adrian.kundert@gmail.com)                  */
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

#include "Arduino.h"

class RingBuffer {
public:
    RingBuffer() {
        Head = Tail = 0;        
    }   
 		inline byte count() {
      char Count = Head - Tail;
			if (Count < 0) Count = BufferSize - (Tail - Head);
			return Count;
    }
 		bool available() {
			return (Head != Tail) ? true: false;
    } 
		byte write(char* data) {
        byte i=0;
				byte h = (Head + 1) % BufferSize;
        while((data[i] != 0) && (h != Tail)) {
						Buffer[Head] = data[i]; 
            Head = h;
						h = (Head + 1) % BufferSize;
						i++;						
        }
        return i; // return the written count
    }
 		byte writeF(const byte* data) {
        byte i=0;
				byte h = (Head + 1) % BufferSize;
				char* ptr = (char*)data;
        while((data[i] != 0) && (h != Tail)) {
						Buffer[Head] = pgm_read_byte(ptr++); 
            Head = h;
						h = (Head + 1) % BufferSize;
            i++;						
        }
        return i; // return the written count
    }
    byte write(char data) {
        byte i=0;
				byte h = (Head + 1) % BufferSize;
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
						Tail = (Tail + 1) % BufferSize;
        }        
        return retVal;
    }
 		char readfast() {
        char retVal = Buffer[Tail];
				Tail = (Tail + 1) % BufferSize;
        return retVal;
		}    
		char peek() {
        char retVal = 0;
				if (Tail != Head) {
						retVal = Buffer[Tail];
        }            
        return retVal;
    }

    static const byte BufferSize = 64;
private:
    volatile byte Head;
    volatile byte Tail;    
    volatile char Buffer[BufferSize];
};

#endif //RINGBUFFER_H


