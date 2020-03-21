# APL – Arduino Peripheral Library

COPYRIGHT (C) 2020 Adrian Kundert  
[adrian.kundert@gmail.com](mailto:adrian.kundert@gmail.com)  

Introduction:

The APL development begun with an hobbyst project aimed to design a single board computer with audio-video and PS/2 keyboard interfaces. Such open source implementations are already realized by other ATMEL programmer like Nick Gammon and Sandro Maffiodo. Since every design is tradeoff and my features priorization were different I came out with a different implementation. Indeed the VGA resolution was important but I wanted to keep the UART port free for communication and use also as less as possible RAM. Therefore I had to store the data in the PGM memory and trade the color to monochrome.

Features Overview:

- Timer Interrupt based implementation (execution not impaced by the main loop)
- Low RAM foot print by tile rendering from PGM memory (4 clocks / pixel)
- Fast screen scolling by tile index
- Tile size 6 px by 8 px (character mode) or by 10 px by 10 px (graphic mode)
- Optional hardware pixel multiplexer to increase the resolution (3 clocks / pixel)
- PS/2 keyboard support
- Sound Tone from 45 Hz to 12 KHz
- SD card interface (TBD)

The APL is design for Arduino software development environement. The hardware configuration is flexible from standart board like Arduino Uno or Nano, but can also be custom with an higher system clock for better performance. Additionally, an external pixel multiplexer circuit can be added to increase even more the pixel resolution.

| Configuration | Tile Resolution in Character mode | Tile Resolution in Graphic mode | Pixel Resolution (width by height) |
| --- | --- | --- | --- |
| 16 MHz (Uno/Nano) | 11 (15) | 8 (11) | 64x160 (88x160) |
| 24 MHz | 22 (29) | 16 (21) | 128x160 (168x160) |
| 32 MHz (experimental) | 31 (42) |  21 (31) | 168x160 (248x160) |

**Uno/Nano board configuration (16 Mhz)**

![uno](doc/uno.png)

** Possibility to increase the system clock to 20 MHz and even more **

	**Bread board configuration (custom system clock)**

	![uno](doc/schema_APL_without_mux.png)

	**Hardware pixel multiplexer (custom system clock)**

	![uno](doc/schema_APL_with_mux.png)

Demo

Splash &amp; Font images

Pong video

Installation

1. Unzip the library in your Arduino library folder
2. Open the demo Font or Pong application with Arduino IDE
3. (if required) in APLcore.h change the sys clock and Pixel\_Mux configuration
4. Verify and Program your device