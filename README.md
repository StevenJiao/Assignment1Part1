Name: Steven Jiao, Tyler Mah
CCID: 1442672, 1429548
CMPUT 275, Winter 2019
Date: 02/10/2020
Acknowledgements: N/A
Assignment 1 Part 2: Full Version of Restaurant Finder

Included Files:
	* a1part2.cpp
	* Makefile
	* README
	* lcd_image.cpp
	* lcd_image.h

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
	1. Unzip a1part2.tar.gz and put all files into 1 directory
	2. Connect the Arduino to your computer via USB.
	3. Navigate to this directory in the terminal and enter the command "make && make upload && Serial-mon". 

Implementation Notes:
	* Mode 0 displays the map and mode 1 displays the restaurant list. Pressing the joystick will switch between the two modes.
	* Clicking the joystick in mode 0 will switch to mode 1, opening a list of the 21 closest restaurants to the cursor. The selected restaurant is highlighted and can be changed by pressing up or down on the joystick. Pressing the joystick again will switch to mode 0, and show the map centered around the selected retaurant.
	* Scrolling down the list in mode 1 when already at the bottom of any page except the last, will load the next 21 closest restaurants. Scrolling up while at the top of any page except page 1 will load the previous page of 21 closer restaurants.
	* Touching the screen when in mode 0 will cause all restaurants in view to appear as black dots.

Other Notes:
	* The cursor will overwrite the black dots if hovered over them
	* Some restaurant text that are very long will not be erased from the right-most 60 pixels of the display due to not clearing this portion in order to implement part 2 of assignment 1
	* Sorting occurs both when you touch the screen as well as when you press in the joystick for ease; no need to keep track when moving from screen to screen
	* Moving upward on the restaurant list in mode 1 will just stop at the first restaurant that shows up
