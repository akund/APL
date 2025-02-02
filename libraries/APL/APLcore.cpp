/***************************************************************************************************/
/*                                                                                                 */
/* file:          APLcore.cpp                                                                      */
/*                                                                                                 */
/* source:        2018-2025, written by Adrian Kundert (adrian.kundert@gmail.com)                  */
/*                                                                                                 */
/* description:   APL interrupt driven engine for VGA, Audio, UART and PS2 peripherals             */
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

#include "APLcore.h"
#include "APLfont.h"

#ifdef ATMEL_STUDIO
	#define NULL 0
	#define bit(b) (1UL << (b))
#endif

#pragma GCC optimize ("-O3") // speed optimization

//================================ ISR variables (begin) ==========================================//
// screen and buffer allocations
const int srcBufSize = (int)scrBufWidthInTile * (int)(scrBufHeightInTile); // one tile row more required for y scrolling in graph mode
volatile uint8_t* scrBuf[srcBufSize];	// non-atomic shared variable
const uint8_t NONE=0, UPDATE=1, S_LEFT=2, S_RIGHT=3, S_UP=4, S_DOWN=5;
volatile uint8_t TileNext = NONE;		// semaphore for scrBuf[] update
volatile uint8_t* TilePtrNext;			
volatile unsigned int newTileIndexNext;
const unsigned int PGM_MARKER = 0x8000;

// VGA rendering variables
const uint8_t verticalBackPorchLines = 35;  // includes 2 sync pulses  
const unsigned int activeLines = 480;  // 480 pixels high
const uint8_t verticalFrontPorchLines = 10;  
const unsigned int totalLines = verticalBackPorchLines + activeLines + verticalFrontPorchLines;  // 525 lines totally
volatile uint8_t cursorMutex = 0;				// mutex for cursorTileIndex
volatile unsigned int cursorTileIndex = 0xffff;	// non-atomic shared variable
volatile uint8_t* cursorOnTile = NULL;
volatile uint8_t* cursorOffTile = NULL;
volatile uint8_t xScroll = 0, yScroll = 0; // x scrolling, value between 0 and 6, y between 0 and 7
volatile uint8_t TileScroll = 0;
volatile uint8_t VGAmode;
volatile uint8_t MemWidth;
volatile uint8_t pixLine = 0;

// Audio variable
volatile uint8_t soundMutex = 0;			// mutex for soundbufptr and BASIC_duration
volatile uint8_t* soundbufptr = NULL;		// non-atomic shared variable
volatile uint8_t BASIC_tone = 0;			// atomic shared variable
volatile unsigned int BASIC_duration = 0;	// non-atomic shared variable

// PS2 keyboard instantiation
PS2Keyboard kbd;
		
// UART ringbuffer (DO NOT USE ANY OTHER UART LIB TO AVOID INTERRUPT CONFLICT)
RingBuffer16 txbuffer;	// atomic queue
RingBuffer32 rxbuffer;	// atomic queue

volatile unsigned long ElaspsedTime = 0; // 32-bit counter in ms since last reset
volatile unsigned char dateY, dateM, dateD, timeH, timeM, timeS, tick;
//================================ Hardware Config (end) ==========================================//

