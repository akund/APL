/***************************************************************************************************/
/*                                                                                                 */
/* file:          APLcore.cpp                                                                      */
/*                                                                                                 */
/* source:        2018-2020, written by Adrian Kundert (adrian.kundert@gmail.com)                  */
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

//================================ ISR variables (begin) ==========================================//
// screen and buffer allocations
const int srcBufSize = (int)scrBufWidthInTile * (int)scrBufHeightInTile;
volatile byte* scrBuf[srcBufSize];

// VGA rendering variables
const byte verticalBackPorchLines = 35;  // includes 2 sync pulses  
const unsigned int activeLines = 480;  // 480 pixels high
const byte verticalFrontPorchLines = 10;  
const unsigned int totalLines = verticalBackPorchLines + activeLines + verticalFrontPorchLines;  // 525 lines totally
volatile unsigned int vLineActive = activeLines;
volatile byte xScroll = 0, yScroll = 0; // x scrolling, value between 0 and 6, y between 0 and 7
enum VGAmode {textMode = 0, graphMode = 1};
volatile VGAmode mode = textMode;

// Audio variable
volatile byte* soundbufptr = NULL;

// PS2 keyboard instantiation
PS2Keyboard kbd;
volatile unsigned int bitStream=0;
volatile byte bitCount = 0;
		
// UART ringbuffer (DO NOT USE ANY OTHER UART LIB TO AVOID INTERRUPT CONFLICT)
RingBuffer txbuffer, rxbuffer;

volatile unsigned long ElaspsedTime = 0; // 32-bit counter in ms since last reset
//================================ Hardware Config (end) ==========================================//

