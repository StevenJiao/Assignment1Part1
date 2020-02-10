/*
Name: Steven Jiao, Tyler Mah
CCID: 1442672, 1429548
CMPUT 275, Winter 2019
Date: 02/08/2020
Acknowledgements: N/A
Assignment 1 Part 2: Mode 1
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <SD.h>
#include <TouchScreen.h>

#define SD_CS 10
#define JOY_VERT  A9 // should connect A9 to pin VRx
#define JOY_HORIZ A8 // should connect A8 to pin VRy
#define JOY_SEL   53 // Digital pin 53 should connect to pin SW

// physical dimensions of the tft display (# of pixels)
#define DISPLAY_WIDTH  480
#define DISPLAY_HEIGHT 320

// touch screen pins, obtained from the documentaion
#define YP A3 // must be an analog pin, use "An" notation!
#define XM A2 // must be an analog pin, use "An" notation!
#define YM 9  // can be a digital pin
#define XP 8  // can be a digital pin

// dimensions of the part allocated to the map display
#define MAP_DISP_WIDTH (DISPLAY_WIDTH - 60)
#define MAP_DISP_HEIGHT DISPLAY_HEIGHT

#define REST_START_BLOCK 4000000
#define NUM_RESTAURANTS 1066

// These constants are for the 2048 by 2048 map.
#define MAP_WIDTH 2048
#define MAP_HEIGHT 2048
#define LAT_NORTH 53618581
#define LAT_SOUTH 53409531
#define LON_WEST -113686521
#define LONG_EAST -113334961

// calibration data for the touch screen, obtained from documentation
// the minimum/maximum possible readings from the touch point
#define TS_MINX 100
#define TS_MINY 120
#define TS_MAXX 940
#define TS_MAXY 920

// thresholds to determine if there was a touch
#define MINPRESSURE   10
#define MAXPRESSURE 1000

// joystick center position and deadzone size
#define JOY_CENTER   512
#define JOY_DEADZONE 64

MCUFRIEND_kbv tft;

// a multimeter reading says there are 300 ohms of resistance across the plate,
// so initialize with this to get more accurate readings
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// different than SD
Sd2Card card;

// Stores restaurant lat, lon, and rating
struct restaurant { 
    int32_t lat; // Stored in 1/100,000 degrees
    int32_t lon; // Stored in 1/100,000 degrees
    uint8_t rating; // 0-10; [2 = 1 star, 10 = 5 stars]
    char name[55]; // already null terminated on the SD card
};

// Stores restaurant index and distance to cursor
struct RestDist {
    uint16_t index; // index of restaurant from 0 to NUM_RESTAURANTS-1
    uint16_t dist; // Manhattan distance to cursor position
};

RestDist rest_dist[NUM_RESTAURANTS];

// Stores most recent block read from the SD card and it's number
restaurant storeBlock[8];
uint32_t recentBlockNum;

// swap function for insertion sort
void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

// insertion sort for sorting the restaurant struct
void iSort(RestDist* a[], int n) {
    for (int i = 1; i < n; i++) {
        for (int j = i; j > 0; j--) {
            swap(a[j], a[j-1]);
        }
    }
}

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
    return map(lat, LAT_NORTH, LAT_SOUTH, 0, MAP_WIDTH);
}

// Reads restaurant data from SD card
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

// Sorts restaurants by closest
void sortRest() {
    restaurant r;
    for (int16_t i = 0; i < NUM_RESTAURANTS-1; i++) {
        getRestaurant(i, &r);

        // Calculates Manhatten distance d((x1,y1),(x2,y2)) = |x1-x2| + |y1-y2|
        // replace 111 with <mode0.cpp> cursorX
        int dx = 111 - lon_to_x(r.lon);
        if (dx < 0) {
            dx = 0 - dx;
        }
        // replace 222 with <mode0.cpp> cursorY
        int dy = 222 - lat_to_y(r.lat);
        if (dy < 0) {
            dx = 0 - dy;
        }
        int mDist = dx + dy;
        rest_dist[i].dist = mDist;
        rest_dist[i].index = i;
    }

    // test if this works**
    iSort(rest_dist, NUM_RESTAURANTS-1);
}

// Stores selected restaurant
int selectedRest = 0;

// Displays the restaurant list
void updateDisplay() {
    restaurant r;
    tft.setCursor(0,0);
    for (int16_t i = 0; i < 22; i++) {
        getRestaurant(rest_dist[i].index, &r);
        if (i != selectedRest) {
            // white text on black background
            tft.setTextColor(0xFFFF, 0x0000);
        }
        else {
            // black text on white background
            tft.setTextColor(0x0000, 0xFFFF);
        }
        tft.print(r.name);
        tft.setCursor(0,15*i);
    }
}

// Displays initial restaurant list
void initialDisplay() {
    tft.setRotation(1);
    tft.fillScreen(0x0000);
    tft.setTextSize(2);
    tft.setTextWrap(false);

    selectedRest = 0;
    sortRest();
    updateDisplay();
}

void setup() {
    init();
    card.init();
    Serial.begin(9600);

    // tft display initialization
    uint16_t ID = tft.readID();
    tft.begin(ID);

    initialDisplay();

    // SD card initialization for raw reads
    Serial.print("Initializing SPI communication for raw reads...");
    if (!card.init(SPI_HALF_SPEED, SD_CS)) {
        Serial.println("failed! Is the card inserted properly?");
        while (true) {}
    }
    else {
        Serial.println("OK!");
    }
}

// Checks if joystick is used
void processJoyStick() {
    int yVal = analogRead(JOY_VERT);
    int buttonVal = digitalRead(JOY_SEL);

    if (yVal < JOY_CENTER - JOY_DEADZONE) {
        if (selectedRest > 0) {
            selectedRest -= 1;
        }
        updateDisplay();
    }
    else if (yVal > JOY_CENTER + JOY_DEADZONE) {
        if (selectedRest < 20) {
            selectedRest += 1;
        }
        updateDisplay();
    }

    if (buttonVal == LOW) {
        // switch to mode 0
    }
}

int main() {
    setup();

    while (true) {
        processJoyStick();
    }

    Serial.end();

    return 0;
}