#ifdef PIXEL_HW_MUX
inline void VGArendering() {
	// the tile line is repeated HeightScaling
	register unsigned int TileIndex = (pixLine / TileMemHeight) * (unsigned int)scrBufWidthInTile;
	register uint8_t TilePixOffset = (pixLine & (TileMemHeight-1)) * MemWidth;

	// blit pixel data to screen in HW mux (8 clock / 2 pix)
	asm volatile (
		// save used registers (the compiler does not consider the code below)
		"push r0 \n\t" "push r1 \n\t"  "push r15 \n\t" "push r16 \n\t" "push r17 \n\t"

		// re-assign for controlled behavior (could be optimized out)
		"mov r15, %1 \n\t"
		"mov r16, %2 \n\t"
		"mov r17, %3 \n\t"
		"clr r1 \n\t"
		"cpi r16, 3 \n\t" "brne TEXT_MODE \n\t" "rjmp GRAPH_MODE \n\t"

		//---------------------------- TEXT MODE: render the ascii character without x scrolling -----------------------------------------------------//
		"TEXT_MODE: \n\t"
		"cpi r16, 1 \n\t" "breq TEXT_MODE_1 \n\t" "rjmp PGM_RAM_end8 \n\t" // exit when mode disabled
		"TEXT_MODE_1: \n\t"
		#if F_CPU == 32000000UL
		".rept 15  \n\t" // porch delay
			"nop     \n\t"
		".endr     \n\t"
		#endif
	
		"sbi 0x05, 0 \n\t"                            // 2      the SBI enables the pixelMux, critical timing set just before the out instruction or 8*n cycles earlier
		"ld ZL, X+ \n\t" "ld ZH, X+ \n\t"             // 2+2    load the value end of the line
		"nop \n\t"                                    // 1
		"add ZL, r15  \n\t" "adc ZH, r1 \n\t"         // 1+1
	
		#if F_CPU == 32000000UL
		".rept 29 \n\t"
		#elif F_CPU == 24000000UL
		".rept 20 \n\t"
		#elif F_CPU == 20000000UL
		".rept ?? \n\t"
		#else // Arduino default 16 MHz
		".rept 9 \n\t"
		#endif
										  "lpm r0, Z+ \n\t"                          "nop \n\t"       "nop \n\t"         // 3+1+1
			"out 0x0b, r0 \n\t" "lpm r0, Z+ \n\t" "ld YL, X+ \n\t"                   "ld YH, X+ \n\t"                    // 1+3+2+2
			"out 0x0b, r0 \n\t" "lpm r0, Z+ \n\t" "nop \n\t"     "add YL, r15  \n\t" "adc YH,r1\n\t"  "movw Z,Y \n\t"    // 1+3+1+1+1+1
			"out 0x0b, r0 \n\t" "nop \n\t" "nop \n\t"                                                                    // 1+1+1
		".endr    \n\t"
														"rjmp PGM_RAM_end5 \n\t"

		//------------------------------ GRAPH MODE-----------------------------------------------------------------------------------------------------------------//
		"GRAPH_MODE: \n\t"
		//------------------------------ render the Tiles with x scrolling -----------------------------------------------------//  the first out instruction comes after 25 cycles
		"ld ZL, X+ \n\t" "ld ZH, X+ \n\t" "add ZL, r15  \n\t" "adc ZH, r1 \n\t"
		"ld YL, X+ \n\t" "ld YH, X+ \n\t" "add YL, r15  \n\t" "adc YH,r1\n\t"
		"adiw Z,1 \n\t"				// default is xscroll=2
		"tst ZH \n\t"
		"sbi 0x05, 0 \n\t"			// enables the pixelMux, critical timing set just before the out instruction or 8*n cycles earlier
		"brpl RAM0_6 \n\t" "nop \n\t"																													// 1(2)+1
		//------------------------------------ first part tile ----------------------------------------
		"PGM0_6: \n\t" "cpi r17, 6 \n\t" "brlo PGM0_4 \n\t" "adiw Z,2 \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "rjmp Psc6 \n\t"		//1+1(2)+1+1+1+1+1+1+2
		"PGM0_4: \n\t" "cpi r17, 4 \n\t" "brlo PGM0_2 \n\t" "adiw Z,1 \n\t" "nop \n\t" "nop \n\t" "rjmp Psc4 \n\t"  									//1+1(2)+1+2
		"PGM0_2: \n\t" "cpi r17, 2 \n\t" "brlo PGM0_0 \n\t" "rjmp Psc2 \n\t"																			//1+1+2
		"PGM0_0: \n\t" "sbiw Z,1 \n\t"																													//1
	
		"Psc0: \n\t" "lpm r0, Z+ \n\t" "nop \n\t" "nop \n\t"      "nop \n\t" "nop \n\t"    "out 0x0b, r0 \n\t" // 3+1+1+1+1+1
		"Psc2: \n\t" "lpm r0, Z+ \n\t" "nop \n\t" "nop \n\t"      "nop \n\t" "nop \n\t"    "out 0x0b, r0 \n\t" // 3+1+1+1+1+1
		"Psc4: \n\t" "lpm r0, Z+ \n\t" "nop \n\t" "nop \n\t"      "nop \n\t" "nop \n\t"    "out 0x0b, r0 \n\t" // 3+1+1+1+1+1
		"Psc6: \n\t" "lpm r0, Z+ \n\t" "nop \n\t" "movw Z,Y \n\t" "nop \n\t" "tst YH \n\t" "out 0x0b, r0 \n\t" // 3+1+1+1+1+1
		"brmi .+100 \n\t" "rjmp .+142 \n\t"                                                        			   // 1(2)+2

		"RAM0_6: \n\t" "cpi r17, 6 \n\t" "brlo RAM0_4 \n\t" "adiw Z,2 \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "rjmp Rsc6 \n\t"		//1+1(2)+1+1+1+1+1+1+2
		"RAM0_4: \n\t" "cpi r17, 4 \n\t" "brlo RAM0_2 \n\t" "adiw Z,1 \n\t" "nop \n\t" "nop \n\t" "rjmp Rsc4 \n\t"										//1+1(2)+1+2
		"RAM0_2: \n\t" "cpi r17, 2 \n\t" "brlo RAM0_0 \n\t" "rjmp Rsc2 \n\t"																			//1+1+2
		"RAM0_0: \n\t" "sbiw Z,1 \n\t"																													//1
	
		"Rsc0: \n\t" "ld r0, Z+ \n\t" "nop \n\t" "nop \n\t" "nop \n\t"      "nop \n\t" "nop \n\t"    "out 0x0b, r0 \n\t"	// 2+1+1+1+1+1+1
		"Rsc2: \n\t" "ld r0, Z+ \n\t" "nop \n\t" "nop \n\t" "nop \n\t"      "nop \n\t" "nop \n\t"    "out 0x0b, r0 \n\t"	// 2+1+1+1+1+1+1
		"Rsc4: \n\t" "ld r0, Z+ \n\t" "nop \n\t" "nop \n\t" "nop \n\t"      "nop \n\t" "nop \n\t"    "out 0x0b, r0 \n\t"	// 2+1+1+1+1+1+1
		"Rsc6: \n\t" "ld r0, Z+ \n\t" "nop \n\t" "nop \n\t" "movw Z,Y \n\t" "nop \n\t" "tst YH \n\t" "out 0x0b, r0 \n\t"	// 2+1+2+1+1+1
		"brmi .+2 \n\t" "rjmp .+44 \n\t"																 					// 1(2)+2
	
		//------------------------------ render the Tiles after the x scrolling -----------------------------------------------------//
		#if F_CPU == 32000000UL
		".rept 21-1 \n\t" // tile below and scrolled tile subtracted
		#elif F_CPU == 24000000UL
		".rept 14-1 \n\t" // tile below and scrolled tile subtracted
		#elif F_CPU == 20000000UL
		".rept ?? \n\t" // tile below and scrolled tile subtracted
		#else   // Arduino default 16 MHz
		".rept 6-1 \n\t"
		#endif
			//"PGM_n: \n\t" // (code size 22 words)
												"lpm r0, Z+ \n\t"				  "nop \n\t"           "nop \n\t"        //     3+1+1
			"out 0x0b, r0 \n\t" "lpm r0, Z+ \n\t" "ld YL, X+ \n\t"                "nop \n\t"           "nop \n\t"        // 1+3+2+1+1
			"out 0x0b, r0 \n\t" "lpm r0, Z+ \n\t" "ld YH, X+ \n\t"                "add YL, r15  \n\t"  "adc YH,r1\n\t"   // 1+3+2+1+1
			"out 0x0b, r0 \n\t" "lpm r0, Z+ \n\t" "movw Z,Y \n\t" "tst YH \n\t"   "nop \n\t"		   "nop \n\t"		 // 1+3+1+1+1+1
			"out 0x0b, r0 \n\t" "brmi .+52 \n\t" "rjmp .+94 \n\t"                                                        // 1+1(2)+2

			//"RAM_n: \n\t" // (code size 25 words)
														"ld r0, Z+ \n\t"                "nop \n\t"           "nop \n\t"        //       2+1+1
			"out 0x0b, r0 \n\t" "ld r0, Z+ \n\t" "nop \n\t"  "ld YL, X+ \n\t"           "nop \n\t"           "nop \n\t"        // 1+2+1+2+1+1
			"out 0x0b, r0 \n\t" "ld r0, Z+ \n\t" "nop \n\t"  "ld YH, X+ \n\t"           "add YL, r15  \n\t"  "adc YH,r1 \n\t"  // 1+2+1+2+1+1+
			"out 0x0b, r0 \n\t" "ld r0, Z+ \n\t" "movw Z,Y \n\t" "nop \n\t" "tst YH \n\t" "nop \n\t"		 "nop \n\t"		   // 1+2+1+1+1+1+1
			"out 0x0b, r0 \n\t" "nop \n\t" "brpl .+44 \n\t"                                                                    // 1+1+1(2)
		".endr    \n\t"

		//"PGMlast: \n\t" // (code size 22 words)
												"cpi r17,0 \n\t" "breq PGM_RAM_end6 \n\t"   "lpm r0, Z+ \n\t"        				//       1+1(2)+3
		"out 0x0b, r0 \n\t" "lpm r0, Z+ \n\t"                   "nop \n\t" "cpi r17,2 \n\t" "breq PGM_RAM_end8 \n\t" "nop \n\t" 	// 1+3+1+1+1(2)+1
		"out 0x0b, r0 \n\t" "lpm r0, Z+ \n\t"                   "nop \n\t" "cpi r17,4 \n\t" "breq PGM_RAM_end8 \n\t" "nop \n\t" 	// 1+3+1+1+1(2)+1
		"out 0x0b, r0 \n\t" "nop \n\t" "rjmp PGM_RAM_end4 \n\t"            												   		// 1+1+2

		//"RAMlast: \n\t"  // (code size 25 words)
											"cpi r17,0 \n\t" "breq PGM_RAM_end7 \n\t"    "ld r0, Z+ \n\t"							//         1+1(2)+2
		"out 0x0b, r0 \n\t" "ld r0, Z+ \n\t"      "nop \n\t" "nop \n\t" "cpi r17,2 \n\t" "breq PGM_RAM_end8 \n\t" "nop \n\t"		// 1+2+1+1+1+1(2)+1
		"out 0x0b, r0 \n\t" "ld r0, Z+ \n\t"      "nop \n\t" "nop \n\t" "cpi r17,4 \n\t" "breq PGM_RAM_end8 \n\t" "nop \n\t"		// 1+2+1+1+1+1(2)+1
		"out 0x0b, r0 \n\t" "nop \n\t" "nop \n\t" "nop \n\t"																	// 1+1+1+1

		//-------------------------------------------------------------------------------------------------------------------------------//
		// complete the 8 cycles until the out instruction
		"PGM_RAM_end4: \n\t"
		"nop \n\t"
		"PGM_RAM_end5: \n\t"
		"nop \n\t"
		"PGM_RAM_end6: \n\t"
		"nop \n\t"
		"PGM_RAM_end7: \n\t"
		"nop \n\t"
		"PGM_RAM_end8: \n\t"
		"out 0x0b, r1 \n\t"      // back to black
		"cbi 0x05, 0 \n\t"       // disable pixelMux
		// restore unsaved registers
		"pop r17 \n\t" "pop r16 \n\t" "pop r15 \n\t" "pop r1 \n\t" "pop r0 \n\t"
		:
		:  "x" (&scrBuf[TileIndex]), "r" (TilePixOffset), "r" (VGAmode), "r" (xScroll)
		// gcc assignation        x,                 r15,           r16,          r17,	    // ensure the assigned register by the compiler are not overwritten by the user code
		: "r31", "r30", "r29", "r28"														// specify to the compiler the used registers not explicitly taken as parameter
	);
}
#else
inline void VGArendering() {
	// the tile line is repeated HeightScaling
	register unsigned int TileIndex = (pixLine / TileMemHeight) * (unsigned int)scrBufWidthInTile;
	register uint8_t TilePixOffset = (pixLine & (TileMemHeight-1)) * MemWidth;

	// blit pixel data to screen without pixel HW mux (4 clock / pix)
	asm volatile (
		// save used registers (the compiler does not consider the code below)
		"push r0 \n\t" "push r1 \n\t" "push r2 \n\t" "push r15 \n\t" "push r16 \n\t"  "push r17 \n\t"
		
		// re-assign for controlled behavior (could be optimized out)
		"mov r15, %1 \n\t"
		"mov r16, %2 \n\t"
		"mov r17, %3 \n\t"
		"cpi r16, 3 \n\t" "breq GRAPH_MODE \n\t" "rjmp NEXT_MODE \n\t"
				
		//-- 4clk/pix (4 colors only)---------------------------- GRAPH MODE: render the Tiles WITH x scrolling -----------------------------------------------------//
		"GRAPH_MODE: \n\t"
		"ldi r16, 4 \n\t" "mov r2, r16 \n\t" "clr r16 \n\t"
		
		// xscrolling calculation
		//#ifdef NO_XSCROLLING
		//"clr r17 \n\t" "rjmp BEGIN \n\t" // add this line to remove the xscrolling and increase the tile resolution
		//#endif
		"movw Y,X \n\t"		// do a copy
		
		#if F_CPU == 32000000UL
			"adiw Y,34 \n\t"	// move to the end + (2*scrViewWidthInTileGRAPH)
		#elif F_CPU == 24000000UL
			"adiw Y,22 \n\t"	// move to the end + (2*scrViewWidthInTileGRAPH)
		#elif F_CPU == 20000000UL
			"adiw Y,?? \n\t"	// move to the end + (2*scrViewWidthInTileGRAPH)
		#else   // Arduino default 16 MHz
			"adiw Y,8 \n\t"	// move to the end + (2*scrViewWidthInTileGRAPH)
		#endif		
		"ld ZL, Y+ \n\t" "ld ZH, Y+ \n\t" "add ZL, r15 \n\t" "adc ZH, r16 \n\t"
		"tst ZH \n\t"
		"brmi PGM6 \n\t" "nop \n\t"														//1(2)+1
				
		"RAM6: \n\t" "cpi r17, 5 \n\t" "brlo RAM4 \n\t" "ld r0, Z+ \n\t" "nop \n\t" "ld r1, Z+ \n\t" "nop \n\t" "push r1 \n\t" "push r0 \n\t" "rjmp BEGIN \n\t" 	//1+1(2)+3+1+3+1+2	save the last value
		"RAM4: \n\t" "cpi r17, 0 \n\t" "breq RAM0 \n\t" "nop \n\t" "ld r0, Z+ \n\t" "nop \n\t" "push r0 \n\t" "rjmp BEGIN \n\t" 									//1+1(2)+1+3+1+2	save the last-1 value
		"RAM0: \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "nop \n\t"  "rjmp BEGIN \n\t"
		
		"PGM6: \n\t" "cpi r17, 5 \n\t" "brlo PGM4 \n\t" "lpm r0, Z+ \n\t" "lpm r1, Z \n\t" "push r1 \n\t" "push r0 \n\t" "rjmp BEGIN \n\t" 	//1+1(2)+3+1+3+1+2	save the last value
		"PGM4: \n\t" "cpi r17, 0 \n\t" "breq PGM0 \n\t" "nop \n\t" "lpm r0, Z \n\t" "push r0 \n\t" "rjmp BEGIN \n\t" 						//1+1(2)+1+3+1+2	save the last-1 value
		"PGM0: \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "nop \n\t"  "nop \n\t" "nop \n\t"

		"BEGIN: \n\t" 
		"ld ZL, X+ \n\t" "ld ZH, X+ \n\t" "add ZL, r15  \n\t" "adc ZH, r16 \n\t"
		"ld YL, X+ \n\t" "ld YH, X+ \n\t" "add YL, r15  \n\t" "adc YH, r16\n\t"		
		"tst ZH \n\t" "brpl RAM0_6 \n\t"

		//------------------------------------ first part tile ----------------------------------------
					   "lpm r0, Z+ \n\t" // pre-load out1, inc for default is xscroll=4
		"PGM0_6: \n\t" "cpi r17, 6 \n\t" "brlo PGM0_4 \n\t" "lpm r0, Z+ \n\t" "mul r0,r2 \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "rjmp Psc6 \n\t"		//1+1(2)+1+1+1+1+1+1+2
		"PGM0_4: \n\t" "cpi r17, 4 \n\t" "brlo PGM0_2 \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "rjmp Psc4 \n\t"  								//1+1(2)+1+2
		"PGM0_2: \n\t" "cpi r17, 2 \n\t" "brlo PGM0_0 \n\t" "mul r0,r2 \n\t" "rjmp Psc2 \n\t"															//1+1+2
		"PGM0_0: \n\t" "nop \n\t" "nop \n\t" 																											//1+1
		
		"Psc0: \n\t" "nop \n\t"	      "nop \n\t"      "nop \n\t"	"out 0x0b, r0 \n\t" // 3+1
					 "mul r0,r2 \n\t" "nop \n\t"	  "nop \n\t"    "out 0x0b, r0 \n\t" // 1+1+1+1
		"Psc2: \n\t" "mul r0,r2 \n\t" "nop \n\t"      "nop \n\t"    "out 0x0b, r0 \n\t" // 1+1+1+1
					 "mul r0,r2 \n\t" "nop \n\t"	  "nop \n\t"    "out 0x0b, r0 \n\t" // 1+1+1+1
		"Psc4: \n\t" "lpm r0, Z+ \n\t"								"out 0x0b, r0 \n\t" // 3+1
					 "mul r0,r2 \n\t" "nop \n\t"	  "nop \n\t"    "out 0x0b, r0 \n\t" // 1+1+1+1
		"Psc6: \n\t" "mul r0,r2 \n\t" "nop \n\t"	  "nop \n\t"    "out 0x0b, r0 \n\t" // 1+1+1+1
					 "mul r0,r2 \n\t" "movw Z,Y \n\t" "tst YH \n\t" "out 0x0b, r0 \n\t" // 1+1+1+1
					 "brmi .+122 \n\t" "rjmp .+172 \n\t"                   			    // 1(2)+2

		"RAM0_6: \n\t" "ld r0, Z+ \n\t" // pre-load out1, inc for default is xscroll=4
					   "cpi r17, 6 \n\t" "brlo RAM0_4 \n\t" "ld r0, Z+ \n\t" "mul r0,r2 \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "rjmp Rsc6 \n\t"		//1+1(2)+1+1+1+1+1+1+2
		"RAM0_4: \n\t" "cpi r17, 4 \n\t" "brlo RAM0_2 \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "nop \n\t" "rjmp Rsc4 \n\t"										//1+1(2)+1+2
		"RAM0_2: \n\t" "cpi r17, 2 \n\t" "brlo RAM0_0 \n\t" "mul r0,r2 \n\t" "rjmp Rsc2 \n\t"																	//1+1+2
		"RAM0_0: \n\t" "nop \n\t" "nop \n\t"																													//1
		
		"Rsc0: \n\t" "ld r0, Z+ \n\t"				  "nop \n\t"    "out 0x0b, r0 \n\t"	// 2+1+1
					 "mul r0,r2 \n\t" "nop \n\t"	  "nop \n\t"    "out 0x0b, r0 \n\t" // 1+1+1+1
		"Rsc2: \n\t" "mul r0,r2 \n\t" "nop \n\t"      "nop \n\t"    "out 0x0b, r0 \n\t"	// 1+1+1+1
					 "mul r0,r2 \n\t" "nop \n\t"	  "nop \n\t"    "out 0x0b, r0 \n\t" // 1+1+1+1
		"Rsc4: \n\t" "ld r0, Z+ \n\t"				  "nop \n\t"    "out 0x0b, r0 \n\t"	// 2+1+1
					 "mul r0,r2 \n\t" "nop \n\t"	  "nop \n\t"    "out 0x0b, r0 \n\t" // 1+1+1+1
		"Rsc6: \n\t" "mul r0,r2 \n\t" "nop \n\t"	  "nop \n\t"    "out 0x0b, r0 \n\t"	// 1+1+1+1
					 "mul r0,r2 \n\t" "movw Z,Y \n\t" "tst YH \n\t" "out 0x0b, r0 \n\t" // 1+1+1+1
					 "brmi .+12 \n\t" "rjmp .+62 \n\t"				 					// 1(2)+2

		// max value allowed is scrBufWidthInTile
		#if F_CPU == 32000000UL
		".rept 17-1 \n\t"
		#elif F_CPU == 24000000UL
		".rept 11-1 \n\t"
		#elif F_CPU == 20000000UL
		".rept ?? \n\t"
		#else   // Arduino default 16 MHz
		".rept 4-1 \n\t"
		#endif
		
			//--------------------- PGM source (code size 29 words) -------------------------
			//"PGM_1: \n\t"
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "nop \n\t"			"nop \n\t"			// out7 1+1+1+1
			"out 0x0b, r0 \n\t"															// out8 1
			//"PGM_entry: \n\t"
								"lpm r0, Z+ \n\t"                   					//      3
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "ld YL, X+ \n\t" 						// out1 1+1+2
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "ld YH, X+ \n\t"      					// out2 1+1+2
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "add YL,r15 \n\t"	"adc YH, r16 \n\t"	// out3 1+1+1+1
			"out 0x0b, r0 \n\t" "lpm r0, Z+ \n\t"  										// out4 1+3
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "movw Z,Y \n\t"	"tst ZH \n\t"		// out5 1+1+1+1
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "brmi .+62 \n\t"	"nop \n\t"			// out6 1+1+1(2)+1
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "rjmp .+112 \n\t"						// out7	1+1+2
		
			//--------------------- RAM source (code size 27 words) ------------------------
		
			//"RAM_1: \n\t"
			"out 0x0b, r0 \n\t"	"nop \n\t"												// out8 1+1
			//"RAM_entry: \n\t"
			"ld r0, Z+ \n\t"						//		  2
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "ld YL, X+ \n\t"						// out1 1+1+2
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "ld YH, X+ \n\t"      					// out2 1+1+2
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "add YL,r15 \n\t"  "adc YH, r16 \n\t"	// out3 1+1+1+1
			"out 0x0b, r0 \n\t" "ld r0, Z+ \n\t"  					"nop \n\t"			// out4 1+2+1
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "movw Z,Y \n\t"	"tst ZH \n\t"		// out5 1+1+1+1
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "brmi .+8 \n\t"	"nop \n\t"			// out6 1+1+1(2)+1
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "rjmp .+58 \n\t"						// out7 1+1+2
		".endr    \n\t"
		
		//--------------------- PGM source (code size 29 words) -------------------------
		//"PGM_last: \n\t"
		"out 0x0b, r0 \n\t" "nop \n\t"		 "nop \n\t"				 "mul r0,r2 \n\t"	// out7 1+1+1+1
		"out 0x0b, r0 \n\t"	"cpi r17,0 \n\t" "breq PGM_RAM_end \n\t" "pop r0 \n\t"		// out8 1+1+1(2)+1
		"out 0x0b, r0 \n\t" "nop \n\t"		 "nop \n\t"				 "mul r0,r2 \n\t"	// out1 1+1+1+1
		"PGM_last_: \n\t"
		"out 0x0b, r0 \n\t" "cpi r17,2 \n\t" "breq PGM_RAM_end \n\t" "mul r0,r2 \n\t"	// out2 1+1+1(2)+1
		"out 0x0b, r0 \n\t" "nop \n\t"		 "nop \n\t"				 "mul r0,r2 \n\t"	// out3 1+1+1+1
		"out 0x0b, r0 \n\t" "cpi r17,4 \n\t" "breq PGM_RAM_end \n\t" "pop r0 \n\t"		// out4 1+1+1(2)+1
		"out 0x0b, r0 \n\t" "nop \n\t"		 "nop \n\t"				 "mul r0,r2 \n\t"	// out5 1+1+1+1
		"out 0x0b, r0 \n\t" "nop \n\t"		 "rjmp PGM_RAM_end \n\t"					// out6 1+1+2
		
		//--------------------- RAM source (code size 27 words) ------------------------
		//"RAM_last: \n\t"
		"out 0x0b, r0 \n\t"	"cpi r17,0 \n\t" "breq PGM_RAM_end \n\t" "pop r0 \n\t"		// out8 1+1+1(2)+1
		"out 0x0b, r0 \n\t" "mul r0,r2 \n\t" "rjmp PGM_last_ \n\t"						// out1 1+1+2		
		"PGM_RAM_end: \n\t"
		"out 0x0b, r16 \n\t"      // back to black
		"rjmp _end \n\t"
		
		"NEXT_MODE: \n\t"		
		"cpi r16, 0 \n\t" "breq PGM_RAM_end \n\t" // exit when mode disabled
		#if F_CPU == 32000000UL
		".rept 15  \n\t" // porch delay for GRAPH_PGM and TEXT
			"nop     \n\t"
		".endr     \n\t"
		#endif
		"cpi r16, 2 \n\t" "breq GRAPH_PGM_MODE \n\t" "rjmp NEXT_MODE2 \n\t"
		
		//-- 4clk/pix (RGB 8 colors )---------------------------- GRAPH_PGM MODE: render the Tiles WITHOUT x scrolling -----------------------------------------------------//
		"GRAPH_PGM_MODE: \n\t"		
		"ldi r16, 8 \n\t" "mov r2, r16 \n\t" "clr r16 \n\t"
		"ld ZL, X+ \n\t" "ld ZH, X+ \n\t" "add ZL, r15  \n\t" "adc ZH, r16 \n\t"
		
		// max value allowed is scrBufWidthInTile
		#if F_CPU == 32000000UL
		".rept 19 \n\t"
		#elif F_CPU == 24000000UL
		".rept 13 \n\t"
		#elif F_CPU == 20000000UL
		".rept ?? \n\t"
		#else   // Arduino default 16 MHz
		".rept 6 \n\t"
		#endif
								"lpm r0, Z+ \n\t"                   					//      3
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t"  "ld YL, X+ \n\t" 						// out1 1+1+2
			"out 0x0b, r0 \n\t" "lpm r0, Z+ \n\t"       								// out2 1+3
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t"  "ld YH, X+ \n\t"						// out3 1+1+1+1
			"out 0x0b, r0 \n\t" "lpm r0, Z+ \n\t"  										// out4 1+3
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t"  "add YL,r15 \n\t" "adc YH, r16 \n\t"	// out5 1+1+1+1
			"out 0x0b, r0 \n\t" "lpm r0, Z+ \n\t" 										// out6 1+3
			"out 0x0b, r0 \n\t" "mul r0,r2 \n\t"  "movw Z,Y \n\t"	"nop \n\t"			// out7	1+1+1+1
			"out 0x0b, r0 \n\t"															// out8 1
		".endr    \n\t"
								"clr r16 \n\t"	"rjmp _end \n\t"						//		1+2
		
		"NEXT_MODE2: \n\t"
		"cpi r16, 1 \n\t" "breq TEXT_MODE \n\t" "rjmp _end \n\t"

		//-mono 4clk (R or G or B)----------------------------- TEXT MODE: render the ascii character without x scrolling -----------------------------------------------------//
		"TEXT_MODE: \n\t"
		"clr r1 \n\t"
		"ld ZL, X+ \n\t" "ld ZH, X+ \n\t" "add ZL, r15 \n\t" "adc ZH, r1 \n\t"
		
		// max value allowed is scrBufWidthInTile
		#if F_CPU == 32000000UL
		"ldi r16, 29 \n\t"
		#elif F_CPU == 24000000UL
		"ldi r16, 19 \n\t"
		#elif F_CPU == 20000000UL
		"ldi r16, ?? \n\t"
		#else   // Arduino default 16 MHz
		"ldi r16, 8 \n\t"
		#endif
		"rjmp TEXT_entry \n\t"	
		
		"TEXT_1: \n\t"												"lsl r0 \n\t"		//		  1
		"out 0x0b, r0 \n\t"																// out6 1+1
		"TEXT_entry: \n\t"	"lpm r0, Z \n\t"											//          3
		"out 0x0b, r0 \n\t" "lsl r0 \n\t"		"ld ZL, X+ \n\t"						// out1 1+1+2
		"out 0x0b, r0 \n\t" "lsl r0 \n\t"		"ld ZH, X+ \n\t"						// out2 1+1+2
		"out 0x0b, r0 \n\t" "lsl r0 \n\t"		"add ZL, r15 \n\t"	"adc ZH, r1 \n\t"	// out3 1+1+1+1
		"out 0x0b, r0 \n\t" "lsl r0 \n\t"		"nop \n\t"			"subi r16,1 \n\t"	// out4 1+1+1+1
		"out 0x0b, r0 \n\t" "brne TEXT_1\n\t"	"nop \n\t"			"lsl r0 \n\t"       // out5 1+1+1+1
		"out 0x0b, r0 \n\t"	"nop \n\t"			"nop \n\t"			"clr r16 \n\t"		// out6 1	
		
		//-------------------------------------------------------------------------------------------------------------------------------//
		"_end: \n\t"
		"out 0x0b, r16 \n\t"      // back to black

		// restore unsaved registers
		"pop r17 \n\t" "pop r16 \n\t" "pop r15 \n\t" "pop r2 \n\t" "pop r1 \n\t" "pop r0 \n\t"
		:
		:  "x" (&scrBuf[TileIndex]), "r" (TilePixOffset), "r" (VGAmode), "r" (xScroll)
		// gcc assignation        x,                 r15,           r16,          r17,	    // ensure the assigned registers by the compiler are not overwritten by the user code
		: "r31", "r30", "r29", "r28", "r1", "r0"												// specify to the compiler the used registers not explicitly taken as parameter
	);
}
#endif