// ISR (Hsync pulse based) for the APL core
ISR (TIMER1_OVF_vect) {
  static volatile unsigned int vLine = 1;
  static volatile byte scalingCnt = 0;
  static volatile byte pixLine = 0; 
  
  //interrupt jitter fix (needed to keep pixels synced from free running timer0)
  // reference https://github.com/cnlohr/avrcraft/blob/master/terminal/ntsc.c
  // code source https://github.com/smaffer/vgax/blob/master/VGAX.cpp   
  asm volatile(
    "     lds r26, %[timer0]    \n\t" //
    "     andi r26, 3          \n\t" //
    "     call TL               \n\t" //
    "TL:                        \n\t" //
    "     pop r31               \n\t" //
    "     pop r30               \n\t" //
    "     adiw Z, 5+0           \n\t" // 5 cycles for the previous instructions + offset tuning (0, 1 or 2)
    "     add r30, r26          \n\t" //
    "     ijmp                  \n\t" //    
    ".rept 3+0                  \n\t" // between 3 and 3+offset
    "     nop                   \n\t" //     
    ".endr                      \n\t" //
  :
  : [timer0] "i" (&TCNT0)
 );
 
  static volatile byte PS2clk_last = 1;
  register byte PS2clk = PINC & 0x20;
  register byte PS2bit = (PINC & 0x10) >> 4;
  
  if (++vLineActive < activeLines) {
    vLine++;
    // the tile line is repeated HeightScaling    
    register unsigned int TileIndex = (pixLine / TileMemHeight) * (unsigned int)scrBufWidthInTile; 
    register byte TilePixOffset = (pixLine & (TileMemHeight-1)) * TileMemWidth;
    
#ifdef PIXEL_HW_MUX
		// blit pixel data to screen in HW mux (6 clock / 2 pix)
    asm volatile (            
      "push XL \n\t" "push XH \n\t" "push r28 \n\t" "push r29 \n\t" "push ZL \n\t" "push ZH \n\t"        
        "push r0 \n\t" "push r1 \n\t"           // save used registers (the compiler does not consider this code)

      "clr r0 \n\t" "cp r0, %3 \n\t" "breq TEXT_MODE \n\t" "rjmp GRAPH_MODE \n\t"

      //------------------------------ TEXT MODE: render the ascii character without x scrolling -----------------------------------------------------//
      "TEXT_MODE: \n\t" 
        "ld ZL, X+ \n\t" "ld ZH, X+ \n\t"             // 2+2    load the value end of the line
        "ldi r28, 4 \n\t" "ldi r29, 0 \n\t"           // 1+1
        "add ZL, %1 \n\t" "adc ZH, r29 \n\t"          // 1+1
        "sbi 0x05, 0 \n\t"                            // 2      the SBI enables the pixelMux, critical timing set just before the out instruction or 6*n cycles earlier

      #if F_CPU == 32000000UL
        ".rept 12  \n\t" // porch delay
					"nop     \n\t"
				".endr     \n\t"
				".rept 42 \n\t" 
      #elif F_CPU == 24000000UL
        ".rept 29 \n\t" 
      #elif F_CPU == 20000000UL
        ".rept ?? \n\t"
      #else // Arduino default 16 MHz
        ".rept 15 \n\t"
      #endif
                                                          "lpm r0, Z \n\t"                                      //     3
          "out 0x0b, r0 \n\t" "mul r28, r0 \n\t"          "nop \n\t" "ld ZL, X+ \n\t"                           // 1+2+1+2  
          "out 0x0b, r0 \n\t" "mul r28, r0 \n\t"          "nop \n\t" "ld ZH, X+ \n\t"                           // 1+2+1+2
          "out 0x0b, r0 \n\t" "add ZL, %1 \n\t" "adc ZH, r29 \n\t"                                              // 1+1+1
        ".endr    \n\t"                                         
        "rjmp PGM_RAM_end \n\t" 

      //------------------------------ GRAPH MODE: render the Tiles without x scrolling -----------------------------------------------------//
      "GRAPH_MODE: \n\t" 
        "ld ZL, X+ \n\t" "ld ZH, X+ \n\t"             // 2+2    load the value end of the line
        "sbi 0x05, 0 \n\t"                            // 2      the SBI enables the pixelMux, critical timing set just before the out instruction or 6*n cycles earlier
        "ldi r28, 4 \n\t" "ldi r29, 0 \n\t"           // 1+1
        "add ZL, %1 \n\t" "adc ZH, r29 \n\t"          // 1+1
        "cp ZH, %2 \n\t" "brlo .+34 \n\t"             // 1+1(2)

      #if F_CPU == 32000000UL
        ".rept 12  \n\t" // porch delay
					"nop     \n\t"    
				".endr     \n\t"
				".rept 31 \n\t" 
      #elif F_CPU == 24000000UL
        ".rept 21 \n\t" 
      #elif F_CPU == 20000000UL
        ".rept ?? \n\t"
      #else   // Arduino default 16 MHz
        ".rept 11 \n\t"
      #endif
          //"PGM_n: \n\t" // (code size 17 words)        
                                                          "lpm r0, Z \n\t"                                      //     3
          "out 0x0b, r0 \n\t" "mul r28, r0 \n\t"          "nop \n\t" "ld ZL, X+ \n\t"                           // 1+2+1+2  
          "out 0x0b, r0 \n\t" "mul r28, r0 \n\t"          "nop \n\t" "ld ZH, X+ \n\t"                           // 1+2+1+2
          "out 0x0b, r0 \n\t" "mul r28, r0 \n\t"          "add ZL, %1 \n\t" "adc ZH, r29 \n\t" "cp ZH, %2 \n\t" // 1+2+1+1+1
          "out 0x0b, r0 \n\t" "brsh .+36 \n\t" "rjmp .+68 \n\t"                                                 // 1+1(2)+2    
           
          //"RAM_n: \n\t" // (code size 17 words)
                                                                     "ld r0, Z \n\t"                            //       2
          "out 0x0b, r0 \n\t" "mul r28, r0 \n\t"          "nop \n\t" "ld ZL, X+ \n\t"                           // 1+2+1+2  
          "out 0x0b, r0 \n\t" "mul r28, r0 \n\t"          "nop \n\t" "ld ZH, X+ \n\t"                           // 1+2+1+2
          "out 0x0b, r0 \n\t" "mul r28, r0 \n\t"          "add ZL, %1 \n\t" "adc ZH, r29 \n\t" "cp ZH, %2 \n\t" // 1+2+1+1+1
          "out 0x0b, r0 \n\t" "nop \n\t" "brlo .+34 \n\t"                                                       // 1+1+1(2)

        ".endr    \n\t" 
        
        //"PGM_last: \n\t"  // (code size 17 words)                            
        "rjmp PGM_RAM_end \n\t"     // 1
        ".rept 16 \n\t"
          "nop \n\t"                // filling up to 16 words block code size
        ".endr    \n\t" 
        //"PGM_last: \n\t"  // (code size optional)                     
        "nop \n\t"         // "branch + nop" equals the "jmp" execution time 
     //-------------------------------------------------------------------------------------------------------------------------------//  	  
     "PGM_RAM_end: \n\t"        
        "clr r1 \n\t" "out 0x0b, r1 \n\t"      // back to black
        "cbi 0x05, 0 \n\t"                     // disable pixelMux

        "pop r1 \n\t" "pop r0 \n\t"                           // restore unsaved registers       
        "pop ZH \n\t" "pop ZL \n\t" "pop r29 \n\t" "pop r28 \n\t" "pop XH \n\t" "pop XL \n\t"        
        :
        :  "x" (&scrBuf[TileIndex]), "r" (TilePixOffset), "r" (PGMaddrH), "r" (mode)
    );  
#else
    // blit pixel data to screen without pixel HW mux (4 clock / pix)
    asm volatile (            
      "push XL \n\t" "push XH \n\t" "push r28 \n\t" "push r29 \n\t" "push ZL \n\t" "push ZH \n\t"        
        "push r0 \n\t" "push r1 \n\t"           // save used registers (the compiler does not consider this code)

      "clr r0 \n\t" "cp r0, %3 \n\t" "breq TEXT_MODE \n\t" "rjmp GRAPH_MODE \n\t"

      //------------------------------ TEXT MODE: render the ascii character without x scrolling -----------------------------------------------------//
      "TEXT_MODE: \n\t" 
        "cbi 0x05, 0 \n\t"                            // 2 disable pixelMux
				"ld ZL, X+ \n\t" "ld ZH, X+ \n\t"             // 2+2    load the value end of the line
        "ldi r28, 4 \n\t" "ldi r29, 0 \n\t"           // 1+1
        "add ZL, %1 \n\t" "adc ZH, r29 \n\t"          // 1+1
        
      // max value allowed is scrBufWidthInTile    
      #if F_CPU == 32000000UL
        ".rept 12  \n\t" // porch delay
					"nop     \n\t"    
				".endr     \n\t"
        ".rept 31 \n\t" 
      #elif F_CPU == 24000000UL
        ".rept 22 \n\t" 
      #elif F_CPU == 20000000UL
        ".rept ?? \n\t"
      #else   // Arduino default 16 MHz
        ".rept 11 \n\t"
      #endif
                              "lpm r0, Z \n\t"                                      //     3
          "out 0x0b, r0 \n\t" "lsl r0 \n\t"  "nop \n\t"        "nop \n\t"           // 1+1+1+1
          "out 0x0b, r0 \n\t" "lsl r0 \n\t"  "nop \n\t"        "nop \n\t"           // 1+1+1+1
          "out 0x0b, r0 \n\t" "lsl r0 \n\t"  "ld ZL, X+ \n\t"                       // 1+1+2  
          "out 0x0b, r0 \n\t" "lsl r0 \n\t"  "ld ZH, X+ \n\t"                       // 1+1+2
          "out 0x0b, r0 \n\t" "lsl r0 \n\t"  "add ZL, %1 \n\t" "adc ZH, r29 \n\t"   // 1+1+1+1  
          "out 0x0b, r0 \n\t"                                                       // 1
        ".endr    \n\t"                                         
        "rjmp PGM_RAM_end \n\t" 

      //------------------------------ GRAPH MODE: render the Tiles without x scrolling -----------------------------------------------------//
      "GRAPH_MODE: \n\t" 
        "cbi 0x05, 0 \n\t"                                // 2      disable pixelMux
				"ld ZL, X+ \n\t" "ld ZH, X+ \n\t"                 // 2+2    load the value end of the line
        "ldi r28, 4 \n\t" "ldi r29, 0 \n\t"               // 1+1
        "add ZL, %1 \n\t" "adc ZH, r29 \n\t"              // 1+1
        "cp ZH, %2 \n\t" "brlo .+74\n\t" "rjmp .+20 \n\t" // 1+1(2) branch -1

      // max value allowed is scrBufWidthInTile    
      #if F_CPU == 32000000UL
         ".rept 12  \n\t" // porch delay
					"nop     \n\t"    
				".endr     \n\t"
				".rept 21 \n\t" 
      #elif F_CPU == 24000000UL
        ".rept 16 \n\t" 
      #elif F_CPU == 20000000UL
        ".rept ?? \n\t"
      #else   // Arduino default 16 MHz
        ".rept 8 \n\t"
      #endif
            
          //--------------------- PGM source (code size 32 words) -------------------------  
          //"PGM_1: \n\t"     
                                                                   "lsl r0 \n\t"  //      1
          "out 0x0b, r0 \n\t" "nop \n\t"        "nop \n\t"         "lsl r0 \n\t"  // out6 1+1+1+1
          "out 0x0b, r0 \n\t" "nop \n\t"        "nop \n\t"         "lsl r0 \n\t"  // out7 1+1+1+1
          "out 0x0b, r0 \n\t"                                                     // out8 1+3  
          //"PGM_entry: \n\t"
                              "lpm r0, Z \n\t"                                    //        3
          "out 0x0b, r0 \n\t" "ld ZL, X+ \n\t"                     "lsl r0 \n\t"  // out1 1+2+1
          "out 0x0b, r0 \n\t" "ld ZH, X+ \n\t"                     "lsl r0 \n\t"  // out2 1+2+1
          "out 0x0b, r0 \n\t" "add ZL, %1 \n\t" "adc ZH, r29 \n\t" "lsl r0 \n\t"  // out3 1+1+1+1  
          "out 0x0b, r0 \n\t" "nop \n\t"        "lsl r0 \n\t"   "cp ZH, %2 \n\t"  // out4 1+1+1+1
          "out 0x0b, r0 \n\t" "brsh .+66 \n\t"  "nop \n\t"         "lsl r0 \n\t"  // out5 1+1+1(2)+1
          "out 0x0b, r0 \n\t" "lsl r0 \n\t"     "rjmp .+120 \n\t"                 // out6 1+1+2   
           
          //--------------------- RAM source (code size 28 words) ------------------------   
          //"RAM_1: \n\t"        
          "out 0x0b, r0 \n\t" "nop \n\t"        "nop \n\t"         "lsl r0 \n\t"  // out7 1+1+1+1
          //"RAM_entry: \n\t"
          "out 0x0b, r0 \n\t"     "ld r0, Z \n\t"                  "nop \n\t"     // out8 1+2+1
          "out 0x0b, r0 \n\t" "ld ZL, X+ \n\t"                     "lsl r0 \n\t"  // out1 1+2+1
          "out 0x0b, r0 \n\t" "ld ZH, X+ \n\t"                     "lsl r0 \n\t"  // out2 1+2+1
          "out 0x0b, r0 \n\t" "add ZL, %1 \n\t" "adc ZH, r29 \n\t" "lsl r0 \n\t"  // out3 1+1+1+1  
          "out 0x0b, r0 \n\t" "nop \n\t"        "lsl r0 \n\t"   "cp ZH, %2 \n\t"  // out4 1+1+1+1
          "out 0x0b, r0 \n\t" "brsh .+10 \n\t"  "nop \n\t" "lsl r0 \n\t"          // out5 1+1+1(2)+1
          "out 0x0b, r0 \n\t" "lsl r0 \n\t" "rjmp .+64 \n\t"                      // out6 1+1+2
        ".endr    \n\t" 
        
        //"PGM_last: \n\t"  // (code size 32 words) 
                                                                 "lsl r0 \n\t"    //      1            
        "out 0x0b, r0 \n\t" "nop \n\t"        "nop \n\t"         "lsl r0 \n\t"    // out6 1+1+1+1 
        "out 0x0b, r0 \n\t" "nop \n\t"        "nop \n\t"         "lsl r0 \n\t"    // out7 1+1+1+1 
        "out 0x0b, r0 \n\t" "rjmp PGM_RAM_end \n\t"                               // out8 1+2	
        ".rept 21 \n\t"
              "nop \n\t"								// filling up to 31 words block code size remaining
        ".endr    \n\t" 
        //"RAM_last: \n\t"  // (code size optional)                                            
        "out 0x0b, r0 \n\t" "nop \n\t"        "nop \n\t"         "lsl r0 \n\t"    // out7 1+1+1+1 
        "out 0x0b, r0 \n\t" "nop \n\t"                                            // out8 1 + "branch + nop" equals the "jmp" execution time      							
        //-------------------------------------------------------------------------------------------------------------------------------//  	  
        "PGM_RAM_end: \n\t"        
        "clr r1 \n\t" "out 0x0b, r1 \n\t"      // back to black
        "pop r1 \n\t" "pop r0 \n\t"                           // restore unsaved registers       
        "pop ZH \n\t" "pop ZL \n\t" "pop r29 \n\t" "pop r28 \n\t" "pop XH \n\t" "pop XL \n\t"        
        :
        :  "x" (&scrBuf[TileIndex]), "r" (TilePixOffset), "r" (PGMaddrH), "r" (mode)
    );  
#endif
    if (++scalingCnt == verticalScaling) { // instead of division by 3
      pixLine++;
      scalingCnt = 0;
    }
  }
  else {
    // V sync  
    if (vLine == 1) {
      PORTB |= 0x04;  // VSYNC set
      
      // sound update
      static byte count=0;
      static unsigned int idx=0;
      if(soundbufptr != NULL) {
        if(count == 0) {
          count = pgm_read_byte(soundbufptr + idx++); // force lpm instruction because the RAM pointer is reading from flash
          if (count == 255) {
            // loop
            idx=0; // reset index at array begin
            count = pgm_read_byte(soundbufptr + idx++);
          }
          if (count == 0) {
            // end single play
            soundbufptr = NULL;
            idx=0; // reset index at array begin
            count = 1;
            TCCR2B &= 0xf8; // disable timer
          }
          else {             
						byte value = pgm_read_byte(soundbufptr + idx++);
            if (value != 0) {
							OCR2A = value;
							TCCR2B = (TCCR2B & 0xf8) | _BV(CS22) | _BV(CS21) | _BV(CS20);  //CTC mode, prescaler clock/1024
						}
						else TCCR2B &= 0xf8; // off because value 0 means no tone
          }
       }
       count--;
      }
    }
    if (vLine == 3) {
      PORTB &= 0xfb;  // VSYNC cleared
      ElaspsedTime += 17; // 16.66 ms added (60Hz)
    }
    if (++vLine == verticalBackPorchLines) {
      vLineActive = 0;  // start pixel out at next call
      scalingCnt = 0; // reset the pointer and counters
      pixLine = yScroll;
    }
    if (vLine > totalLines) vLine = 1;       
  }
  
  if((PS2clk_last != 0) && (PS2clk == 0)) { // falling edge
      kbd.addbit(PS2bit);
  }
  PS2clk_last = PS2clk;

  // uart handling
  if ((UCSR0A &(1<<UDRE0)) && (txbuffer.available() > 0)) UDR0 = txbuffer.readISR(); // extract from tx ringbuffer and send
  if (UCSR0A &(1<<RXC0)) rxbuffer.writeISR((char)UDR0);   // receive and place the rxringbuffer
  
}

