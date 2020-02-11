/*

Name: Steven Jiao, Tyler Mah
CCID: 1442672, 1429548
CMPUT 275, Winter 2019
Date: 02/08/2020
Acknowledgements: N/A
Assignment 1 Part 1: mode 0 and 1

*/

/* ------------------defines and includes related to Arduino --------------- */
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <SPI.h>
#include <SD.h>
#include "lcd_image.h"
#include <TouchScreen.h>

#define SD_CS 10
#define JOY_VERT  A9 // should connect A9 to pin VRx
#define JOY_HORIZ A8 // should connect A8 to pin VRy
#define JOY_SEL   53

MCUFRIEND_kbv tft;

// physical dimensions of the tft display (# of pixels)
#define DISPLAY_WIDTH  480
#define DISPLAY_HEIGHT 320
#define YEG_SIZE 2048

// dimensions of the part allocated to the map display
#define MAP_DISP_WIDTH (DISPLAY_WIDTH - 60)
#define MAP_DISP_HEIGHT DISPLAY_HEIGHT

lcd_image_t yegImage = { "yeg-big.lcd", YEG_SIZE, YEG_SIZE };

// joystick center position and deadzone size
#define JOY_CENTER   512
#define JOY_DEADZONE 64
#define CURSOR_SIZE 9

// touch screen pins, obtained from the documentaion
#define YP A3 // must be an analog pin, use "An" notation!
#define XM A2 // must be an analog pin, use "An" notation!
#define YM 9  // can be a digital pin
#define XP 8  // can be a digital pin

// restaurant SD block and the number of restaurants 
#define REST_START_BLOCK 4000000
#define NUM_RESTAURANTS 1066

// These constants are for the 2048 by 2048 map.
#define MAP_WIDTH 2048
#define MAP_HEIGHT 2048
#define LAT_NORTH 5361858l
#define LAT_SOUTH 5340953l
#define LON_WEST -11368652l
#define LONG_EAST -11333496l

// calibration data for the touch screen, obtained from documentation
// the minimum/maximum possible readings from the touch point
#define TS_MINX 100
#define TS_MINY 120
#define TS_MAXX 940
#define TS_MAXY 920

// thresholds to determine if there was a touch
#define MINPRESSURE   10
#define MAXPRESSURE 1000

// a multimeter reading says there are 300 ohms of resistance across the plate,
// so initialize with this to get more accurate readings
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// different than SD
Sd2Card card;

/* ------------------ Global variables ------------------------------------*/

// made yegMiddleX and yegMiddleY into global variables with 60 bits rightmost 
// column black on the display
int yegMiddleX = YEG_SIZE/2 - (DISPLAY_WIDTH - 60)/2;
int yegMiddleY = YEG_SIZE/2 - DISPLAY_HEIGHT/2;

// the cursor position on the display
int cursorX, cursorY;

// Stores restaurant lat, lon, and rating
struct restaurant { 
    int32_t lat; // Stored in 1/100,000 degrees
    int32_t lon; // Stored in 1/100,000 degrees
    uint8_t rating; // 0-10; [2 = 1 star, 10 = 5 stars]
    char name[55]; // already null terminated on the SD card
};

// Stores restaurant index and distance to cursor
struct RestDist {
    uint16_t dist; // Manhattan distance to cursor position
    uint16_t index; // index of restaurant from 0 to NUM_RESTAURANTS-1
};

// Stores the restaurants in memory for sorting and displaying
RestDist rest_dist[NUM_RESTAURANTS];

// Stores most recent block read from the SD card and it's number for
// faster getRestaurant calls
restaurant storeBlock[8];
uint32_t recentBlockNum;

// Stores selected restaurant
int selectedRest = 0;
// stores highlighted restaurant
int highlightIndex = 0;

/* ------------------------------functions ---------------------------------*/

/* --------------------------------mode 1-----------------------------------*/

// These functions convert between x/y map poisition and lat/lon
// (and vice versa.)
int32_t x_to_lon(int16_t x) {
    return map(x, 0, MAP_WIDTH, LON_WEST, LONG_EAST);
}

int32_t y_to_lat(int16_t y) {
    return map(y, 0, MAP_WIDTH, LAT_NORTH, LAT_SOUTH);
}

int16_t lon_to_x(int32_t lon) {
    return map(lon, LON_WEST, LONG_EAST, 0, MAP_WIDTH);
}

int16_t lat_to_y(int32_t lat) {
    return map(lat, LAT_NORTH, LAT_SOUTH, 0, MAP_HEIGHT);
}

