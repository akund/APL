/***************************************************************************************************/
/*                                                                                                 */
/* file:          APL_pong.ino                                                                     */
/*                                                                                                 */
/* source:        2018-2020, written by Adrian Kundert (adrian.kundert@gmail.com)                  */
/*                                                                                                 */
/* description:   Arduino demo application for the APL graphic usage                               */
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

APLcore* pAPL = 0;
byte scrViewWidthInTile = 0;

// RAM tile the custom rendering
byte RAM_Tile[TileMemSize];
byte* BallTile = &RAM_Tile[TileMemSize * 0];

// PGM tile pointer init
const byte* Black = &Tile[TileMemSize * 0];
const byte* Paddle = &Tile[TileMemSize * 4];
const byte* Net = &Tile[TileMemSize * 5];
byte* ballBackgroundTile = Black;
byte* paddleBackgroundTile = Black;

signed char Paddle_y = scrViewHeightInTile/2;
signed char Ball_x = 0, Ball_y = 0;
signed char Ball_tile_x =scrViewWidthInTile/2, Ball_tile_y = scrViewHeightInTile/2;

#define MIN_BALL_X 0
#define MAX_BALL_X 7
#define MIN_BALL_Y 0
#define MAX_BALL_Y 7

#define UP 0
#define DOWN 1
#define LEFT 0
#define RIGHT 1
byte dirHV = DOWN, dirLR = RIGHT;

void setup() {
  
  pAPL = APLcore::instance();
  pAPL->setGraphMode();
  pAPL->coreInit();
  scrViewWidthInTile = pAPL->getscrViewWidthInTile();
  pAPL->setSound(sound_intro);   // set the intro sound
  
  // send version on serial port
  pAPL->UARTwrite(F("\nArduino Peripheral Library version "));
  pAPL->UARTwrite((byte)((pAPL->getSWversion() >> 8) + '0')); pAPL->UARTwrite(F(".")); pAPL->UARTwrite((byte)((pAPL->getSWversion() & 0xff) + '0'));
  
  // splash screen
  for(byte y=0; y<scrViewHeightInTile; y++) {
    unsigned int i = (unsigned int)y * 20;  // 20 tiles width
    for (byte x=0; x<scrViewWidthInTile; x++) {      
      pAPL->setTileXY(x, y, &TILEimage[TileMemSize * i++]);
    }
  }
  bool b;
  do {
    b = pAPL->isSoundOff();
    pAPL->ms_delay(1000); pAPL->UARTwrite('.'); // workaround: without a statement the soundbufptr is not re-evaluated!
  }
  while(b == false); // wait until intro sound ended
  
  // clear screen
  for(byte y=0; y<scrViewHeightInTile; y++) {
    for (byte x=0; x<scrViewWidthInTile; x++) {
      pAPL->setTileXY(x, y, Black);
    }      
  }

  // draw the horizontal frame
  for (byte x=0; x<scrViewWidthInTile; x++) {
    pAPL->setTileXY(x, 0, &Tile[TileMemSize * 7]);
    pAPL->setTileXY(x, scrViewHeightInTile-1, &Tile[TileMemSize * 11]);
  }      
  // draw the vertical frame
  for(byte y=0; y<scrViewHeightInTile; y++) {
    pAPL->setTileXY(0, y, &Tile[TileMemSize * 13]);
    pAPL->setTileXY(scrViewWidthInTile-1, y, &Tile[TileMemSize * 9]);
  }
  // draw the 4 corners
  pAPL->setTileXY(0, 0, &Tile[TileMemSize * 6]);
  pAPL->setTileXY(scrViewWidthInTile-1, 0, &Tile[TileMemSize * 8]);
  pAPL->setTileXY(0, scrViewHeightInTile-1, &Tile[TileMemSize * 12]);
  pAPL->setTileXY(scrViewWidthInTile-1, scrViewHeightInTile-1, &Tile[TileMemSize * 10]);   
  // draw the paddle
  paddleBackgroundTile = pAPL->getTileXY(0, Paddle_y);
  pAPL->setTileXY(0, Paddle_y, Paddle); 
  // draw the net
  for(byte y=1; y<scrViewHeightInTile-1; y++) {
    pAPL->setTileXY(scrViewWidthInTile/2, y, Net); 
  }
  
  // init the background tile at ball position  
  ballBackgroundTile = pAPL->getTileXY(Ball_tile_x, Ball_tile_y);
}