void APLcore::coreInit() {
	
  TWCR = 0;			// disable the 2-wire serial interface
	
	DDRD |= 0xc0;  // monochrome assigned portd pins 6 and 7 as outputs, without changing the value of pins 0 & 1, which are RX & TX  
  DDRB |= 0x05;  // VSYNC assigned PB2 (Arduino pin D10), PB0 as output for pixelMux
  DDRC &= 0xcf;  // PS2 Keyboard clk on PC5, data PC4
  
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
  ICR1= 31.7F * F_CPU / 1000000 / 8UL - 1; //(period: 31.7 uS) * (FClk/8) - 1 = 62
  OCR1A= 4 * F_CPU / 1000000 / 8UL - 1; //(tOn: 4 uS) * (FClk/8) - 1 = 7
  TIFR1=bit(TOV1); //clear overflow flag
  TIMSK1=bit(TOIE1); //interrupt on overflow on TIMER1
  
  // Timer 2 - audio
  DDRB |= 0x08;                      //PB3 as output (Arduino pin D11)
  TCCR2A = _BV(WGM21) |_BV(COM2A0);  //toggle OC2A on compare match
  OCR2A = 1;                         //top value for counter 0-255
  TCCR2B = (TCCR2B & 0xf8);          //no clock source (disabled)

   // Set baud rate
   const unsigned int BAUD_PRESCALE = (((F_CPU / (USART_BAUDRATE * 16UL))) - 1);
	 UBRR0L = byte(BAUD_PRESCALE & 0xff);        // Load lower 8-bits into the low byte of the UBRR register
   UBRR0H = byte((BAUD_PRESCALE >> 8) & 0xff); // Load upper 8-bits into the high byte of the UBRR register
   /* Default frame format is 8 data bits, no parity, 1 stop bit to change use UCSRC, see AVR datasheet*/ 

  // Enable receiver and transmitter and receive complete interrupt 
  UCSR0B = (1<<TXEN0)|(1<<RXEN0);
  
  // initialize the screen memory with valid content
  for (unsigned int y = 0; y < srcBufSize; y++) {    
    //scrBuf[y] = &coreTile[TileMemSize * 0];  // black tile
    //scrBuf[y] = &coreTile[TileMemSize * 1];  // damier tile
    scrBuf[y] = &coreTile[TileMemSize * 2];  // square tile
  }
}

