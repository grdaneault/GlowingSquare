/*
 *
 *
 *         _             ______
     /\   | |           |  ____|
    /  \  | | _____  __ | |__ ___  _ __ ___ _   _
   / /\ \ | |/ _ \ \/ / |  __/ _ \| '__/ _ \ | | |
  / ____ \| |  __/>  <  | | | (_) | | |  __/ |_| |
 /_/    \_\_|\___/_/\_\ |_|  \___/|_|  \___|\__, |
                                             __/ |
 Glowing Square: Unifi Display              |___/
 For ESP32
 display.h
 *
 */

// We will use PxMatrix in double-buffer mode to allow us
// to change many pixels and then update the display once in one go
#define PxMATRIX_DOUBLE_BUFFER true

// Include must go here because double_buffer muse be defined first
#include "PxMatrix/PxMatrix.h"
#include "Fonts/TomThumb.h"

//
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#define MATRIX_WIDTH 128
#define MATRIX_HEIGHT 64
#define SCROLL_DELAY 50

#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 26
#define P_OE 16

// This defines the 'on' time of the display is us. The larger this number,
// the brighter the display. If too large the ESP will crash
#define DISPLAY_DRAW_TIME 20 // 30-70 is usually fine

// Create our display with the correct pins
// LAT=22, OE=2, A=19, B=23, C=18, D=5 (if there's an E it would be 15)
PxMATRIX display(MATRIX_WIDTH, MATRIX_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C, P_D, P_E);

// Some standard colors
const uint16_t red = display.color565(200, 0, 0);
const uint16_t green = display.color565(0, 200, 0);
const uint16_t blue = display.color565(0, 175, 200);
const uint16_t purple = display.color565(125, 0, 200);
const uint16_t white = display.color565(255, 255, 255);
const uint16_t black = display.color565(0, 0, 0);

const uint16_t red_palette[] = {black, red};
const uint16_t green_palette[] = {black, green};
const uint16_t blue_palette[] = {black, blue};
const uint16_t white_palette[] = {black, white};

const uint16_t logo_palette_default[] = {black, purple, white, white, white, white, white, white, white};


