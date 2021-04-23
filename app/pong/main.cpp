/***************************************************************************************************/
/*                                                                                                 */
/* file:          main.cpp		                                                                   */
/*                                                                                                 */
/* source:        2018-2020, written by Adrian Kundert (adrian.kundert@gmail.com)                  */
/*                                                                                                 */
/* description:   Demo application for the APL graphic usage									   */
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
#include "tile.h"
#include "APLtile.h"

#ifdef ATMEL_STUDIO
	#include <avr/pgmspace.h>
	#include <avr/io.h>
	#include <stdlib.h>
    #include "config.h"
#endif

#pragma GCC optimize ("-O3") // speed optimization gives more deterministic behavior

void scrollingDemo();	// forward declaration

APLcore INSTANCE;
APLcore* pAPL = NULL;
uint8_t scrViewWidthInTile = 0;

// RAM tile the custom rendering
uint8_t RAM_Tile[TileMemSize];
uint8_t* BallTile = RAM_Tile;

// PGM tile pointer init
const uint8_t* Black = (const uint8_t*)&Tile[TileMemSize * tile_empty];
const uint8_t* PaddleL = &Tile[TileMemSize * tile_paddle_left];
const uint8_t* Net = &Tile[TileMemSize * tile_net];
uint8_t* ballBackgroundTile = (uint8_t*)Black;
uint8_t* paddleBackgroundTile = (uint8_t*)Black;

signed char Paddle_y = scrViewHeightInTile/2;
signed char Ball_x = 0, Ball_y = 0;
signed char Ball_tile_x = 0, Ball_tile_y = 0;

#define MIN_BALL_X 0
#define MAX_BALL_X 7
#define MIN_BALL_Y 0
#define MAX_BALL_Y 7

#define UP 0
#define DOWN 1
#define LEFT 0
#define RIGHT 1
uint8_t dirHV = DOWN, dirLR = RIGHT;