bool APLcore::isLineActive() {
	return (vLineActive < activeLines) ? true:false;
}

void APLcore::setTextMode() {
	mode = textMode;
}
void APLcore::setGraphMode() {
	mode = graphMode;
}

byte APLcore::getscrViewWidthInTile() {
	if (mode == textMode)
		return scrViewWidthInTileTEXT; 
	else
		return scrViewWidthInTileGRAPH; 
}

byte* APLcore::getTileXY(byte x, byte y) {
  return scrBuf[(unsigned int)scrBufWidthInTile * y + x];
}

void APLcore::setTileXYdirect(byte x, byte y, byte* TilePtr) {
  scrBuf[(unsigned int)scrBufWidthInTile * y + x] = TilePtr;
}

void APLcore::setTileXY(byte x, byte y, byte* TilePtr) {
  unsigned int newTileIndex = (unsigned int)scrBufWidthInTile * y + x;
	// cli();
	// scrBuf[newTileIndex] = TilePtr; // critical section
	// sei();
	
	// second sync option
	while(isLineActive());
	scrBuf[newTileIndex] = TilePtr; // critical section
}

void APLcore::setTileXYtext(byte x, byte y, char c) { 
	setTileXY(x, y, &console_font_6x8[(unsigned int)c * TileMemHeight]);
}	