// Display writer function from the example code
void IRAM_ATTR display_updater(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  display.display(DISPLAY_DRAW_TIME);
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Draw a red pixel at the bottom of the display if we're
// offline, to show power is still on etc.
void displayOffline() {

  display.clearDisplay();
  display.setTextColor(red);
  display.setCursor(0, 0);
  display.print("error fetching \nstats...");
  display.drawPixel(MATRIX_WIDTH - 1, MATRIX_HEIGHT - 1, red);
  display.showBuffer();
  display.copyBuffer();

}

// Function to clear all the pixels for a given row of text
// We have to do this otherwise the text always gets written over
// the previous text, instead of replacing it
void fillBlankRow(uint8_t ypos) {

  // Clear only the pixels on this part of the display
  for (int x = 0; x < MATRIX_WIDTH; x++) {
    for (int y = ypos; y < ypos + 8; y++) {
      display.drawPixel(x, y, black);
    }
  }
}


void drawScrollingText(uint8_t ypos, int xpos, String scrollingText, uint16_t color) {
  // Write the scrolling text in its current position
  display.setTextColor(color);
  display.setCursor(xpos, ypos);
  display.print(scrollingText);
}


void drawStaticText(uint8_t ypos, uint16_t offset, String staticText, uint16_t color) {
  // Draw the static text
  display.setCursor(offset, ypos);
  display.setTextColor(color, black);
  display.print(staticText);
}

void drawRightAlignedText(uint8_t xpos, uint8_t ypos, String text) {

  uint8_t characterWidth = 5;
  uint16_t textLength = text.length();

  uint16_t offset = xpos - textLength * (characterWidth + 1);

  display.setCursor(offset, ypos);
  display.print(text);

}

void drawStaticAndScrollingText(uint8_t ypos, const long startTime, const long now, String staticText, uint16_t staticColor, String scrollingText, uint16_t scrollingColor) {

    // Asuming 5 pixel average character width
    uint8_t characterWidth = 5;

    const uint8_t scrollDelay = 5000;

    uint16_t scrollingTextLength = scrollingText.length();
    uint16_t offset = staticText.length() * (characterWidth + 1);



    // Initial setup
    display.setTextSize(1);
    display.setRotation(0);

    const uint16_t totalStrLength = staticText.length() + scrollingTextLength;

    if (startTime == now
        || now - startTime < scrollDelay
        || totalStrLength * (characterWidth + 1) < MATRIX_WIDTH) {

        drawStaticText(ypos, offset, scrollingText, scrollingColor);
        drawStaticText(ypos, 0, staticText, staticColor);
    } else {
        int frame = (now - startTime - scrollDelay) / SCROLL_DELAY;
        int xpos = offset - frame;
        if (xpos > -(MATRIX_WIDTH + scrollingTextLength * characterWidth)) {
            drawScrollingText(ypos, xpos, scrollingText, scrollingColor);
            drawStaticText(ypos, 0, staticText, staticColor);
        } else {
            drawStaticText(ypos, offset, scrollingText, scrollingColor);
            drawStaticText(ypos, 0, staticText, staticColor);
        }
    }
}


void setupDisplay() {

  // Start up our display with a 1/32 scan rate
  display.begin(32);

  // This is required for my SRYLED P2.5 display to work properly
  display.setMuxDelay(0,2,0,0,0);

  // Empty anything in the display buffer
  display.flushDisplay();

  // We never want text to wrap in this project
  display.setTextWrap(false);

  display.setBrightness(currentDisplayBrightness);

  // More display setup bits taken from the double_buffer demo
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &display_updater, true);
  timerAlarmWrite(timer, 4000, true);
  timerAlarmEnable(timer);
}


void changeBrightnessBlocking(int fadeTime) {

  // Calculate how far we have to fade
  int change = targetDisplayBrightness - currentDisplayBrightness;

  // We have some fading to do
  if (change != 0) {

    // We fade at a 50Hz rate cos it makes the maths easier
    int step = change / (fadeTime / 20);

    // Prevent forever loops with 0 step
    if (step == 0 && change > 0) step = 1;
    if (step == 0 && change < 0) step = -1;

    Serial.printf("Fading from %i to %i brightness in steps of %i\n", currentDisplayBrightness, targetDisplayBrightness, step);

    while(currentDisplayBrightness != targetDisplayBrightness) {

      // This also supports steps being negative for fade downs
      currentDisplayBrightness += step;

      display.setBrightness(currentDisplayBrightness);

      // 50Hz there it is again
      delay(20);

      // In the case that the next step would take us past the desired target value
      if (abs(targetDisplayBrightness - currentDisplayBrightness) <= abs(step)) {

        // Stop there and set the display again
        currentDisplayBrightness = targetDisplayBrightness;
        display.setBrightness(currentDisplayBrightness);
      }

    }

    // Mark the fade as having completed
    currentDisplayBrightness = targetDisplayBrightness;

    Serial.println("Fade complete");

  }

}


void changeBrightnessNonBlocking() {

  int change = targetDisplayBrightness - currentDisplayBrightness;

  if (change != 0) {

    if (change > 0) {
      currentDisplayBrightness += 1;
    } else if (change < 0) {
      currentDisplayBrightness -= 1;
    }

    display.setBrightness(currentDisplayBrightness);

  }

}

void drawIcon(int x, int y, int width, int height, const uint8_t image[], const uint16_t palette[]) {
  int counter = 0;
  for (int yy = 0; yy < height; yy++) {
    for (int xx = 0; xx < width; xx++) {
      display.drawPixel(xx + x , yy + y, palette[image[counter]]);
      counter++;
    }
  }
}
