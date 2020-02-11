Name: Steven Jiao, Tyler Mah
CCID: 1442672, 1429548
CMPUT 275, Winter 2019
Date: 02/10/2020
Acknowledgements: N/A
Assignment 1 Part 1: mode 0 and mode 1

Included Files:
	* a1part1.cpp
	* Makefile
	* README

Accessories:
	* Arduino Mega2560
	* TFT Display with SD Card
	* Breadboard
	* Joystick

Wiring instructions:
	* Arduino 5V pin <-> positive bus bar <-> joystick 5V pin
	* Arduino GND pin <-> negative bus bar <-> joystick GND pin
	* Analog pin A8 <-> joystick pin VRy
	* Analog pin A9 <-> joystick pin VRx
	* Digital pin 53 <-> joystick pin SW
	* Mount the TFT Display on Arduino Mega2560

Run instructions:
	1. Unzip a1part1.zip and put a1part1.cpp and Makefile in the same folder as 		lcd_image.cpp and lcd_image.h (latter 2 are not provided in zip).
	2. Connect the Arduino to your computer via USB.
	3. Navigate to this directory in the terminal and enter the command "make && make upload". 
	4. An image from the middle of Edmonton on Google Maps is now on the TFT display and the cursor can be controlled with the joystick. Pushing the joystick further from it's center will make the cursor move 10 pixels whereas slightly pushing it will move it 3 pixels.

Notes:
	* Mode 0 displays the map and mode 1 displays the restaurant list. Pressing the joystick will switch between the two modes.
	* Touching the screen when in mode 0 will cause all restaurants in view to appear as black dots.
	* Clicking the joystick in mode 0 will switch to mode 1, opening a list of the 21 closest restaurants to the cursor. The selected restaurant is highlighted and can be changed by pressing up or down on the joystick. Pressing the joystick again will switch to mode 0, and show the map centered around the selected retaurant.
