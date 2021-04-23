/***************************************************************************************************/
/*                                                                                                 */
/* file:          main.cpp	                                                                       */
/*                                                                                                 */
/* source:        2018-2021, written by Adrian Kundert (adrian.kundert@gmail.com)                  */
/*                                                                                                 */
/* description:   demo application for the APL font usage										   */
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

#ifdef ATMEL_STUDIO
	#include <avr/pgmspace.h>
	#include <avr/io.h>
	#include <stdlib.h>	
#endif

#include "config.h"
#include <APLcore.h>

#pragma GCC optimize ("-O3") // speed optimization gives more deterministic behavior

void scrollingDemo();	// forward declaration

APLcore INSTANCE;
APLcore* pAPL = NULL;

int main() {
  
	pAPL = &INSTANCE; //APLcore::instance();
	pAPL->coreInit();
  
#ifdef PIXEL_HW_MUX
	//pAPL->setColor(RED|BLUE);
	//pAPL->setColor(GREEN);
	pAPL->setColor(WHITE);
#else
	pAPL->setColor(BLUE);
	//pAPL->setColor(RED|BLUE); // invalide color, shall become green
#endif
  
	for (uint8_t y=0; y<pAPL->getscrViewHeightInTile(); y++) {
		for (uint8_t x=0; x<pAPL->getscrViewWidthInTile(); x++) {
		pAPL->setTileXYtext(x, y, '0'+y+x); // write the pattern from '0'
		}
	} 

	while(1) {
	  unsigned long t = pAPL->ms_elpased();
	  static uint8_t x=0, y=0;
  
	  if (pAPL->keyPressed() == true) {
        
		// read the next key
		char c = pAPL->keyRead();  
		pAPL->UARTwrite(c);  

		// draw the char on the screen
		pAPL->setTileXYtext(x++, y, c);
		// update the index
		if (x >= pAPL->getscrViewWidthInTile()) { y++; x=0; }
		if (y >= pAPL->getscrViewHeightInTile()) { y=0;}
	  }

	  while(t+500 > pAPL->ms_elpased()); // 500 ms periodic refresh
	  pAPL->UARTwrite('.');
	}
	
	return 0;
  }