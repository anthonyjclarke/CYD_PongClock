#pragma once
// display.h — Virtual LED matrix on TFT_eSPI

#include <TFT_eSPI.h>

extern TFT_eSPI tft;
extern TFT_eSprite matrixSprite;  // 288×96 off-screen buffer for the LED matrix

// Initialise colour constants (call once after tft.init())
void initColours();

// Plot a virtual LED at matrix coordinates (0..47, 0..15). on=true lights it.
// Writes into matrixSprite — call pushMatrix() to send to screen.
void plot(int x, int y, bool on);

// Push matrixSprite to screen at correct offset
void pushMatrix();

// Clear the matrix sprite to all-off colour (does NOT push to screen)
void cls();

// Clear and push immediately
void clsNow();

// Fade backlight down, clear matrix, restore brightness
void fade_down();

// Fade backlight up to current brightness level
void fade_up();

// Set backlight brightness 0–255
void setBrightness(uint8_t pwm);

// Current brightness (PWM 0-255)
extern uint8_t currentBrightness;

// Read LDR (GPIO34), apply rolling average, update backlight brightness.
// No-op if LDR_ENABLED=0 in config.h. Call periodically from mode loops.
void updateBrightness();