void _setDate(unsigned char yy, unsigned char mm, unsigned char dd) {
	// increment when invalid
	if ((dd > 31) && ((mm == 1) || (mm == 3) || (mm == 5) || (mm == 7) || (mm == 8) || (mm == 10) || (mm == 12))) { dd = 1; mm++;}
	if ((dd > 30) && ((mm == 4) || (mm == 6) || (mm == 9) || (mm == 11))) { dd = 1; mm++;}
	if ((dd > 29) && (mm == 2)) { dd = 1; mm++;}
	if ((dd == 29) && (mm == 2)) {
		unsigned int y = (unsigned int)yy + 1980;
		if (((y/4)*4 == y) || (((y/400)*400 == y) && ((y/100)*100 != y)) ) dd = 29; // OK, it's a leap year
		else { dd = 1; mm++;}
	}
	if (mm > 12) {mm = 1; yy++;}
	if (yy > 127) yy = 1;
	
	do{
		dateY = yy;	dateM = mm;	dateD = dd;	 // shadowing
	}while( (dateY != yy) || (dateM != mm) || (dateD != dd) );
}

void _setTime(unsigned char hh, unsigned char mm, unsigned char ss) {
	// increment when invalid
	if (ss > 59) {ss = 0; mm++;}
	if (mm > 59) {mm = 0; hh++;}
	if (hh > 23) { hh = 0;	_setDate(dateY, dateM, dateD+1);}
	
	do{
		timeH = hh;	timeM = mm;	timeS = ss;		 // shadowing
	}while( (timeH != hh) || (timeM != mm) || (timeS != ss) );
}
	
