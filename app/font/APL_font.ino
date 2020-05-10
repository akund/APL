/***************************************************************************************************/
/*                                                                                                 */
/* file:          APL_font.ino                                                                     */
/*                                                                                                 */
/* source:        2018-2020, written by Adrian Kundert (adrian.kundert@gmail.com)                  */
/*                                                                                                 */
/* description:   Arduino demo application for the APL font usage                                  */
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

#include <APLcore.h>

APLcore* pAPL = NULL;

void setup() {
  
  pAPL = APLcore::instance();  
  pAPL->coreInit();
  pAPL->setTextMode();
#ifdef PIXEL_HW_MUX
  //pAPL->setColor(RED|BLUE);
  pAPL->setColor(GREEN|BLUE); //color cyan
  //pAPL->setColor(WHITE);
#else
  pAPL->setColor(BLUE);
#endif
  
  for (byte y=0; y<pAPL->getscrViewHeightInTile(); y++) {
      for (byte x=0; x<pAPL->getscrViewWidthInTile(); x++) {
        pAPL->setTileXYtext(x, y, '0'+y+x); // write the pattern from '0'
      }
  } 
}

void loop() {
  unsigned long t = pAPL->ms_elpased();
  static byte x=0, y=0;
  
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