int main() {
  
	pAPL = &INSTANCE; //APLcore::instance();
	pAPL->coreInit();  

	pAPL->setSound((uint8_t*)sound_intro);   // set the intro sound
  
	// send version on serial port (default 9600 baud)
	pAPL->UARTwrite((PGM_P)("\nAPL version "));
	pAPL->UARTwrite((uint8_t)((pAPL->getSWversion() >> 8) + '0')); pAPL->UARTwrite((PGM_P)(".")); pAPL->UARTwrite((uint8_t)((pAPL->getSWversion() & 0xff) + '0'));
  
	const char menu1[] PROGMEM = "1 scroll";
	uint8_t x=0;
	while (menu1[x] != 0) {pAPL->setTileXYtext(x, 1, menu1[x]); x++;}
	const char menu2[] PROGMEM = "2 pong";
	x=0;
	while (menu2[x] != 0) {pAPL->setTileXYtext(x, 2, menu2[x]); x++;}

	char c=0;
	do {
		if (pAPL->keyPressed() == true) c = pAPL->keyRead();
	} while((c != '1') && (c != '2'));

	if (c == '1') scrollingDemo();
	// otherwise begin the pong game
      
	while(pAPL->isSoundOff() == false); // wait until intro sound ended
    
	pAPL->initScreenBuffer(GraphMode);
	scrViewWidthInTile = pAPL->getscrViewWidthInTile();
	Ball_tile_x =scrViewWidthInTile/2, Ball_tile_y = scrViewHeightInTile/2;
	// clear screen
	for(uint8_t y=0; y<scrViewHeightInTile; y++) {
		for (uint8_t x=0; x<scrViewWidthInTile; x++) {
			pAPL->setTileXY(x, y, (uint8_t *)Black);
		}
	}

	// draw the horizontal frame
	for (uint8_t x=0; x<scrViewWidthInTile; x++) {
		pAPL->setTileXY(x, 0, (uint8_t *)&Tile[TileMemSize * tile_top_border]);
		pAPL->setTileXY(x, scrViewHeightInTile-1, (uint8_t *)&Tile[TileMemSize * tile_bottom_border]);
	}
	// draw the vertical frame
	for(uint8_t y=0; y<scrViewHeightInTile; y++) {
		pAPL->setTileXY(0, y, (uint8_t *)&Tile[TileMemSize * tile_left_border]);
		pAPL->setTileXY(scrViewWidthInTile-1, y, (uint8_t *)&Tile[TileMemSize * tile_right_border]);
	}
	// draw the 4 corners
	pAPL->setTileXY(0, 0, (uint8_t *)&Tile[TileMemSize * tile_top_left_corner]);
	pAPL->setTileXY(scrViewWidthInTile-1, 0, (uint8_t *)&Tile[TileMemSize * tile_top_right_corner]);
	pAPL->setTileXY(0, scrViewHeightInTile-1, (uint8_t *)&Tile[TileMemSize * tile_bottom_left_corner]);
	pAPL->setTileXY(scrViewWidthInTile-1, scrViewHeightInTile-1, (uint8_t *)&Tile[TileMemSize * tile_right_bottom_corner]);   
	// draw the paddle
	paddleBackgroundTile = pAPL->getTileXY(0, Paddle_y);
	pAPL->setTileXY(0, Paddle_y, (uint8_t *)PaddleL); 
	// draw the net
	for(uint8_t y=1; y<scrViewHeightInTile-1; y++) {
		pAPL->setTileXY(scrViewWidthInTile/2, y, (uint8_t *)Net); 
	}
  
	// init the background tile at ball position  
	ballBackgroundTile = pAPL->getTileXY(Ball_tile_x, Ball_tile_y);


	while(1) {  
		unsigned long t = pAPL->ms_elpased();
  
		// check if the shortest command could be in the stream
		if (pAPL->UARTavailableRX() == true) {
			char singleChar = pAPL->UARTread();  // get the command
			pAPL->UARTwrite(".");
			switch (singleChar) {    
			case '1':   // sound1 command: 
				{
				pAPL->setSound((uint8_t *)sound_wall);
				break;
				}
			case '2':   // sound2 command: Paddle sound: duration 96 ms, frequency 459 Hz.
				{
				pAPL->setSound((uint8_t *)sound_paddle);
				break;
				}
			case '3':   // sound3 command: Point sound: duration 257 msec, frequency 490 Hz. 
				{
				pAPL->setSound((uint8_t *)sound_point);
				break;
				}   
			case 'f':   // off command
				{
				pAPL->offSound();
				break;
				}
			}
		}
  
		// keyboard control for the paddle
		if (pAPL->keyPressed() == true) {
			uint8_t paddle = Paddle_y;
        
			// read the next key
			char c = pAPL->keyRead();
			// check for some of the special keys
			if (c == UPARROW) {
				pAPL->UARTwrite("[Up]"); Paddle_y--;
			} else if (c == DOWNARROW) {
				pAPL->UARTwrite("[Down]"); Paddle_y++;
			}
			// limit the paddle position
			if (Paddle_y > scrViewHeightInTile-1) Paddle_y = scrViewHeightInTile-1;
			if (Paddle_y < 0) Paddle_y = 0;
    
			if (Paddle_y != paddle) {
				pAPL->setTileXY(0, paddle, paddleBackgroundTile); // restore tile where was the paddle on the screen       
				paddleBackgroundTile = pAPL->getTileXY(0, Paddle_y); // save the current tile
				pAPL->setTileXY(0, Paddle_y, (uint8_t *)PaddleL); // redraw the paddle on the screen
			}
		}
    
		// direction ball control
		signed char tmpx = Ball_tile_x, tmpy = Ball_tile_y; 
		if((tmpx == 0) && (tmpy == Paddle_y) && (Ball_x == MAX_BALL_Y/2)) {
			// touching the paddle
			dirLR = RIGHT; pAPL->setSound((uint8_t *)sound_paddle);
		}
		if (dirLR == RIGHT) Ball_x++;
		else Ball_x--;
		if (dirHV == DOWN) Ball_y++;
		else Ball_y--;
   
		// left-right ball motion control
		if ((Ball_x > MAX_BALL_X-1) && (tmpx >= scrViewWidthInTile-1)) {      
			pAPL->setSound((uint8_t *)sound_wall);        // bounce right side
			Ball_x--; dirLR = LEFT;            // move back on the same tile
		}
		if (Ball_x > MAX_BALL_X) {
			Ball_x = MIN_BALL_X; tmpx++; // move to the next tile on the right
		}
		if ((Ball_x < MIN_BALL_X+1) && (tmpx == 0)) {
			pAPL->setSound((uint8_t *)sound_point);        // touching the left side
			Ball_x = MIN_BALL_X; tmpx = scrViewWidthInTile/2; tmpy = scrViewHeightInTile/2; dirLR = RIGHT; // restart in the middle
		}
		if (Ball_x < MIN_BALL_X) {
			Ball_x = MAX_BALL_X; tmpx--;  // move to the next tile on the left
		}

		// up-down ball control
		if ((Ball_y > MAX_BALL_Y-1) && (tmpy >= scrViewHeightInTile-1)) {
			pAPL->setSound((uint8_t *)sound_wall);        // bounce bottom
			Ball_y--; dirHV = UP;              // move back on the same tile
		}
		if (Ball_y > MAX_BALL_Y) {
			Ball_y = MIN_BALL_Y; tmpy++; // move to the next tile down
		}
		if ((Ball_y < MIN_BALL_Y+1) && (tmpy == 0)) {
			pAPL->setSound((uint8_t *)sound_wall);        // bounce top
			Ball_y++; dirHV = DOWN;            // move back on the same tile
		}
		if (Ball_y < MIN_BALL_Y) {
			Ball_y = MAX_BALL_Y; tmpy--; // move to the next tile top
		}

		// restore the tile where was the ball on the screen
		pAPL->setTileXY(Ball_tile_x, Ball_tile_y, ballBackgroundTile);
  
		// get the background tile at new ball position  
		Ball_tile_x = tmpx;
		Ball_tile_y = tmpy;
		uint8_t* p = ballBackgroundTile = pAPL->getTileXY(Ball_tile_x, Ball_tile_y);
		for(uint8_t y=0; y<TileMemSize; y++) {
			BallTile[y] = pgm_read_byte(p++);
		}  
		// overlay the ball
#ifdef PIXEL_HW_MUX
		switch (Ball_x) {
			case 0: BallTile[Ball_y*TileMemWidth+0] |= 0b11100000; break;
			case 1: BallTile[Ball_y*TileMemWidth+0] |= 0b00011100; break;
			case 2: BallTile[Ball_y*TileMemWidth+1] |= 0b11100000; break;
			case 3: BallTile[Ball_y*TileMemWidth+1] |= 0b00011100; break;
			case 4: BallTile[Ball_y*TileMemWidth+2] |= 0b11100000; break;
			case 5: BallTile[Ball_y*TileMemWidth+2] |= 0b00011100; break;
			case 6: BallTile[Ball_y*TileMemWidth+3] |= 0b11100000; break;
			case 7: BallTile[Ball_y*TileMemWidth+3] |= 0b00011100; break;
		}
#else
		BallTile[Ball_y] |= 1 << (MAX_BALL_X - Ball_x);
#endif

		pAPL->setRAMTileXY(Ball_tile_x, Ball_tile_y, BallTile); // draw the ball on the screen

		while(t+25 > pAPL->ms_elpased()); // 25 ms periodic refresh
	}
}