// ISR (Hsync pulse based) for the APL core
ISR (TIMER1_OVF_vect) {
	static volatile unsigned int vLine = totalLines;
	static volatile unsigned int vLineActive = activeLines;
	static volatile uint8_t scalingCnt = 0;
	static volatile uint8_t blinkCount = 0;
	//interrupt jitter fix (needed to keep pixels synced from free running timer0)
	// reference https://github.com/cnlohr/avrcraft/blob/master/terminal/ntsc.c
	// code source https://github.com/smaffer/vgax/blob/master/VGAX.cpp   
	asm volatile(
		"     clr r27               \n\t" //
		"     lds r26, %[timer0]    \n\t" //
		"     andi r26, 3           \n\t" //
		"     call TL               \n\t" //
		"TL:                        \n\t" //
		"     pop r31               \n\t" // 1 word
		"     pop r30               \n\t" // 1 word
		"     adiw Z, 6+0           \n\t" // 1 word, total 6 words for the previous instructions + offset tuning (0, 1 or 2)
		"     add r30, r26          \n\t" // 1 word
		"     adc r31, r27          \n\t" // 1 word
		"     ijmp                  \n\t" // 1 word   
		".rept 3+0                  \n\t" // between 3 and 3+offset
		"     nop                   \n\t" //     
		".endr                      \n\t" //
		:
		: [timer0] "i" (&TCNT0)
		: "r31", "r30", "r27", "r26"  	  // specify to the compiler the used registers not explicitly taken as parameter  
	);
 
	static volatile uint8_t PS2clk_last = 1;
	register uint8_t PS2clk = PINC & 0x20;
	register uint8_t PS2bit = (PINC & 0x10) >> 4;
	register uint8_t VGAmode_t = VGAmode;
	
	if (++vLineActive >= activeLines) {
		VGAmode = Disabled;	// disable when in the non active zone
	}
	VGArendering();
	VGAmode = VGAmode_t;	// restore	
	
	if (vLineActive < activeLines) {
		vLine++;
		if (++scalingCnt == verticalScaling) { // instead of division by 3
			pixLine++;
			if(pixLine >= (scrViewHeightInTile * TileMemHeight) + yScroll) pixLine = yScroll;
			scalingCnt = 0;
		}
	}	
	else {
		// V sync  
		if (vLine == 1) {
			PORTB |= 0x04;  // VSYNC set

			// sound update
			static uint8_t count=0;
			static unsigned int idx=0;
			if(soundMutex == 0) {
				if (soundbufptr != NULL) {
					if(count == 0) {
						uint8_t adrH = (unsigned int)soundbufptr >> 8;
						uint8_t* ptr = (uint8_t*)((unsigned int)soundbufptr & 0x7fff);
						if (adrH < 0x80) count = *(ptr + idx++);
						else count = pgm_read_byte(ptr + idx++); // force lpm instruction because the RAM pointer is reading from flash
						if (count == 255) {
							// loop
							idx=0; // reset index at array begin
							if (adrH < 0x80) count = *(ptr + idx++);
							else count = pgm_read_byte(ptr + idx++); // force lpm instruction because the RAM pointer is reading from flash
						}
						if (count == 0) {
							// end single play
							soundbufptr = NULL;
							idx=0; // reset index at array begin
							count = 1;
							TCCR2B &= 0xf8; // disable timer
						}
						else {             
							uint8_t value;
							if (adrH < 0x80) value = *(ptr + idx++);
							else value = pgm_read_byte(ptr + idx++); // force lpm instruction because the RAM pointer is reading from flash
							if (value != 0) {
								OCR2A = value;
								TCCR2B = (TCCR2B & 0xf8) | _BV(CS22) | _BV(CS21) | _BV(CS20);  //CTC mode, prescaler clock/1024
							}
							else TCCR2B &= 0xf8; // off because value 0 means no tone
						}
					}
					count--;
				} 	  
				else if (BASIC_duration > 0) {
					if (BASIC_tone != 0) {
						OCR2A = BASIC_tone;
						TCCR2B = (TCCR2B & 0xf8) | _BV(CS22) | _BV(CS21) | _BV(CS20);  //CTC mode, prescaler clock/1024
						BASIC_tone = 0;							// set back to 0 when sound activated
					}
					if (--BASIC_duration == 0) TCCR2B &= 0xf8; // off because value 0 means no tone  
				}
			}
		}
	
		if (vLine == 3) {
			PORTB &= 0xfb;  // VSYNC cleared
			ElaspsedTime += 17; // 16.66 ms added (60Hz)
			if (++tick == 60) { // pseudo 60Hz for the second clock
				//N.B. ICR1 value 126 gives a faster time clock: 2s/hr. ?? theoretically should be 0.01% slower? over clocking effect?
				//ICR1 value 127 gives a slower time clock: 30s/hr ??
				tick = 0;
				_setTime(timeH, timeM, timeS+1);
			}
		}
		if (++vLine == verticalBackPorchLines) {
			vLineActive = 0;  // start pixel out at next call
			scalingCnt = 0; // reset the pointer and counters
			pixLine = yScroll + TileScroll;	  
			if (cursorMutex == 0) {
				// cursor blinking for text mode only
				if((++blinkCount == 30) && (cursorTileIndex != 0xffff)) {
					scrBuf[cursorTileIndex] = cursorOnTile;  // on
				}
				if(blinkCount == 60) {
					if (cursorTileIndex != 0xffff) {
						scrBuf[cursorTileIndex] = cursorOffTile;  // off
					}		
					blinkCount = 0;		
				}
			}
		}
		if (vLine > totalLines) vLine = 1;
		if ((UCSR0A &(1<<UDRE0)) && (txbuffer.available() == true)) UDR0 = txbuffer.readfast(); // extract from tx ringbuffer and send	
		
		switch (TileNext) {
		case UPDATE:
			scrBuf[newTileIndexNext] = TilePtrNext;
			TileNext = NONE; 	// clear the semaphore
			break;
		case S_LEFT:
			{
				unsigned int *pSrc = (unsigned int *)(&scrBuf[newTileIndexNext]);
				unsigned int *pDst = (unsigned int *)(&scrBuf[newTileIndexNext-1]);
				for (uint8_t n = 0; n < scrBufWidthInTile-1; n++) {
					*pDst++ = *pSrc++;	// shift left a complete line
				}
				TileNext = NONE;				
			}
			break;
		case S_RIGHT:
			{
				unsigned int *pSrc = (unsigned int *)(&scrBuf[newTileIndexNext]);
				unsigned int *pDst = (unsigned int *)(&scrBuf[newTileIndexNext+1]);
				for (uint8_t n = 0; n < scrBufWidthInTile-1; n++) {
					*pDst-- = *pSrc--;	// shift right a complete line
				}
				TileNext = NONE;
			}
			break;
		case S_UP:
			{
				unsigned int *pSrc = (unsigned int *)(&scrBuf[newTileIndexNext]);
				unsigned int *pDst = (unsigned int *)(&scrBuf[newTileIndexNext-scrBufWidthInTile]);
				for (uint8_t n = 0; n < scrBufWidthInTile; n++) {
					*pDst++ = *pSrc++;	// shift up a complete line
				}
				TileNext = NONE;
			}
			break;
		case S_DOWN:
			{
				unsigned int *pSrc = (unsigned int *)(&scrBuf[newTileIndexNext]);
				unsigned int *pDst = (unsigned int *)(&scrBuf[newTileIndexNext+scrBufWidthInTile]);
				for (uint8_t n = 0; n < scrBufWidthInTile; n++) {
					*pDst-- = *pSrc--;	// shift down a complete line
				}
				TileNext = NONE;					
			}
			break;
		}
	}
  
	// alternate between PS2 and UART handling
	if((PS2clk_last != 0) && (PS2clk == 0)) { // falling edge
	  kbd.addbit(PS2bit);
	}
	else {
		// uart handling (for speed alternate between reading and flow control, @57600 a byte is received only each 5th isr's call)
		if (UCSR0A &(1<<RXC0)) rxbuffer.write((char)UDR0);   // receive and place the rxringbuffer
		else {
			// rx flow control (ensure the 16550 16 bytes fifo has can be emptied)
			if (rxbuffer.count() >= rxbuffer.BufferSize-20) {
				PORTC |= 0x08;  // CTS_n set
			}
			else {
				PORTC &= 0xf7;  // CTS_n cleared
			}
		}		
	}	
	PS2clk_last = PS2clk;
}

