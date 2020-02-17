/*

Name: Steven Jiao, Tyler Mah
CCID: 1442672, 1429548
CMPUT 275, Winter 2019
Date: 02/08/2020
Acknowledgements: N/A
Assignment 1 Part 2: Full Version of Restaurant Finder

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

// store the sorting modes
enum whichSort { 
  QSORT, ISORT, BOTH 
};
// Initially set to quick sort
whichSort sortMode = QSORT;

// store rating mode
int ratingMode = 1;

// made yegMiddleX and yegMiddleY into global variables with 60 bits rightmost 
// column black on the display
int yegMiddleX = YEG_SIZE/2 - MAP_DISP_WIDTH/2;
int yegMiddleY = YEG_SIZE/2 - DISPLAY_HEIGHT/2;

// the cursor position on the display initially in middle
int cursorX = MAP_DISP_WIDTH/2; 
int cursorY = DISPLAY_HEIGHT/2;

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
// store number of restaurants in the rest_dist with rating threshold 
uint16_t rest_dist_num = NUM_RESTAURANTS;

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
    return map(x, 0, YEG_SIZE, LON_WEST, LONG_EAST);
}

int32_t y_to_lat(int16_t y) {
    return map(y, 0, YEG_SIZE, LAT_NORTH, LAT_SOUTH);
}

int16_t lon_to_x(int32_t lon) {
    return map(lon, LON_WEST, LONG_EAST, 0, YEG_SIZE);
}

int16_t lat_to_y(int32_t lat) {
    return map(lat, LAT_NORTH, LAT_SOUTH, 0, YEG_SIZE);
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

// Actual sorting of quick sort; implemented from pseudocode on ECLASS
int pivot(RestDist a[], int n, int pi) {
  // swap the pivot with the last term
  swap(a[pi], a[n-1]);
  // store high and low index
  int lo = 0;
  int hi = n-2;

  // iterate until lo > hi
  while (lo <= hi) {
    // if lo index distance is less than or equal to pivot's, move low index up
    if (a[lo].dist <= a[n-1].dist) {
      lo++;
    // if hi index distance is greater than pivot's, move high down
    } else if (a[hi].dist > a[n-1].dist) {
      hi--;
    // else swap high and low 
    } else {
      swap(a[lo], a[hi]);
    }
  }
  // swap the pivot back into position
  swap(a[lo], a[n-1]);

  // return the new low index
  return lo;
}

// quick sort implementation using recursion of a RestDist struct 
void qSort(RestDist a[], int n) {
  // if n <=1 then do nothing
  if (n > 1) {

    // Initial pivot set to be at the half-way point (a little above half)
    int pi = n/2;

    // store the new pivot after pivoting the array
    int new_pi = pivot(a, n, pi);

    // recursively sort the bottom portion before pivot
    qSort(a, new_pi);
    // recursively sort top portion after pivot
    qSort(&a[new_pi+1], n-new_pi-1);
  }
}

// converts ratings of restaurants from rating/10 to rating/5
int ratingConverter(int outOfTen) {
  // floor division
  int outOfFive = (outOfTen + 1)/2;
  // if equals 0, return 1
  if (outOfFive == 0) {
    return 1;
  // else return the rating/5
  } else {
    return outOfFive;
  }
}

// Insertion sorting of the RestDist array and also timing iSort
void sortByInsertion() {
  // first, go through all the restaurants stored on the SD card
  // temporarily store the 64 bit restaurant
  restaurant rest_temp;
  // set the index of the RestDist struct to 0
  uint16_t restIndex = 0;
  // initialize rating 
  int rating;
  // first, go through all the restaurants stored on the SD card
  for (int16_t i = 0; i < NUM_RESTAURANTS; i++) {
    getRestaurant(i, &rest_temp);
    // get the rating of the i'th restaurant
    rating = ratingConverter(rest_temp.rating);

    // if its rating is above threshold currently set, store it 
    if (rating >= ratingMode) {
      // Calculates Manhatten distance d((x1,y1),(x2,y2)) = |x1-x2| + |y1-y2|
      rest_dist[restIndex].dist = abs(lon_to_x(rest_temp.lon) - (yegMiddleX + cursorX))
      + abs(lat_to_y(rest_temp.lat) - (yegMiddleY + cursorY));
      // store the index of the restaurant distance
      rest_dist[restIndex].index = i;
      // increase the index of the rest_dist array
      restIndex++;
    }
  }

  // if the total restaurants above threshold is not the one stored, change it
  if (restIndex != rest_dist_num) {
    rest_dist_num = restIndex;
  }

  // Getting ready to print the running time
  Serial.print("InsertionSort running time of ");
  Serial.print(rest_dist_num);
  Serial.print(" restaurants: ");
  // get initial run-time
  unsigned long initialTimer = millis();

  // sort by insertion sort
  iSort(rest_dist, rest_dist_num);

  // print the running time
  Serial.print(millis() - initialTimer);
  Serial.println(" ms");
}

// Quick sorting of RestDist array and also timing qSort
void sortByQuick() {
  // below is the same code as in "sortByInsertion()" for getting restaurants
  // temporarily store the 64 bit restaurant
  restaurant rest_temp;
  // store the index of rest_dist 
  uint16_t restIndex = 0;
  int rating;
  for (int16_t i = 0; i < NUM_RESTAURANTS; i++) {
    getRestaurant(i, &rest_temp);
    rating = ratingConverter(rest_temp.rating);
    if (rating >= ratingMode) {
      // Calculates Manhatten distance d((x1,y1),(x2,y2)) = |x1-x2| + |y1-y2|
      rest_dist[restIndex].dist = abs(lon_to_x(rest_temp.lon) - (yegMiddleX + cursorX))
      + abs(lat_to_y(rest_temp.lat) - (yegMiddleY + cursorY));
      // store the index of the restaurant distance
      rest_dist[restIndex].index = i;
      restIndex++;
    }
  }

  if (restIndex != rest_dist_num) {
    rest_dist_num = restIndex;
  }

  // Getting ready to print the running time
  Serial.print("QuickSort running time of ");
  Serial.print(rest_dist_num);
  Serial.print(" restaurants: ");
  // get initial run-time
  unsigned long initialTimer = millis();

  // sort by insertion sort
  qSort(rest_dist, rest_dist_num);

  // print the running time
  Serial.print(millis() - initialTimer);
  Serial.println(" ms");
}

// Implements which sort to use depending on the sorting mode chosen
void sortRest() {
  // sorts by sorting algo currently set in sortMode
  if (sortMode == QSORT) {
    sortByQuick();
  } else if (sortMode == ISORT) {
    sortByInsertion();
  } else if (sortMode == BOTH) {
    // first sort by insertion
    sortByQuick();
    // sort by insertion as well
    sortByInsertion();
  }
}

// prints list of 21 restaurants
void restList() {
  // makes screen black
  tft.fillScreen(TFT_BLACK);
  // print initial list of restaurants from restaurants stored in memory
  restaurant rest_temp;
  // topIndex is lowest multiple of 21 less than or equal to selectedRest
  int topIndex = selectedRest/21 * 21;
  // check if list was scrolled back a page
  int highlightRest = 0;
  if (selectedRest < highlightIndex) {
    highlightRest = 20;
  }
  for (int16_t i = 0; i < 21; i++) {
    getRestaurant(rest_dist[topIndex+i].index, &rest_temp);
    tft.setCursor(0,15*i);
    // initially highlight first selection
    // unless the list was scrolled back a page
    if (i == highlightRest) {
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
    } else {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }
      tft.print(rest_temp.name);
  }
}

// updates which restaurant is highlighted and removes highlight if necessary
void updateHighlight() {
  // temporarily store 64 bit restaurants
  restaurant rest_temp;
  // check if the highlighted restaurant is the same as selected restaurant
  if (highlightIndex != selectedRest) {
    // if they're not equal, then change the highlighted line 
    // into non-highlighted
    getRestaurant(rest_dist[highlightIndex].index, &rest_temp);
    tft.setCursor(0, 15*(highlightIndex%21));
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print(rest_temp.name);

    // highlight the new line of restaurant 
    getRestaurant(rest_dist[selectedRest].index, &rest_temp);
    tft.setCursor(0, 15*(selectedRest%21));
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
    // topIndex is lowest multiple of 21 less than or equal to selectedRest
    int prevTopIndex = selectedRest/21 * 21;
    if (selectedRest > 0) {
      selectedRest -= 1;
    }
    // checks if should load previous page
    int newTopIndex = selectedRest/21 * 21;
    if (newTopIndex < prevTopIndex){
      restList();
      highlightIndex -= 1;
    }
    // update the highlight 
    updateHighlight();
  }
  else if (yVal > JOY_CENTER + JOY_DEADZONE) {
    // lowest multiple of 21 less than or equal to selectedRest
    int prevTopIndex = selectedRest/21 * 21;
      if (selectedRest < NUM_RESTAURANTS-1) {
        selectedRest += 1;
        // checks if should load next page
        int newTopIndex = selectedRest/21 * 21;
        if (newTopIndex > prevTopIndex){
          restList();
          highlightIndex += 1;
        }
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

// Prints which button is selected
void printSortButton(char toPrint[]) {
  for (int i = 0; i < 5; i++) {
    tft.setCursor(MAP_DISP_WIDTH + 25, DISPLAY_HEIGHT * 6/10 + (i * 23));
    tft.print(toPrint[i]);
  }
}

// prints the sorting buttons depending on which one is selected
void updateSortUI() {
  // set text settings for black background and size
  tft.setTextColor(TFT_WHITE, TFT_BLACK); 
  tft.setTextSize(3);

  // temporarily store char to print to not take up excessive memory
  if (sortMode == QSORT) {
    char printSort[] = "QSORT";
    printSortButton(printSort);
  } else if (sortMode == ISORT) {
    char printSort[] = "ISORT";
    printSortButton(printSort);
  } else if (sortMode == BOTH) {
    char printSort[] = "BOTH ";
    printSortButton(printSort);
  }
}

// scrolls through the different sorting modes if called
void changeSort() {
  if (sortMode == QSORT) {
    sortMode = ISORT;
  } else if (sortMode == ISORT) {
    sortMode = BOTH;
  } else if (sortMode == BOTH) {
    sortMode = QSORT;
  }
  // after, update the UI
  updateSortUI();
}

// updates the rating button UI
void updateRatingUI() {
  // set the cursor to specified settings for printing
  tft.setCursor(MAP_DISP_WIDTH + 25, DISPLAY_HEIGHT/10 + (2 * 23));
  tft.setTextColor(TFT_WHITE, TFT_BLACK); 
  tft.setTextSize(3);

  // then, print the rating mode its in
  tft.print(ratingMode); 
}

// scrolls through the ratings if called
void changeRating() {
  if (ratingMode == 1) {
    ratingMode = 2;
  } else if (ratingMode == 2) {
    ratingMode = 3;
  } else if (ratingMode == 3) {
    ratingMode = 4;
  } else if (ratingMode == 4) {
    ratingMode = 5;
  } else if (ratingMode == 5) {
    ratingMode = 1;
  }
  // then, update the UI with new rating
  updateRatingUI();
}

// draw the initial UI on the TFT display for the buttons to the right
void drawInitialButtonUI() {
  // fill the rightmost button to "erase" previous stuff
  tft.fillRect(MAP_DISP_WIDTH, 0, 60, DISPLAY_HEIGHT, TFT_BLACK);
  // draw top rectangle button on right side
  tft.drawRect(MAP_DISP_WIDTH, 0, 60, DISPLAY_HEIGHT/2,TFT_BLUE);
  // draw bottom rectangle button
  tft.drawRect(MAP_DISP_WIDTH, DISPLAY_HEIGHT/2, 60, DISPLAY_HEIGHT/2,TFT_RED);

  // draw the buttons 
  updateRatingUI();
  updateSortUI();
}

// setup specific for mode0
void setupMode0() {
  // draw initial button UI
  drawInitialButtonUI();

  // draws the centre of the Edmonton map
  lcd_image_draw(&yegImage, &tft, yegMiddleX, yegMiddleY,
                 0, 0, MAP_DISP_WIDTH, DISPLAY_HEIGHT);

  // place cursor at position
  // cursorX = MAP_DISP_WIDTH/2;
  // cursorY = DISPLAY_HEIGHT/2;

  redrawCursor(TFT_RED);
}

// setup specific for mode1
void setupMode1() {
  // initial setup of the screen and text size
  tft.setTextSize(2);
  tft.setTextWrap(false);

  // make the index start from the top
  selectedRest = 0;
  highlightIndex = 0;
  // get the list of restaurants, and sort them
  sortRest();

  // print initial list of restaurants from restaurants stored in memory
  restList();
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
      yegMiddleX = yegMiddleX - MAP_DISP_WIDTH;

    // if cursor is at right of LCD display
  	} else if ((cursorX > (MAP_DISP_WIDTH - CURSOR_SIZE/2)) 
  					&& (yegMiddleX < (YEG_SIZE - MAP_DISP_WIDTH))) {

	    hasMoved = true;
      yegMiddleX = yegMiddleX + MAP_DISP_WIDTH;
  
  	}

  	// if we moved, then move the map if possible
  	if (hasMoved) {
  		// constrain the coordinates to 0 and the maximum size of the YEG 
  		// picture size subtracting the size of the LCD screen
  		yegMiddleX = constrain(yegMiddleX, 0, (YEG_SIZE - MAP_DISP_WIDTH));
  		yegMiddleY = constrain(yegMiddleY, 0, (YEG_SIZE - DISPLAY_HEIGHT));
  		lcd_image_draw(&yegImage, &tft, yegMiddleX, yegMiddleY,
                 0, 0, MAP_DISP_WIDTH, DISPLAY_HEIGHT);
  		
  		cursorX = MAP_DISP_WIDTH/2;
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

  // x-reading increases as we push left
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
    MAP_DISP_WIDTH - 1 - CURSOR_SIZE/2);
  cursorY = constrain(cursorY, CURSOR_SIZE/2, 
  	DISPLAY_HEIGHT - CURSOR_SIZE/2 - 1);

  // check if cursor has moved. If it has, redraw the map.
  if ((icursorX != cursorX) or (icursorY != cursorY)) {
    redrawMap(icursorX, icursorY);

  } 

  // redraw the cursor now
  redrawCursor(TFT_RED);

  // delay(20);
}

// shows the closest restaurants thats on the screen at the moment
void showRest() {
  // get the closest restaurants to the cursor right now
  sortRest();
  // get restaurant name from the SD card
  restaurant rest_temp;
  // for storing x and y LCD positions of the restaurants
  uint16_t xLCDPos;
  uint16_t yLCDPos;

  // look for all restaurants in rest_dist thats within the screen 
  // and only getRestaurant of the ones within the vicinity
  for (unsigned int i = 0; (i < rest_dist_num) 
          && (rest_dist[i].dist < (MAP_DISP_WIDTH + DISPLAY_HEIGHT)); i++) {

    getRestaurant(rest_dist[i].index, &rest_temp);

    // get the x and y coordinates relative to LCD screen of current restaurant
    xLCDPos = lon_to_x(rest_temp.lon) - (uint16_t) yegMiddleX;
    yLCDPos = lat_to_y(rest_temp.lat) - (uint16_t) yegMiddleY;

    // if its within the left-most screen, draw a circle 
    if ((xLCDPos > 0) && (xLCDPos < MAP_DISP_WIDTH) 
          && (yLCDPos > 0) && (yLCDPos < DISPLAY_HEIGHT)){
      // draw a black circle at the restaurant
      tft.fillCircle(xLCDPos, yLCDPos, CURSOR_SIZE/3, TFT_BLACK);
    }
  }
}

// implements the movement of the cursor to the restaurant selected in mode1
// as well as taking care of the 2 boundary cases
void movetoRest() {
  // get the selected restaurant's lat and lon for mapping 
  restaurant rest_temp;
  getRestaurant(rest_dist[selectedRest].index, &rest_temp);

  // map screen to position to the restaurant selected with cursor in middle
  yegMiddleX = lon_to_x(rest_temp.lon) - MAP_DISP_WIDTH/2;
  yegMiddleY = lat_to_y(rest_temp.lat) - DISPLAY_HEIGHT/2;

  // constrain the map so we don't go off the yeg map
  yegMiddleX = constrain(yegMiddleX, 0, (YEG_SIZE - MAP_DISP_WIDTH));
  yegMiddleY = constrain(yegMiddleY, 0, (YEG_SIZE - DISPLAY_HEIGHT));

  // map cursor to the restaurant 
  cursorX = lon_to_x(rest_temp.lon) - yegMiddleX;
  cursorY = lat_to_y(rest_temp.lat) - yegMiddleY;

  // constrain the cursor to the lcd screen size
  cursorX = constrain(cursorX, CURSOR_SIZE/2, 
    MAP_DISP_WIDTH - 1 - CURSOR_SIZE/2);
  cursorY = constrain(cursorY, CURSOR_SIZE/2, 
    DISPLAY_HEIGHT - CURSOR_SIZE/2 - 1);

}

// main program that runs either mode 0 or 1 along with handling 
// screen touches and joy-stick pushing 
void runProgram() {
  // check if in mode 0 or 1; mode0 is true and initally set to this
  bool isMode0 = true;

  if (isMode0 == true) {
    // run mode0 setup if in mode0
    setupMode0();
    // loop the mode0 implementation
    while (isMode0 == true) {
      // get reading from TS then reset pins
      TSPoint touch = ts.getPoint();
      pinMode(YP, OUTPUT);
      pinMode(XM, OUTPUT);
      
      // if screen is touched
      if (touch.z >= MINPRESSURE) {
        // if screen touched is on left 60 pixels, show restaurants
        if (touch.y > (TS_MAXY - TS_MINY)/4) {
          showRest();
        // if screen is touched on the bottom right 60 pixels, change sorting
        } else if ((touch.y < (TS_MAXY - TS_MINY)/4) && (touch.x < 500)) {
          changeSort();
        // if upper part, change the rating
        } else if ((touch.y < (TS_MAXY - TS_MINY)/4) && (touch.x > 500)) {
          changeRating();
        }
        // slow down the scrolling of ratings and sorting 
        delay(300);
      }

      // run mode0
      mode0();

      // if the button is pressed, go to mode 1
      if (digitalRead(JOY_SEL) == LOW) {
        isMode0 = false;
      }
    }
  }

  // check if we're in mode 1 
  if (isMode0 == false) {
    setupMode1();

    while(isMode0 == false) {
      // run mode1
      mode1();

      // if the button is pressed, set the coordinates so we run mode 0 
      // and have the cursor at the selected restaurant
      if (digitalRead(JOY_SEL) == LOW) {
        // move to the restaurant
        movetoRest();
        
        // go back to mode 0
        isMode0 = true;
      }
      // control scroll speed
      delay(100);
    }
  }
}

int main() {
  // main setup
	setupMain();

  // loop the main program
	while (true) {
    runProgram();
  }

	Serial.end();
	return 0;
}
