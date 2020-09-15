/*

Name: Steven Jiao, Tyler Mah
CCID: -,-
CMPUT 275, Winter 2019
Date: 02/08/2020
Acknowledgements: N/A
Assignment 1 Part 2: Mode 0

*/

#define SD_CS 10
#define JOY_VERT  A9 // should connect A9 to pin VRx
#define JOY_HORIZ A8 // should connect A8 to pin VRy
#define JOY_SEL   53

#include <Arduino.h>

// core graphics library (written by Adafruit)
#include <Adafruit_GFX.h>

// Hardware-specific graphics library for MCU Friend 3.5" TFT LCD shield
#include <MCUFRIEND_kbv.h>

// LCD and SD card will communicate using the Serial Peripheral Interface (SPI)
// e.g., SPI is used to display images stored on the SD card
#include <SPI.h>

// needed for reading/writing to SD card
#include <SD.h>

#include "lcd_image.h"


MCUFRIEND_kbv tft;

#define DISPLAY_WIDTH  480
#define DISPLAY_HEIGHT 320
#define YEG_SIZE 2048

lcd_image_t yegImage = { "yeg-big.lcd", YEG_SIZE, YEG_SIZE };

#define JOY_CENTER   512
#define JOY_DEADZONE 64

#define CURSOR_SIZE 9


// made yegMiddleX and yegMiddleY into global variables with 60 bits rightmost 
// column black on the display
int yegMiddleX = YEG_SIZE/2 - (DISPLAY_WIDTH - 60)/2;
int yegMiddleY = YEG_SIZE/2 - DISPLAY_HEIGHT/2;

// the cursor position on the display
int cursorX, cursorY;

// forward declaration for redrawing the cursor
void redrawCursor(uint16_t colour);

void setup() {
	init();
	Serial.begin(9600);

	pinMode(JOY_SEL, INPUT_PULLUP);

	//    tft.reset();             // hardware reset
	uint16_t ID = tft.readID();    // read ID from display
	Serial.print("ID = 0x");
	Serial.println(ID, HEX);
	if (ID == 0xD3D3) ID = 0x9481; // write-only shield
  
	// must come before SD.begin() ...
	tft.begin(ID);                 // LCD gets ready to work

	Serial.print("Initializing SD card...");
	if (!SD.begin(SD_CS)) {
		Serial.println("failed! Is it inserted properly?");
		while (true) {}
	}
	Serial.println("OK!");

	tft.setRotation(1);

	tft.fillScreen(TFT_BLACK);

	// draws the centre of the Edmonton map, leaving the rightmost 60 columns black
	lcd_image_draw(&yegImage, &tft, yegMiddleX, yegMiddleY,
                 0, 0, DISPLAY_WIDTH - 60, DISPLAY_HEIGHT);

	// initial cursor position is the middle of the screen
	cursorX = (DISPLAY_WIDTH - 60)/2;
	cursorY = DISPLAY_HEIGHT/2;

	redrawCursor(TFT_RED);
}

void redrawCursor(uint16_t colour) {
  tft.fillRect(cursorX - CURSOR_SIZE/2, cursorY - CURSOR_SIZE/2,
               CURSOR_SIZE, CURSOR_SIZE, colour);
}

// redraw the map using the cursor and edmonton image positions
void redrawMap(int xPos,int yPos) {
  lcd_image_draw(&yegImage, &tft, yegMiddleX + xPos - CURSOR_SIZE/2, 
    yegMiddleY + yPos - CURSOR_SIZE/2, xPos - CURSOR_SIZE/2, 
    yPos - CURSOR_SIZE/2, CURSOR_SIZE, CURSOR_SIZE);
}