APLcore::APLcore() {
	VGAmode = Disabled;
	dateY = timeH = timeM = timeS = 0; dateM = dateD = 1; // Jan 1st, 1981, 00hh00m00s
}

void APLcore::coreInit() {
	
	TWCR = 0;			// disable the 2-wire serial interface

	initScreenBuffer(TextMode); // includes ports initialization

	DDRB |= 0x05;  // portb outputs: VSYNC assigned PB2 (Arduino pin D10), PB0 as output for pixelMux
	DDRC &= 0xcf;  // portc inputs: PS2 Keyboard clk on PC5, data PC4	

	// disable Timer 0
	TIMSK0 = 0;  // no interrupts on Timer 0
	TCCR0A=0;
	TCCR0B=1; //enable ext clock counter (used to fix the HSYNC interrupt jitter)
	OCR0A = 0;   // and turn it off
	OCR0B = 0;
	TCNT0=0;

	// Timer 1 - Horizontal Sync period: (1/60) / 525 * 1e6 = 31.746 uS  
	DDRB |= 0x02;  // HSYNC assigned PB1 (Arduino pin D9)
	TCCR1A=bit(WGM11) | bit(COM1A1);
	TCCR1B=bit(WGM12) | bit(WGM13) | bit(CS11); //8 prescaler
	ICR1=31.75F * F_CPU / 1000000 / 8UL - 1; //(period: 31.746 uS round-up to 31.75uS) * (FClk/8) - 1
	OCR1A= 4 * F_CPU / 1000000 / 8UL - 1; //(tOn: 4 uS) * (FClk/8) - 1 = 7
	TIFR1=bit(TOV1); //clear overflow flag
	TIMSK1=bit(TOIE1); //interrupt on overflow on TIMER1

	// Timer 2 - audio
	DDRB |= 0x08;                      //PB3 as output (Arduino pin D11)
	TCCR2A = _BV(WGM21) |_BV(COM2A0);  //toggle OC2A on compare match
	OCR2A = 1;                         //top value for counter 0-255
	TCCR2B = (TCCR2B & 0xf8);          //no clock source (disabled)

	// Set baud rate 9600 by default
	UARTsetBaudrate(9600);
	
	// Enable receiver and transmitter
	UCSR0B = (1<<TXEN0)|(1<<RXEN0);
	
	DDRC |= _BV(PC3);	// portc outputs: PC3 as output for CTS_n
	
	sei();	// Enable global interrupts	
	PORTB &= 0xfe;  // default pixelMux cleared
}