void scrollingDemo() {
   signed char xAPL = 0, yAPL = 0;
#ifdef PIXEL_HW_MUX
   pAPL->initScreenBuffer(GraphMode);      // 8 colors in Graph mode      
#else   
    //pAPL->initScreenBuffer(GraphMode);   // 4 colors in Graph mode
	pAPL->initScreenBuffer(GraphPgmMode);   // 8 colors in GraphPGM mode
#endif    
    
    uint8_t scrViewWidthInTile = pAPL->getscrViewWidthInTile(); // xscrolling col excluded
    const uint8_t scrWidthInTile = scrViewWidthInTile+1; // add the right xscrolling column
    const uint8_t scrHeightInTile = scrViewHeightInTile+1; // add the bottom yscrolling row
    
    // splash screen (20 by 20 tiles)
    const uint8_t xSize = 20, ySize = 20;
    for(uint8_t y=0; y<scrHeightInTile; y++) {
        for (uint8_t x=0; x<scrWidthInTile; x++) {
            unsigned int yy = y;
            unsigned int i = yy * xSize + x;                        
            if ((yy >= ySize) || (x >= xSize)) { // fillout when exceed image
                yy = ySize-1;
                i = yy * xSize + xSize-1;
            }            
			//pAPL->setTileXY(x, y, (uint8_t *)&PETtile2B[TileMemSize * i]); // 4 colors
			pAPL->setTileXY(x, y, (uint8_t *)&TILEimage[TileMemSize4B * i]); // 8 colors
        }
    }

    signed char dirX = 1;
    signed char dirY = 15;
    while(1) {
        /*switch(pAPL->keyRead()) {
            case LEFTARROW : xAPL-=2; break;
            case RIGHTARROW : xAPL+=2; break;
            case DOWNARROW : yAPL-=1; break;
            case UPARROW : yAPL+=1; break;          
        }*/
        
        if (dirY > 0) { dirY++; yAPL++; }
        else { dirY--; yAPL--; }
        
        if (dirX > 0) { dirX++; xAPL++; }
        else { dirX--; xAPL--; }
                
        if(dirY >  30) dirY = -1;
        if(dirY < -30) dirY = 1;
        if(dirX >  30) dirX = -1;
        if(dirX < -30) dirX = 1;
        
        pAPL->ms_delay(20); // slow down
                
        // --------------- vertical up/down shift (by APL) --------------------------//
        if(yAPL <= -1) {
            uint8_t* pArray[scrWidthInTile];
            for(uint8_t x=0; x<scrWidthInTile; x++) {
                pArray[x] = pAPL->getTileXY(x, scrHeightInTile-1); // save bottom row (last scrolling row used in scrolling)
            }
            yAPL = 7; pAPL->setYScroll(yAPL); pAPL->shiftDownTile();
            for(uint8_t x=0; x<scrWidthInTile; x++) {
                pAPL->setTileXY(x, 0, pArray[x]); // restore
            }
            
        }
        if(yAPL >= 8) {
            uint8_t* pArray[scrWidthInTile];
            for(uint8_t x=0; x<scrWidthInTile; x++) {
                pArray[x] = pAPL->getTileXY(x, 0); // save top row (last scrolling row used in scrolling)
            }
            yAPL = 0; pAPL->setYScroll(yAPL); pAPL->shiftUpTile();
            for(uint8_t x=0; x<scrWidthInTile; x++) {
                pAPL->setTileXY(x, scrHeightInTile-1, pArray[x]); // restore
            }           
        }
        pAPL->setYScroll(yAPL);
    
        // --------------- horizontal left/right shift (by APL) --------------------------//
        if(xAPL <= -1) {
            uint8_t* pArray[scrHeightInTile];
            for(uint8_t y=0; y<scrHeightInTile; y++) {
                pArray[y] = pAPL->getTileXY(scrWidthInTile-1, y); // save right col (last scrolling col used in scrolling)
            }
            xAPL = 6; pAPL->setXScroll(xAPL); pAPL->shiftRightTile();
            for(uint8_t y=0; y<scrHeightInTile; y++) {
                pAPL->setTileXY(0, y, pArray[y]); // restore to left col
            }           
        }
        if(xAPL >= 8) {
            uint8_t* pArray[scrHeightInTile];
            for(uint8_t y=0; y<scrHeightInTile; y++) {
                pArray[y] = pAPL->getTileXY(0, y); // save left col
            }
            xAPL = 0; pAPL->setXScroll(xAPL); pAPL->shiftLeftTile();
            for(uint8_t y=0; y<scrHeightInTile; y++) {
                pAPL->setTileXY(scrWidthInTile-1, y, pArray[y]); // restore to right col (last scrolling col used in scrolling)
            }           
        }
        pAPL->setXScroll(xAPL); 
    }
}
