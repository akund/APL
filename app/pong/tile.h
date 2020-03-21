/***************************************************************************************************/
/*                                                                                                 */
/* file:          tile.h                                                                           */
/*                                                                                                 */
/* source:        2018-2020, written by Adrian Kundert (adrian.kundert@gmail.com)                  */
/*                                                                                                 */
/* description:   Basic tiles and sound for the APL                                                */
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

#ifndef tile_h
#define tile_h

const byte Tile[] __attribute__ ((aligned (512))) PROGMEM = {
  // tile 0 (empty)
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  // tile 1 (damier)
  0b10101010,
  0b01010101,
  0b10101010,
  0b01010101,
  0b10101010,
  0b01010101,
  0b10101010,
  0b01010101,
  // tile 2 (square)
  0b11111111,
  0b10000001,
  0b10000001,
  0b10000001,
  0b10000001,
  0b10000001,
  0b10000001,
  0b11111111,
  // tile 3 (ball)
  0b00000000,
  0b00000000,
  0b00000000,
  0b01000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  // tile 4 (paddle)
  0b01000000,
  0b01000000,
  0b01000000,
  0b01000000,
  0b01000000,
  0b01000000,
  0b01000000,
  0b01000000,
  // tile 5 (net)
  0b00000000,
  0b00000000,
  0b00000000,
  0b10000000,
  0b10000000,
  0b00000000,
  0b00000000,
  0b00000000,
  // tile 6 (top left corner)
  0b11111111,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  // tile 7 (top border)
  0b11111111,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  // tile 8 (top right corner)
  0b11111111,
  0b00000001,
  0b00000001,
  0b00000001,
  0b00000001,
  0b00000001,
  0b00000001,
  0b00000001,
  // tile 9 (right border)
  0b00000001,
  0b00000001,
  0b00000001,
  0b00000001,
  0b00000001,
  0b00000001,
  0b00000001,
  0b00000001,
  // tile 10 (right bottom corner)
  0b00000001,
  0b00000001,
  0b00000001,
  0b00000001,
  0b00000001,
  0b00000001,
  0b00000001,
  0b11111111,
  // tile 11 (bottom border)
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b11111111,
  // tile 12 (bottom left corner)
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b11111111,
  // tile 13 (left border)
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000
};  //  end of tile

const byte sound_intro[] PROGMEM = {
  10,  77 * scaling,
  5,  155 * scaling,
  5,    0 * scaling,
  5,  116 * scaling,
  5,    0 * scaling,
  5,   77 * scaling,
  5,    0 * scaling,
  5,  155 * scaling,
  5,  116 * scaling,
  10,   0 * scaling,
  10,  77 * scaling,
  10,   0 * scaling,
  10,  82 * scaling,
  5,  167 * scaling,
  5,    0 * scaling,
  5,  125 * scaling,
  5,    0 * scaling,
  5,   82 * scaling,
  5,    0 * scaling,
  5,  167 * scaling,
  5,  125 * scaling,
  10,   0 * scaling,
  10,  82 * scaling,
  10,   0 * scaling,
  10,  77 * scaling,
  5,  155 * scaling,
  5,    0 * scaling,
  5,  116 * scaling,
  5,    0 * scaling,
  5,   77 * scaling,
  5,    0 * scaling,
  5,  155 * scaling,
  5,  116 * scaling,
  10,   0 * scaling,
  10,  77 * scaling,
  10,   0 * scaling,
  5,   77 * scaling,
  5,   82 * scaling,
  5,   87 * scaling,
  5,    0 * scaling,
  5,   92 * scaling,
  5,   98 * scaling,
  5,  103 * scaling,
  5,    0 * scaling,
  5,  109 * scaling,
  5,  116 * scaling,
  5,  125 * scaling,
  5,    0 * scaling,
  10, 155 * scaling,
  0
};

// Wall sound: duration 16.6 ms, frequency 226 Hz.
const byte sound_wall[] PROGMEM = {
  16/17+1, 68 * scaling,
  0 
};

// Paddle sound: duration 96 ms, frequency 459 Hz.
const byte sound_paddle[] PROGMEM = {
  96/17+1, 33 * scaling,
  0 
};

// Point sound: duration 257 msec, frequency 490 Hz. 
const byte sound_point[] PROGMEM = {
  257/17+1, 31 * scaling,
  0 
};

#endif