// Reads restaurant data from SD card (fast version)
void getRestaurant(int restIndex, restaurant* restPtr) {
    uint32_t blockNum = REST_START_BLOCK + restIndex/8;

    if (blockNum != recentBlockNum) {
        recentBlockNum = blockNum;
        while (!card.readBlock(blockNum, (uint8_t*) storeBlock)) {
            Serial.println("Read block failed, trying again.");
        }
    }

    *restPtr = storeBlock[restIndex % 8];
}

// swap function for insertion sort
void swap(RestDist &a, RestDist &b) {
    RestDist temp = a;
    a = b;
    b = temp;
}

// insertion sort for sorting the restaurant struct as implemented from
// the pesudo-code on ECLASS
void iSort(RestDist a[], int n) {
  for (int i = 1; i < n; i++) {
      for (int j = i; j > 0 && (a[j-1].dist > a[j].dist); j--) {
          swap(a[j], a[j-1]);
      }
  }
}

// Sorts restaurants by closest manhattan distance
void sortRest() {
  // temporarily store the 64 bit restaurant
  restaurant rest_temp;
  for (int16_t i = 0; i < NUM_RESTAURANTS; i++) {
    getRestaurant(i, &rest_temp);

    // Calculates Manhatten distance d((x1,y1),(x2,y2)) = |x1-x2| + |y1-y2|
    rest_dist[i].dist = abs(lon_to_x(rest_temp.lon) - (yegMiddleX + cursorX))
      + abs(lat_to_y(rest_temp.lat) - (yegMiddleY + cursorY));

    // also store the index of the restaurant distance
    rest_dist[i].index = i;
  }

  // sort the restaurants by distance
  iSort(rest_dist, NUM_RESTAURANTS);   
}

// Displays the restaurant list
void updateHighlight() {
  // temporarily store 64 bit restaurants
  restaurant rest_temp;
  // check if the highlighted restaurant is the same as selected restaurant
  if (highlightIndex != selectedRest) {
    // if they're not equal, then change the highlighted line 
    // into non-highlighted
    getRestaurant(rest_dist[highlightIndex].index, &rest_temp);
    tft.setCursor(0, 15*highlightIndex);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print(rest_temp.name);

    // highlight the new line of restaurant 
    getRestaurant(rest_dist[selectedRest].index, &rest_temp);
    tft.setCursor(0, 15*selectedRest);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.print(rest_temp.name);
  }

  // now we are at the same selection and highlight
  highlightIndex = selectedRest;
}

// implementation of mode 1 as specified on Eclass; prints out a list of 
// 21 restaurants in order of proximity to cursor
void mode1() {
  // get the joy cursor y value for scrolling
  int yVal = analogRead(JOY_VERT);

  // if the cursor moves up or down, store its position on the scrolling list
  if (yVal < JOY_CENTER - JOY_DEADZONE) {
      if (selectedRest > 0) {
          selectedRest -= 1;
      }
      // update the highlight 
      updateHighlight();
  }
  else if (yVal > JOY_CENTER + JOY_DEADZONE) {
      if (selectedRest < 20) {
          selectedRest += 1;
      }
      updateHighlight();
  }
}


/* ------------------------------------mode 0 -----------------------------*/

// function for redrawing the cursor; just to look a little better
void redrawCursor(uint16_t colour) {
  tft.fillRect(cursorX - CURSOR_SIZE/2, cursorY - CURSOR_SIZE/2,
               CURSOR_SIZE, CURSOR_SIZE, colour);
}
// setup pertaining to both mode 0 and 1
void setupMain() {
	init();
  card.init();
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

  // SD card initialization for raw reads
  Serial.print("Initializing SPI communication for raw reads...");
  if (!card.init(SPI_HALF_SPEED, SD_CS)) {
      Serial.println("failed! Is the card inserted properly?");
      while (true) {}
  }
  else {
      Serial.println("OK!");
  }

	Serial.println("OK!");

	tft.setRotation(1);

	tft.fillScreen(TFT_BLACK);
}

// setup specific for mode0
void setupMode0() {
  // draws the centre of the Edmonton map,
  // leaving the rightmost 60 columns black
  lcd_image_draw(&yegImage, &tft, yegMiddleX, yegMiddleY,
                 0, 0, DISPLAY_WIDTH - 60, DISPLAY_HEIGHT);

  // initial cursor position is the middle of the screen
  cursorX = (DISPLAY_WIDTH - 60)/2;
  cursorY = DISPLAY_HEIGHT/2;

  redrawCursor(TFT_RED);
}

