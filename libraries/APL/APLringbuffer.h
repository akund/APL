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
		inline byte available() {
      unsigned char Count = Head - Tail;
			if (Count < 0) Count += BufferSize;
			return (byte)Count;
    }    
		byte write(char* data) {
        byte i=0;
        while((data[i] != NULL) && (available() < BufferSize)) {
            cli(); // begin critical section
						Buffer[Head++] = data[i]; 
            if (Head >= BufferSize) Head = 0; //index reached buffer's end
						sei(); // end critical section
						i++;						
        }
        return i; // return the written count
    }
 		byte writeF(const byte* data) {
        byte i=0;
				char* ptr = (char*)data;
        while((data[i] != NULL) && (available() < BufferSize)) {
            cli(); // begin critical section
						Buffer[Head++] = pgm_read_byte(ptr++); 
            if (Head >= BufferSize) Head = 0; //index reached buffer's end
						sei(); // end critical section
            i++;						
        }
        return i; // return the written count
    }
    byte write(char data) {
        byte i=0;
        if(available() < BufferSize-1) {  // avoid to full completely for the subtraction Head - Tail
            cli(); // begin critical section
            Buffer[Head++] = data;
            if (Head >= BufferSize) Head = 0; //index reached buffer's end    
						sei(); // end critical section
					  i++;          
        }
        return i; // return the written count
    }
     byte writeISR(char data) {
        byte i=0;
        if(available() < BufferSize-1) {  // avoid to full completely for the subtraction Head - Tail
            Buffer[Head++] = data; i++;
            if (Head >= BufferSize) Head = 0; //index reached buffer's end            
        }
        return i; // return the written count
    }
		char read() {
        char retVal = 0;
        if (available() > 0) {
            cli(); // begin critical section
						retVal = Buffer[Tail++];
            if (Tail >= BufferSize) Tail = 0; //index reached buffer's end
						sei(); // end critical section  
        }        
        return retVal;
    }
 		char readISR() {
        char retVal = 0;
        if (available() > 0) {
            retVal = Buffer[Tail++];
            if (Tail >= BufferSize) Tail = 0; //index reached buffer's end            
        }        
        return retVal;
		}
    char peek() {
        char retVal = 0;
        if (available() > 0) {
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