void APLcore::setColor(uint8_t color) {
	setColor(color, VGAmode);
}

void APLcore::setColor(uint8_t color, uint8_t mode) {
#ifdef PIXEL_HW_MUX
	// color validation
	screenColor = color; // all colors are allowed for text or graph mode
	// font assignment for text mode
	pFont = (uint8_t*)&fontWhite[0]; // same font for all colors
#else
	if(mode == GraphPgmMode) screenColor = color; // all colors are allowed
	else {
		if(mode == GraphMode) {
			if((color == RED) || (color == GREEN) || (color == (RED|GREEN))) screenColor = color; // only 4 colors
		}
		else {
			// color validation for TextMode
			if((color == RED) || (color == GREEN) || (color == BLUE)) screenColor = color; // only monochrome
			else screenColor = GREEN;	// default
		}
	}
	// font assignment for text mode
	pFont = (uint8_t*)&fontGreen[0]; //default green
	if(screenColor == RED) pFont = (uint8_t*)&fontRed[0];
	if(screenColor == BLUE) pFont = (uint8_t*)&fontBlue[0];	
#endif

	// (R0,G0, B0, R1, G1, B1) color assigned portd pins 2 to 7 as outputs, without changing the value of pins 0 & 1, which are RX & TX  
	DDRD &= 0b00000011;
	if (screenColor & RED) 	DDRD |= 0b10010000;
	if (screenColor & GREEN)DDRD |= 0b01001000;
	if (screenColor & BLUE)	DDRD |= 0b00100100;
}

