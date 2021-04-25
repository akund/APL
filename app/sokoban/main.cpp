/***************************************************************************************************/
/*                                                                                                 */
/* file:           main.cpp			                                                               */
/*                                                                                                 */
/* source:        2021, written by Adrian Kundert (adrian.kundert@gmail.com)                       */
/*                                                                                                 */
/* description:   Sokoban game (GNU C++ application)                                               */
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


#include "config.h"
#include "APLcore.h"
#include "APLtile.h"
#include "sokoban_tile.h"
#ifdef ATMEL_STUDIO
	#include <avr/pgmspace.h>
    #include <avr/io.h> 
#endif

#pragma GCC optimize ("-O3") // speed optimization gives more deterministic behavior

//------------------------------------------------------ APL layer ----------------------------------------------------------------------------//
APLcore INSTANCE;
APLcore* pAPL;
unsigned char heading;

void initAPL() {
	pAPL = &INSTANCE;
	pAPL->coreInit();	// default init text mode
	pAPL->initScreenBuffer(GraphPgmMode);	
	pAPL->UARTsetBaudrate(57600); //baud rate for the APL UART	
}

//------------------------------------------------------ Game layer ---------------------------------------------------------------------------//
const unsigned char walk[] PROGMEM = {1, 200, 0};
void stepSound() {
	pAPL->setSound((unsigned char*)&walk);
}

void updateMap(unsigned char* pMap, unsigned char x, unsigned char y, unsigned char tileIndex) {
			
	pMap[(unsigned int)y*SCREEN_SIZE_COL + x] = tileIndex;
	if (tileIndex >= iMAN_STANDING)	{	
		switch(heading) {
			case LEFTARROW :	tileIndex = (tileIndex - iMAN_STANDING) + iMAN_STANDING_L;	break;
			case RIGHTARROW :	break;
			case UPARROW :		tileIndex = (tileIndex - iMAN_STANDING) + iMAN_STANDING_U;	break;			
			case DOWNARROW :	tileIndex = (tileIndex - iMAN_STANDING) + iMAN_STANDING_D;	break;
		}
	}
	pAPL->setTileXY(x, y, (unsigned char*)&SOKOtile[(unsigned int)TileMemSize4B * tileIndex]);
}

unsigned char getMapTileIndex(unsigned char* pMap, unsigned char x, unsigned char y) {	
	return pMap[(unsigned int)y*SCREEN_SIZE_COL + x];
}

void sendPgmString(unsigned char x, unsigned char y, const char* str) {
	unsigned char n = 0;
	while(1) {
		unsigned char c = pgm_read_byte((unsigned char*)str + n);
		if (c == 0) break;
		if (c >= 'A') c = c-'A'+1;	// re-mapping
		pAPL->setTileXY(x++, y, (unsigned char*)&PETtile4B[(unsigned int)TileMemSize4B * c]);
		n++;
	}
}

#define stringSize 	4
void printScoreboard(unsigned char* moveStr, unsigned char* pushStr) {
	unsigned char n;
	for (n = 0; n < stringSize-1; n++) {
		if (moveStr[n] > '9') {
			moveStr[n] = '0';
			moveStr[n+1]++;
		}
	}	
	for (n = 0; n < stringSize; n++) {
		pAPL->setTileXY(n+5, 19, (unsigned char*)&PETtile4B[(unsigned int)TileMemSize4B * moveStr[stringSize-1-n]]);
	}

	for (n = 0; n < stringSize-1; n++) {
		if (pushStr[n] > '9') {
			pushStr[n] = '0';
			pushStr[n+1]++;
		}
	}
	for (n = 0; n < stringSize; n++) {
		pAPL->setTileXY(n+15, 19, (unsigned char*)&PETtile4B[(unsigned int)TileMemSize4B * pushStr[stringSize-1-n]]);
	}	
}		
		
/***********************************************************************************************/
//	main program
/***********************************************************************************************/

unsigned long timeOut;	// for unknown reason needs to be out of the main()
const char gameLevel[] PROGMEM = {"LEVEL 01             "};
const char scoreBoard[] PROGMEM = {"PUSH:     MOVE:     "};
const char gameEnded[] PROGMEM = {"WELL DONE!"};