// check if the map should be moved or not depending on if the cursor
// is at the edge and where we are on the map
void checkMoveMap() {
	// check if the map has moved
	bool hasMoved = false;
	// if the cursor is at the top of LCD display, see if we can move the map
	if ((cursorY < CURSOR_SIZE/2) && (yegMiddleY > 0)) {
	hasMoved = true;
	// change the point to redraw the map
    yegMiddleY = yegMiddleY - DISPLAY_HEIGHT;

    // if cursor is at bottom of LCD display
 	} else if ((cursorY > (DISPLAY_HEIGHT - CURSOR_SIZE/2)) 
                && (yegMiddleY < (YEG_SIZE - DISPLAY_HEIGHT))) {

 	hasMoved = true;
    yegMiddleY = yegMiddleY + DISPLAY_HEIGHT;
   
    // if cursor is at left of LCD display
  	} else if ((cursorX < CURSOR_SIZE/2) && (yegMiddleX > 0)) {

  	hasMoved = true;
    yegMiddleX = yegMiddleX - (DISPLAY_WIDTH - 60);

    // if cursor is at right of LCD display
  	} else if ((cursorX > (DISPLAY_WIDTH - 60 - CURSOR_SIZE/2)) 
  					&& (yegMiddleX < (YEG_SIZE - DISPLAY_WIDTH - 60))) {

	hasMoved = true;
    yegMiddleX = yegMiddleX + (DISPLAY_WIDTH - 60);
  
  	}

  	// if we moved, then move the map if possible
  	if (hasMoved) {
  		// constrain the coordinates to 0 and the maximum size of the YEG 
  		// picture size subtracting the size of the LCD screen
  		yegMiddleX = constrain(yegMiddleX, 0, (YEG_SIZE - DISPLAY_WIDTH - 60));
  		yegMiddleY = constrain(yegMiddleY, 0, (YEG_SIZE - DISPLAY_HEIGHT));
  		lcd_image_draw(&yegImage, &tft, yegMiddleX, yegMiddleY,
                 0, 0, DISPLAY_WIDTH - 60, DISPLAY_HEIGHT);
  		
  		cursorX = (DISPLAY_WIDTH - 60)/2;
  		cursorY = DISPLAY_HEIGHT/2;
  	}
}

// main function to run everything; processes the movement of joystick
void processJoystick() {
  int xVal = analogRead(JOY_HORIZ);
  int yVal = analogRead(JOY_VERT);
  int buttonVal = digitalRead(JOY_SEL);

  // get the initial cursor positions 
  int icursorX = cursorX;
  int icursorY = cursorY;

  // now move the cursor
  if (yVal < JOY_CENTER - JOY_DEADZONE) {
    // if the joystick is moved far enough, the cursor will go faster
    if (yVal > JOY_CENTER - JOY_DEADZONE - 350) {
      cursorY -= 1; 
    } else {
      cursorY -= 5;
    }
  }
  else if (yVal > JOY_CENTER + JOY_DEADZONE) {
    if (yVal < JOY_CENTER + JOY_DEADZONE + 350) {
      cursorY += 1; 
    } else {
      cursorY += 5;
    }
  }

  // remember the x-reading increases as we push left
  if (xVal > JOY_CENTER + JOY_DEADZONE) {
    if (xVal < JOY_CENTER + JOY_DEADZONE + 350) {
      cursorX -= 1; 
    } else {
      cursorX -= 5;
    }

  }
  else if (xVal < JOY_CENTER - JOY_DEADZONE) {
    if (xVal > JOY_CENTER - JOY_DEADZONE - 350) {
      cursorX += 1; 
    } else {
      cursorX += 5;
    }
  }

  // function for checking if the map can and should be shifted or not 
  checkMoveMap();

  // constrain x and y cursor positions to the given map.
  // hard-coded in -1 for both the upper-bounds of cursorX and Y due to 
  // int division of CURSOR_SIZE not giving the exact pixels needed
  cursorX = constrain(cursorX, CURSOR_SIZE/2, 
    DISPLAY_WIDTH - 61 - CURSOR_SIZE/2);
  cursorY = constrain(cursorY, CURSOR_SIZE/2, 
  	DISPLAY_HEIGHT - CURSOR_SIZE/2 - 1);

  // check if cursor has moved. If it has, redraw the map.
  if ((icursorX != cursorX) or (icursorY != cursorY)) {
    redrawMap(icursorX, icursorY);

  } 

  // redraw the cursor now
  redrawCursor(TFT_RED);

  delay(20);
}

int main() {
	setup();

	while (true) {
    processJoystick();
	}

	Serial.end();
	return 0;
}
