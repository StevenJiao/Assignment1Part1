/*

Name: Steven Jiao, Tyler Mah
CCID: 1442672, ***ADD CCID***
CMPUT 275, Winter 2019
Date: 02/08/2020
Acknowledgements: N/A
Assignment 1 Part 2: Mode 1 

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

int main() {
	setup();


	Serial.end();
	return 0;
}