void APLcore::initScreenBuffer() {
	initScreenBuffer(VGAmode);
}

void APLcore::initScreenBuffer(uint8_t mode) {	
	// validate the mode
	if((mode != TextMode) && (mode != GraphPgmMode) && (mode != GraphMode)) return;
		
#ifdef PIXEL_HW_MUX	
	if(mode == TextMode) {
		MemWidth = FontMemWidth;
		
		VGAmode = Disabled;
		if (mode != VGAmode) setColor(GREEN, TextMode); // init the color when mode changes
		
		// initialize the screen memory with valid content
		for (unsigned int y = 0; y < srcBufSize; y++) {			
			scrBuf[y] = (uint8_t*)&pFont[(unsigned int)FontMemSize * ' '];
		}		
		VGAmode = TextMode;	// restart VGA rendering	
	}
	else {
		MemWidth = TileMemWidth;
				
		VGAmode = Disabled;
		if (mode != VGAmode) setColor(WHITE, GraphMode); // init the color when mode changes
		//initialize the screen memory with valid content
		for (unsigned int y = 0; y < srcBufSize; y++) {    
			//scrBuf[y] = &coreTile4B[TileMemSize4B * 0];  // black tile
			if(y&1) scrBuf[y] = (volatile uint8_t*)&coreTile4B[TileMemSize4B * 4];  // checkerboard tile
			else scrBuf[y] = (volatile uint8_t*)&coreTile4B[TileMemSize4B * 2];  // square tile
			//else scrBuf[y] = &coreTile4B[TileMemSize4B * 3];  // full tile			
			scrBuf[y] = (uint8_t*)((unsigned int)scrBuf[y] | PGM_MARKER);		// add pgm marker
		}
		VGAmode = GraphMode; // restart VGA rendering
	}	
#else
	// no pixel mux
	if (mode != VGAmode) setColor(GREEN); // default init the color when mode changes
	if(mode == TextMode) {		
		MemWidth = FontMemWidth;
		VGAmode = Disabled;
		for (unsigned int y = 0; y < srcBufSize; y++) {			
			scrBuf[y] = (uint8_t*)&pFont[(unsigned int)FontMemSize * ' '];			
		}
		VGAmode = TextMode;	
	}
	else {
		if(mode == GraphPgmMode) {
			MemWidth = TileMemWidth4B;
			// initialize the screen memory with valid content
			VGAmode = Disabled;
			for (unsigned int y = 0; y < srcBufSize; y++) {
				//scrBuf[y] = (volatile uint8_t*)&coreTile4B[TileMemSize4B * 0];  // black tile
				if(y&1) scrBuf[y] = (volatile uint8_t*)&coreTile4B[TileMemSize4B * 1];  // checkerboard tile
				else scrBuf[y] = (volatile uint8_t*)&coreTile4B[TileMemSize4B * 2];  // square tile
				scrBuf[y] = (uint8_t*)((unsigned int)scrBuf[y] | PGM_MARKER);		// add pgm marker
			}			
			VGAmode = GraphPgmMode;			
			setColor(WHITE);
		}
		else {
			MemWidth = TileMemWidth;
			// initialize the screen memory with valid content
			VGAmode = Disabled;
			for (unsigned int y = 0; y < srcBufSize; y++) {    
				//scrBuf[y] = (volatile uint8_t*)&coreTile2B[TileMemSize * 0];  // black tile
				if(y&1) scrBuf[y] = (volatile uint8_t*)&coreTile2B[TileMemSize * 1];  // checkerboard tile
				else scrBuf[y] = (volatile uint8_t*)&coreTile2B[TileMemSize * 2];  // square tile
				scrBuf[y] = (uint8_t*)((unsigned int)scrBuf[y] | PGM_MARKER);		// add pgm marker
			}			
			VGAmode = GraphMode;
			setColor(RED|GREEN);
		}
	}
#endif	
	// critical section
	cursorMutex = 1;	// set the mutex	
	cursorTileIndex = 0xffff; // by default deactivate cursor
	cursorOnTile = (uint8_t*)&pFont[(unsigned int)FontMemSize * '_'];
	cursorOffTile = (uint8_t*)&pFont[(unsigned int)FontMemSize * ' '];
	cursorMutex = 0;	// release the mutex
	xScroll = yScroll = 0;	
}
	
uint8_t APLcore::getscrViewWidthInTile() {
	uint8_t retVal = 0;
	switch (VGAmode) {
	case TextMode: retVal = scrViewWidthInTileTEXT; break;
#ifndef PIXEL_HW_MUX	
	case GraphPgmMode: retVal = scrViewWidthInTileGRAPH_PGM; break;
#endif
	case GraphMode: 
		retVal = scrViewWidthInTileGRAPH; break;
	}
	return retVal;
}

uint8_t APLcore::getTileMemSize() {
	return MemWidth*TileMemHeight;		
}

#pragma GCC push_options
#pragma GCC optimize ("O0") // avoid optimization to ensure volatile variable propriety
uint8_t* APLcore::getTileXY(uint8_t x, uint8_t y) {
  while(TileNext != NONE);
  return (uint8_t*)((unsigned int)scrBuf[(unsigned int)scrBufWidthInTile * y + x] & (~(unsigned int)PGM_MARKER));
}

void APLcore::setRAMTileXY(uint8_t x, uint8_t y, uint8_t* TilePtr) {
	unsigned int tmp = (unsigned int)scrBufWidthInTile * y + x;
	while(TileNext != NONE);
	newTileIndexNext = tmp;
	TilePtrNext = TilePtr;
	TileNext = UPDATE;	// set the semaphore
}

void APLcore::setTileXY(uint8_t x, uint8_t y, uint8_t* TilePtr) {
	unsigned int tmp = (unsigned int)scrBufWidthInTile * y + x;
	while(TileNext != NONE);
	newTileIndexNext = tmp;
	TilePtrNext = (uint8_t*)((unsigned int)TilePtr | PGM_MARKER);
	TileNext = UPDATE;	// set the semaphore
}