// setup specific for mode1
void setupMode1() {
  // initial setup of the screen and text size
  tft.fillScreen(0x0000);
  tft.setTextSize(2);
  tft.setTextWrap(false);

  // make the index start from the top
  selectedRest = 0;
  // get the list of restaurants, and sort them
  sortRest();

  // print initial list of restaurants from restaurants stored in memory
  restaurant rest_temp;
  for (int16_t i = 0; i < 21; i++) {
    getRestaurant(rest_dist[i].index, &rest_temp);
    tft.setCursor(0,15*i);
    // initially highlight first selection
    if (i == 0) {
      tft.setTextColor(0x0000, 0xFFFF);
    } else {
      tft.setTextColor(0xFFFF, 0x0000);
    }
      tft.print(rest_temp.name);
  }
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

// implementation of mode0, map movement
void mode0() {
  int xVal = analogRead(JOY_HORIZ);
  int yVal = analogRead(JOY_VERT);

  // get the initial cursor positions 
  int icursorX = cursorX;
  int icursorY = cursorY;

  // now move the cursor
  if (yVal < JOY_CENTER - JOY_DEADZONE) {
    // if the joystick is moved far enough, the cursor will go faster
    if (yVal > JOY_CENTER - JOY_DEADZONE - 350) {
      cursorY -= 3; 
    } else {
      cursorY -= 10;
    }
  }
  else if (yVal > JOY_CENTER + JOY_DEADZONE) {
    if (yVal < JOY_CENTER + JOY_DEADZONE + 350) {
      cursorY += 3; 
    } else {
      cursorY += 10;
    }
  }

  // remember the x-reading increases as we push left
  if (xVal > JOY_CENTER + JOY_DEADZONE) {
    if (xVal < JOY_CENTER + JOY_DEADZONE + 350) {
      cursorX -= 3; 
    } else {
      cursorX -= 10;
    }

  }
  else if (xVal < JOY_CENTER - JOY_DEADZONE) {
    if (xVal > JOY_CENTER - JOY_DEADZONE - 350) {
      cursorX += 3; 
    } else {
      cursorX += 10;
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

// shows the closest restaurants thats on the screen at the moment
void showRest() {
  // get the closest restaurants to the cursor right now
  sortRest();
  // get restaurant name from the SD card
  restaurant rest_temp;
  // look for all restaurants thats within the maximum manhattan distance 
  for (int i = 0; i < NUM_RESTAURANTS && 
          (rest_dist[i].dist < (MAP_DISP_WIDTH/2 + MAP_DISP_HEIGHT/2)); i++) {

    getRestaurant(rest_dist[i].index, &rest_temp);
    // draw a black circle at the restaurant
    tft.fillCircle(lon_to_x(rest_temp.lon) - (yegMiddleX),
      lat_to_y(rest_temp.lat) - (yegMiddleY), CURSOR_SIZE/3, TFT_BLACK);
  }
}

int main() {
  // main setup
	setupMain();
  // check if in mode 0 or 1; mode0 is true
  bool isMode0 = true;

	while (true) {

    if (isMode0 == true) {
      // run mode0 setup if in mode0
      setupMode0();
      // loop the mode0 implementation
      while (isMode0 == true) {
        // get reading from TS then reset pins
        TSPoint touch = ts.getPoint();
        pinMode(YP, OUTPUT);
        pinMode(XM, OUTPUT);
        // if the screen is tapped, then show the closest restaurants
        if (touch.z >= MINPRESSURE) {
          showRest();
        }

        mode0();

        // if the button is pressed, go to mode 1
        if (digitalRead(JOY_SEL) == LOW) {
          isMode0 = false;
        }
      }
    }

    // run mode 1
    if (isMode0 == false) {
      setupMode1();

      while(isMode0 != true) {

        mode1();

        // if the button is pressed, set the coordinates so we run mode 0 
        // and have the cursor at the selected restaurant
        if (digitalRead(JOY_SEL) == LOW) {
          restaurant rest_temp;
          getRestaurant(rest_dist[selectedRest].index, &rest_temp);

          yegMiddleX = lon_to_x(rest_temp.lon) - cursorX;
          yegMiddleY = lat_to_y(rest_temp.lat) - cursorY;
          // go back to mode 0
          isMode0 = true;
        }
      }
    }
  }

	Serial.end();
	return 0;
}