void loop() {
  
  unsigned long t = pAPL->ms_elpased();
  
  // check if the shortest command could be in the stream
  if (pAPL->UARTavailable_rx() >= 1) {
    char singleChar = pAPL->UARTread();  // get the command
    pAPL->UARTwrite(".");
    switch (singleChar) {    
    case '1':   // sound1 command: 
      {
        pAPL->setSound(sound_wall);
        break;
      }
    case '2':   // sound2 command: Paddle sound: duration 96 ms, frequency 459 Hz.
      {
        pAPL->setSound(sound_paddle);
        break;
      }
    case '3':   // sound3 command: Point sound: duration 257 msec, frequency 490 Hz. 
      {
        pAPL->setSound(sound_point);
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
    byte paddle = Paddle_y;
        
    // read the next key
    char c = pAPL->keyRead();
    // check for some of the special keys
    if (c == PS2_UPARROW) {
      pAPL->UARTwrite("[Up]"); Paddle_y--;
    } else if (c == PS2_DOWNARROW) {
      pAPL->UARTwrite("[Down]"); Paddle_y++;
    }
    // limit the paddle position
    if (Paddle_y > scrViewHeightInTile-1) Paddle_y = scrViewHeightInTile-1;
    if (Paddle_y < 0) Paddle_y = 0;
    
    if (Paddle_y != paddle) {
      pAPL->setTileXY(0, paddle, paddleBackgroundTile); // restore tile where was the paddle on the screen       
      paddleBackgroundTile = pAPL->getTileXY(0, Paddle_y); // save th current tile
      pAPL->setTileXY(0, Paddle_y, Paddle); // redraw the paddle on the screen
    }
  }
    
  // direction ball control
  signed char tmpx = Ball_tile_x, tmpy = Ball_tile_y; 
  if((tmpx == 0) && (tmpy == Paddle_y) && (Ball_x == MAX_BALL_Y/2)) {
    // touching the paddle
    dirLR = RIGHT; pAPL->setSound(sound_paddle);
  }
  if (dirLR == RIGHT) Ball_x++;
  else Ball_x--;
  if (dirHV == DOWN) Ball_y++;
  else Ball_y--;
   
  // left-right ball motion control
  if ((Ball_x > MAX_BALL_X-1) && (tmpx >= scrViewWidthInTile-1)) {      
    pAPL->setSound(sound_wall);        // bounce right side
    Ball_x--; dirLR = LEFT;            // move back on the same tile
  }
  if (Ball_x > MAX_BALL_X) {
    Ball_x = MIN_BALL_X; tmpx++; // move to the next tile on the right
  }
  if ((Ball_x < MIN_BALL_X+1) && (tmpx == 0)) {
    pAPL->setSound(sound_point);        // touching the left side
    Ball_x = MIN_BALL_X; tmpx = scrViewWidthInTile/2; tmpy = scrViewHeightInTile/2; dirLR = RIGHT; // restart in the middle
  }
  if (Ball_x < MIN_BALL_X) {
    Ball_x = MAX_BALL_X; tmpx--;  // move to the next tile on the left
  }

  // up-down ball control
  if ((Ball_y > MAX_BALL_Y-1) && (tmpy >= scrViewHeightInTile-1)) {
    pAPL->setSound(sound_wall);        // bounce bottom
    Ball_y--; dirHV = UP;              // move back on the same tile
  }
  if (Ball_y > MAX_BALL_Y) {
    Ball_y = MIN_BALL_Y; tmpy++; // move to the next tile down
  }
  if ((Ball_y < MIN_BALL_Y+1) && (tmpy == 0)) {
    pAPL->setSound(sound_wall);        // bounce top
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
  byte* p = ballBackgroundTile = pAPL->getTileXY(Ball_tile_x, Ball_tile_y);
  for(byte y=0; y<TileMemHeight; y++) {
    BallTile[y] = pgm_read_byte(p++);
  }  
  // overlay the ball
  BallTile[Ball_y] |= 1 << (MAX_BALL_X - Ball_x);
  pAPL->setTileXY(Ball_tile_x, Ball_tile_y, BallTile); // draw the ball on the screen    

  while(t+25 > pAPL->ms_elpased()); // 25 ms periodic refresh
 }