void APLcore::shiftLeftTile() {
	for (unsigned int n = 1; n < srcBufSize; n+=scrBufWidthInTile) {
		while(TileNext != NONE);
		newTileIndexNext = n;		// src index
		TileNext = S_LEFT;	// set the semaphore
	}	
}

void APLcore::shiftRightTile() {
	for (signed int n = srcBufSize-2; n > 0; n-=scrBufWidthInTile) {
		while(TileNext != NONE);
		newTileIndexNext = (unsigned int)n;		// src index
		TileNext = S_RIGHT;	// set the semaphore
	}
}

void APLcore::shiftUpTile() {
	for (unsigned int n = scrBufWidthInTile; n < srcBufSize; n+=scrBufWidthInTile) {
		while(TileNext != NONE);
		newTileIndexNext = n;		// src index
		TileNext = S_UP;	// set the semaphore
	}
}

void APLcore::shiftDownTile() {
	for (signed int n = srcBufSize-1-scrBufWidthInTile; n > 0; n-=scrBufWidthInTile) {
		while(TileNext != NONE);
		newTileIndexNext = (unsigned int)n;		// src index
		TileNext = S_DOWN;	// set the semaphore
	}
}
#pragma GCC pop_options

void APLcore::setTileXYtext(uint8_t x, uint8_t y, char c) { 
	setTileXY(x, y, &pFont[(unsigned int)c * FontMemSize]);
}	

void APLcore::setCursor(uint8_t x, uint8_t y, bool active) { 
	// critical section
	cursorMutex = 1;	// set the mutex
	if((active == true) && (VGAmode == TextMode)) {		
		cursorTileIndex = (unsigned int)scrBufWidthInTile * y + x;
	}
	else cursorTileIndex = 0xffff; // inactive value
	cursorMutex = 0;	// release the mutex
}	

void APLcore::setCursorXY(uint8_t x, uint8_t y) { 
	if(cursorTileIndex != 0xffff) {		
		// critical section	
		cursorMutex = 1;	// set the mutex
		cursorTileIndex = (unsigned int)scrBufWidthInTile * y + x;
		cursorMutex = 0;	// release the mutex
	}
}	

void APLcore::setXScroll(uint8_t scrollValue) {
#ifndef NO_XSCROLLING
	if(VGAmode != TextMode) xScroll = scrollValue & 0b110;
#endif
}

void APLcore::setYScroll(uint8_t scrollValue) {
  if(VGAmode != TextMode) yScroll = scrollValue & 0b111;
}

void APLcore::setTileScroll(uint8_t scrollValue) {
  if (scrollValue > scrViewHeightInTile-1) TileScroll = (scrViewHeightInTile-1) * TileMemHeight;
  else TileScroll = scrollValue * TileMemHeight;
}

#pragma GCC push_options
#pragma GCC optimize ("O0") // avoid optimization to ensure volatile variable proprieties
bool APLcore::setRAMSound(uint8_t* str) {
  bool b = false;
  if(soundbufptr == NULL) {
	soundMutex = 1;	// set the mutex
	soundbufptr = (volatile uint8_t*)str;
	soundMutex = 0;	// release the mutex
    b = true;
  }
  return b;
}

bool APLcore::setSound(uint8_t* str) {
  bool b = false;
  if(soundbufptr == NULL) {
	soundMutex = 1;	// set the mutex
	soundbufptr = (volatile uint8_t*)((unsigned int)str | PGM_MARKER);
	soundMutex = 0;	// release the mutex
    b = true;
  }
  return b;
}

bool APLcore::setTone(uint8_t pitch, uint8_t duration) {
  bool b = false;
  if (BASIC_duration == 0) { // ensure last command completed
	  soundMutex = 1;	// set the mutex  
	  BASIC_tone = pgm_read_byte(BASIC_sound + pitch); // pitch in f/4
	  BASIC_duration = (int)duration * 6;  // for 1/10 to 1/60 of second
	  soundMutex = 0;	// release the mutex
	  b = true;
  }
  return b;
}

void APLcore::offSound() {
  // critical section at writing double byte (not atomic)
  soundMutex = 1;	// set the mutex
  soundbufptr = NULL; 
  soundMutex = 0;	// release the mutex}
}

bool APLcore::isSoundOff() {
  return (soundbufptr == NULL); 
}
#pragma GCC pop_options

bool APLcore::keyPressed() {
	return kbd.available();
}

char APLcore::keyRead() {
	return kbd.read();
}

void APLcore::UARTsetBaudrate(unsigned int baudrate) {
  // Set baud rate
  unsigned int BAUD_PRESCALE = ((F_CPU + baudrate * 16UL/2) / (baudrate * 16UL)) - 1; // rounded calculation
  UBRR0L = uint8_t(BAUD_PRESCALE & 0xff);        // Load lower 8-bits into the low byte of the UBRR register
  UBRR0H = uint8_t((BAUD_PRESCALE >> 8) & 0xff); // Load upper 8-bits into the high byte of the UBRR register
  /* Default frame format is 8 data bits, no parity, 1 stop bit to change use UCSRC, see AVR datasheet*/ 
}

bool APLcore::UARTavailableRX() {
	return rxbuffer.available();
}

bool APLcore::UARTavailableTX() {
	return txbuffer.available();
}

uint8_t APLcore::UARTcountRX() {
	return rxbuffer.count();
}

#ifdef ATMEL_STUDIO
uint8_t APLcore::UARTwrite(const char* data) {
#else
uint8_t APLcore::UARTwrite(const __FlashStringHelper* data) {
#endif
	char str[2]; str[1]=0; 
	char c;	char* ptr = (char*)data;
	do {
    str[0] = c = pgm_read_byte(ptr++);
		txbuffer.write(str);
  } while(c != 0);
	return 0;
}

uint8_t APLcore::UARTwrite(char* data) {
	return txbuffer.write(data);
}

uint8_t APLcore::UARTwrite(char data) {
	return txbuffer.write(data);
}

char APLcore::UARTread(){
	return rxbuffer.read();
}
	
char APLcore::UARTpeek() {
	return rxbuffer.peek();
}

#pragma GCC push_options
#pragma GCC optimize ("O0") // avoid optimization to ensure volatile variable proprieties
void APLcore::ms_delay(unsigned int t) {
  unsigned long tEnd = ms_elpased() + t;
  while(tEnd > ms_elpased());
}

unsigned long APLcore::ms_elpased() {
  unsigned long t = ElaspsedTime; // shadowing
  while (t != ElaspsedTime) {t = ElaspsedTime;};
  return t;
}

void APLcore::setDate(unsigned char yy, unsigned char mm, unsigned char dd) {	
	_setDate(yy, mm, dd);
}

void APLcore::setTime(unsigned char hh, unsigned char mm, unsigned char ss) {
	_setTime(hh, mm, ss);
}

/****************************************************************************************
                  Fat32 date and time encoding
Bytes   Content
0		Time (5/6/5 bits, for hour/minutes/doubleseconds)
1		Date (7/4/5 bits, for year-since-1980/month/day)
****************************************************************************************/
unsigned int APLcore::GetDateF32(void) {
	unsigned char yy, mm, dd;
	do{
		yy = dateY;	mm = dateM;	dd = dateD;  // shadowing
	}while( (dateY != yy) || (dateM != mm) || (dateD != dd) );
	return ((yy)<<9) | ((mm & 0x0f)<<5) | (dd & 0x1F);
}

unsigned int APLcore::GetTimeF32(void) {
    unsigned char hh, mm, ss;
	do{
		hh= timeH;	mm = timeM;	ss = timeS;  // shadowing
	}while( (timeH != hh) || (timeM != mm) || (timeS != ss) );	
	return (hh << 11) | ((mm & 0x3f) << 5) | ((ss / 2) & 0x1F);	
}

#pragma GCC pop_options