int main() {
	unsigned char map[MAP_SIZE];  //N.B. gcc takes less memory when not global!!!
	enum MoveState {standing=0, push1=1, push2_empty=2, push2_box=3};
	MoveState state = standing;
	unsigned char xPos, yPos;
	unsigned char key;
	unsigned char pushStr[stringSize];
	unsigned char moveStr[stringSize];
	bool undo = false;

	initAPL();

	timeOut = key = 0;
	xPos=12; yPos=11; heading = RIGHTARROW;
	
	// get the the ROM map
	for (unsigned char y=0; y<SCREEN_SIZE_ROW; y++) {
		for (unsigned char x=0; x<SCREEN_SIZE_COL; x++) {
			unsigned int i = (unsigned int)y*SCREEN_SIZE_COL + x;
			updateMap(map, x, y, pgm_read_byte((unsigned char*)SOKOmap_L1 + i));
		}
	}
	
	// add the man
	updateMap(map, xPos, yPos, iMAN_STANDING);
		
	// init score board
	sendPgmString(0, 0, gameLevel);
	sendPgmString(0, 19, scoreBoard);
	for (unsigned char x = 0; x < stringSize; x++) {
		pushStr[x] = moveStr[x] = '0';
	}
	printScoreboard(moveStr, pushStr);
	
	while(1) {			
		// keyboard handling		
		if ((key == 0) && (pAPL->keyPressed() == true)) {
			key = pAPL->keyRead();
			
			if ((state != standing) && (key != heading)) key = 0; // don't allow other directions when still in move
			
			// undo command
			if((key == 'u') && (undo == true)) {
				unsigned int i=0;
				unsigned char tile=0;
				switch(heading) {
					case LEFTARROW :
						updateMap(map, xPos+1, yPos, getMapTileIndex(map, xPos, yPos)); // move man						
						tile = getMapTileIndex(map, xPos-1, yPos); // move box						
						if ((tile == iBOX) || (tile == iSTORED_BOX)) {
							i = (unsigned int)yPos*SCREEN_SIZE_COL + (xPos);
							if (pgm_read_byte((unsigned char*)SOKOmap_L1 + i) == iSTORAGE) tile = iSTORED_BOX;
							else tile = iBOX;
						}
						updateMap(map, xPos, yPos, tile);
						i = (unsigned int)yPos*SCREEN_SIZE_COL + (xPos-1);
						if (pgm_read_byte((unsigned char*)SOKOmap_L1 + i) == iSTORAGE) updateMap(map, xPos-1, yPos, iSTORAGE); // get back initial
						else updateMap(map, xPos-1, yPos, iEMPTY);
						xPos++;
						break;					
					case RIGHTARROW :
						updateMap(map, xPos-1, yPos, getMapTileIndex(map, xPos, yPos)); // move man
						tile = getMapTileIndex(map, xPos+1, yPos); // move box
						if ((tile == iBOX) || (tile == iSTORED_BOX)) {
							i = (unsigned int)yPos*SCREEN_SIZE_COL + (xPos);
							if (pgm_read_byte((unsigned char*)SOKOmap_L1 + i) == iSTORAGE) tile = iSTORED_BOX;
							else tile = iBOX;
						}						
						updateMap(map, xPos, yPos, tile);
						i = (unsigned int)yPos*SCREEN_SIZE_COL + (xPos+1);
						if (pgm_read_byte((unsigned char*)SOKOmap_L1 + i) == iSTORAGE) updateMap(map, xPos+1, yPos, iSTORAGE); // get back initial
						else updateMap(map, xPos+1, yPos, iEMPTY);
						xPos--;
						break;
					case UPARROW :
						updateMap(map, xPos, yPos+1, getMapTileIndex(map, xPos, yPos)); // move man
						tile = getMapTileIndex(map, xPos, yPos-1); // move box
						if ((tile == iBOX) || (tile == iSTORED_BOX)) {
							i = (unsigned int)yPos*SCREEN_SIZE_COL + (xPos);
							if (pgm_read_byte((unsigned char*)SOKOmap_L1 + i) == iSTORAGE) tile = iSTORED_BOX;
							else tile = iBOX;
						}
						updateMap(map, xPos, yPos, tile);						
						i = (unsigned int)(yPos-1)*SCREEN_SIZE_COL + xPos;
						if (pgm_read_byte((unsigned char*)SOKOmap_L1 + i) == iSTORAGE) updateMap(map, xPos, yPos-1, iSTORAGE); // get back initial
						else updateMap(map, xPos, yPos-1, iEMPTY);
						yPos++;
						break;
					break;
					case DOWNARROW :
						updateMap(map, xPos, yPos-1, getMapTileIndex(map, xPos, yPos)); // move man
						tile = getMapTileIndex(map, xPos, yPos+1); // move box
						if ((tile == iBOX) || (tile == iSTORED_BOX)) {
							i = (unsigned int)yPos*SCREEN_SIZE_COL + (xPos);
							if (pgm_read_byte((unsigned char*)SOKOmap_L1 + i) == iSTORAGE) tile = iSTORED_BOX;
							else tile = iBOX;
						}
						updateMap(map, xPos, yPos, tile);
						i = (unsigned int)(yPos+1)*SCREEN_SIZE_COL + xPos;
						if (pgm_read_byte((unsigned char*)SOKOmap_L1 + i) == iSTORAGE) updateMap(map, xPos, yPos+1, iSTORAGE); // get back initial
						else updateMap(map, xPos, yPos+1, iEMPTY);
						yPos--;
						break;
				}
				undo = false;
			}
			
			if((key != LEFTARROW) && (key != UPARROW) && (key != DOWNARROW) && (key != RIGHTARROW)) key = 0;
			else {
				if (key != heading) { // change direction when push completed
					heading = key;	// new direction
				}
			}
		}		
		
		unsigned char nextxPos = 0, nextyPos = 0, nextnextxPos = 0, nextnextyPos = 0;
		switch(heading) {
		case LEFTARROW : 
			nextxPos = xPos-1; nextyPos = yPos;
			nextnextxPos = xPos-2; nextnextyPos = yPos;
			break;
		case RIGHTARROW :
			nextxPos = xPos+1; nextyPos = yPos;
			nextnextxPos = xPos+2; nextnextyPos = yPos;
			break;
		case UPARROW :
			nextxPos = xPos; nextyPos = yPos-1;
			nextnextxPos = xPos; nextnextyPos = yPos-2;
			break;
		case DOWNARROW :
			nextxPos = xPos; nextyPos = yPos+1;
			nextnextxPos = xPos; nextnextyPos = yPos+2;
			break;
		}
		
		// execution in 50 ms pace sync
		while(pAPL->ms_elpased() < timeOut);
		timeOut = pAPL->ms_elpased() + 50;
		 
		switch(state) {
		case standing:
			{
				unsigned char nextTile = getMapTileIndex(map, nextxPos, nextyPos);
				unsigned char nextnextTile = getMapTileIndex(map, nextnextxPos, nextnextyPos);
				if ((key != 0) && ((nextTile == iEMPTY) || (nextTile == iSTORAGE) || (((nextTile == iBOX) || (nextTile == iSTORED_BOX)) && ((nextnextTile == iSTORAGE) || (nextnextTile == iEMPTY)))) ) {
					state = push1;		// begin to move				
					updateMap(map, xPos, yPos, iPUSHING1);
				}
				else key = 0;
			}			
			break;
		case push1:
			{
				unsigned char nextTile = getMapTileIndex(map, nextxPos, nextyPos);
				if(nextTile == iWALL) {
					state = standing;	// stop pushing
					updateMap(map, xPos, yPos, iMAN_STANDING);				
				}			
				if((nextTile == iBOX) || (nextTile == iSTORED_BOX)) {
					unsigned char nextnextTile = getMapTileIndex(map, nextnextxPos, nextnextyPos);
					if((nextnextTile == iEMPTY) || (nextnextTile == iSTORAGE)) {
						state = push2_box;	// push the box
						stepSound();
						unsigned int i = (unsigned int)yPos*SCREEN_SIZE_COL + xPos;
						if (pgm_read_byte((unsigned char*)SOKOmap_L1 + i) == iSTORAGE) updateMap(map, xPos, yPos, iSTORAGE); // get back initial
						else updateMap(map, xPos, yPos, iEMPTY);						
						xPos = nextxPos; yPos = nextyPos; // move to next field with half box
						updateMap(map, xPos, yPos, iPUSHING2_MAN_BOX);
						updateMap(map, nextnextxPos, nextnextyPos, iPUSHING2_HALF_BOX);	// second half box at next field					
					}
					else {
						state = standing;	// cannot push anymore
						updateMap(map, xPos, yPos, iMAN_STANDING);					
					}
				}
				if((nextTile == iEMPTY) || (nextTile == iSTORAGE)) {
					state = push2_empty;	// push the box
					stepSound();
					unsigned int i = (unsigned int)yPos*SCREEN_SIZE_COL + xPos;
					if (pgm_read_byte((unsigned char*)SOKOmap_L1 + i) == iSTORAGE) updateMap(map, xPos, yPos, iSTORAGE); // get back initial
					else updateMap(map, xPos, yPos, iEMPTY);
					xPos = nextxPos; yPos = nextyPos; // move to next field
					updateMap(map, xPos, yPos, iPUSHING2_MAN_ONLY);				
				}				
				key = 0;	// move lock end				
				break;
			}
		case push2_empty:
			if (key != 0) {
				state = push1;	// prepare to move to the next field
				updateMap(map, xPos, yPos, iPUSHING1);				
			}
			else {
				state = standing;	// stop moving
				updateMap(map, xPos, yPos, iMAN_STANDING);				
			}
			moveStr[0]++; printScoreboard(moveStr, pushStr);
			undo = false;
			break;
		case push2_box:
			if (key != 0) {
				state = push1;	// keep pushing box through next field
				updateMap(map, xPos, yPos, iPUSHING1);
			}
			else {
				state = standing;
				updateMap(map, xPos, yPos, iMAN_STANDING);
			}
			unsigned int i = (unsigned int)nextyPos*SCREEN_SIZE_COL + nextxPos;
			if (pgm_read_byte((unsigned char*)SOKOmap_L1 + i) == iSTORAGE) {
				updateMap(map, nextxPos, nextyPos, iSTORED_BOX); // get back initial
				// check if all boxes are stored
				unsigned int i = 0;
				while ((i < MAP_SIZE) && (map[i] != iBOX)) { i++;};
				if (i == MAP_SIZE) {
					sendPgmString(6, 10, gameEnded);
					while(1); // stop here
				}
			}
			else updateMap(map, nextxPos, nextyPos, iBOX);
			moveStr[0]++; pushStr[0]++; printScoreboard(moveStr, pushStr);
			undo = true;			
			break;
		}
	}
}