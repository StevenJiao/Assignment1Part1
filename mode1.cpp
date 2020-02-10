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
#define JOY_SEL   53

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

// Stores restaurant index and distance to cursor
struct RestDist {
    uint16_t index; // index of restaurant from 0 to NUM_RESTAURANTS-1
    uint16_t dist; // Manhattan distance to cursor position
}

// Stores most recent block read from the SD card and it's number
restaurant storeBlock[8];
uint32_t recentBlockNum;

// Stores selected restaurant
int selectedRest = 0;

// Displays initial restaurant list
void updateDisplay() {
    tft.fillScreen(0x0000);
    tft.setRotation(1);
    tft.setTextSize(2);
    tft.setTextWrap(false);
    tft.setCursor(0,0)

    selectedRest = 0;
    updateDisplay();
}

void updateDisplay() {
    for (int16_t i = 0; i < 21; i++) {
        Restaurant r;
        getRestaurant(restDist[i].index, &r);
        if (i != selectedRest) {
            // white text on black background
            tft.setTextColor(0xFFFF, 0x0000);
        }
        else {
            // black text on white background
            tft.setTextColor(0x0000, 0xFFFF);
        }
        tft.print(r.name);
        tft.print("\n");
    }
    tft.print("\n");
}

void setup() {
    init();
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

// Checks if joystick is used
void processJoyStick() {
    int yVal = analogRead(JOY_VERT);
    int buttonVal = digitalRead(JOY_SEL);

    if (yVal < JOY_CENTER + JOY_DEADZONE) {
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

    if (buttonVal == 1) {
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