void APLcore::setXScroll(byte scrollValue) {
  xScroll = scrollValue;
}

void APLcore::setYScroll(byte scrollValue) {
  yScroll = scrollValue;
}

bool APLcore::setSound(char* str) {
  bool b = false;
  if (soundbufptr == NULL) {
    soundbufptr = str; // use the custom sound buffer
    b = true;
  }
  return b;
}

void APLcore::offSound() {
  soundbufptr = NULL; 
}

bool APLcore::isSoundOff() {
  return (soundbufptr == NULL); 
}

bool APLcore::keyPressed() {
	return kbd.available();
}

char APLcore::keyRead() {
	return kbd.read();
}

byte APLcore::UARTavailable_rx() {
	return rxbuffer.available();
}

byte APLcore::UARTavailable_tx() {
	return txbuffer.available();
}

byte APLcore::UARTwrite(const __FlashStringHelper* data) {
	char str[2]; str[1]=0; 
	char c;	char* ptr = (char*)data;
	do {
    str[0] = c = pgm_read_byte(ptr++);
		txbuffer.write(str);
  } while(c != 0);
	return 0;
}

byte APLcore::UARTwrite(char* data) {
	return txbuffer.write(data);
}

byte APLcore::UARTwrite(char data) {
	return txbuffer.write(data);
}

char APLcore::UARTread(){
	return rxbuffer.read();
}
	
char APLcore::UARTpeek() {
	return rxbuffer.peek();
}

void APLcore::ms_delay(unsigned int t) {
  unsigned long tEnd = ElaspsedTime + t;
  while(tEnd > ElaspsedTime);
}

unsigned long APLcore::ms_elpased() {
  return ElaspsedTime;
}
