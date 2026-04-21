#include "display.h"
#include "config.h"
#include "debug.h"
#include <Arduino.h>

TFT_eSprite matrixSprite = TFT_eSprite(&tft);

uint8_t currentBrightness = BRIGHTNESS_DEFAULT;

// Resolved colour values — set in initColours()
static uint16_t colourOn  = 0;
static uint16_t colourOff = 0;

bool ledColourChanged = false;

void initColours() {
  colourOn  = tft.color565(COLOUR_LED_ON_R,  COLOUR_LED_ON_G,  COLOUR_LED_ON_B);
  colourOff = tft.color565(COLOUR_LED_OFF_R, COLOUR_LED_OFF_G, COLOUR_LED_OFF_B);

  // 16-bit sprite at 32 rows needs 288×192×2 = 110KB — too large for ESP32 without PSRAM.
  // 8-bit (RGB332) halves that to 55KB, matching the old 16-row footprint, and is pushed
  // to the 16-bit TFT by TFT_eSPI with automatic colour expansion.
  matrixSprite.setColorDepth(8);
  void* buf = matrixSprite.createSprite(LED_WIDTH * PIXEL_SIZE, LED_HEIGHT * PIXEL_SIZE);
  if (!buf) {
    DBG_ERROR("Sprite alloc FAILED — heap %d bytes, max block %d bytes",
              ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  }
  matrixSprite.fillSprite(colourOff);

  // Fill the screen background (border strips around the matrix)
  tft.fillScreen(tft.color565(5, 2, 0));

  DBG_INFO("Display colours initialised, sprite %dx%d depth=8 heap=%d",
           LED_WIDTH * PIXEL_SIZE, LED_HEIGHT * PIXEL_SIZE, ESP.getFreeHeap());
}

// Plot a single virtual LED into the sprite.
// Each LED occupies a PIXEL_SIZE × PIXEL_SIZE cell.
// We draw a PIXEL_INNER × PIXEL_INNER rect centred in that cell for the glow look.
void plot(int x, int y, bool on) {
  if (x < 0 || x >= LED_WIDTH || y < 0 || y >= LED_HEIGHT) return;

  uint16_t colour = on ? colourOn : colourOff;
  int px = x * PIXEL_SIZE + PIXEL_MARGIN;
  int py = y * PIXEL_SIZE + PIXEL_MARGIN;
  matrixSprite.fillRoundRect(px, py, PIXEL_INNER, PIXEL_INNER, 1, colour);
}

void pushMatrix() {
  tft.startWrite();
  matrixSprite.pushSprite(LED_X_OFFSET, LED_Y_OFFSET);
  tft.endWrite();
}

void cls() {
  matrixSprite.fillSprite(colourOff);
  // Re-draw the off-state pixel cells so the grid pattern is visible
  for (int x = 0; x < LED_WIDTH; x++) {
    for (int y = 0; y < LED_HEIGHT; y++) {
      int px = x * PIXEL_SIZE + PIXEL_MARGIN;
      int py = y * PIXEL_SIZE + PIXEL_MARGIN;
      matrixSprite.fillRoundRect(px, py, PIXEL_INNER, PIXEL_INNER, 1, colourOff);
    }
  }
}

void clsNow() {
  cls();
  pushMatrix();
}

void setBrightness(uint8_t pwm) {
  currentBrightness = pwm;
  ledcWrite(TFT_BL, pwm);
}

void fade_down() {
  for (int i = currentBrightness; i >= 0; i -= 8) {
    ledcWrite(TFT_BL, (uint8_t)i);
    delay(FADE_DELAY);
  }
  ledcWrite(TFT_BL, 0);
  cls();
  pushMatrix();
  ledcWrite(TFT_BL, currentBrightness);
}

void fade_up() {
  for (int i = 0; i <= currentBrightness; i += 8) {
    ledcWrite(TFT_BL, (uint8_t)i);
    delay(FADE_DELAY);
  }
  ledcWrite(TFT_BL, currentBrightness);
}

void setLedColours(uint8_t onR, uint8_t onG, uint8_t onB,
                   uint8_t offR, uint8_t offG, uint8_t offB) {
  colourOn  = tft.color565(onR, onG, onB);
  colourOff = tft.color565(offR, offG, offB);
  // Immediately repaint all off-state pixels so the new colourOff is visible at once.
  // ledColourChanged tells each mode loop to repaint its on-state pixels on the next pass.
  cls();
  pushMatrix();
  ledColourChanged = true;
}

void updateBrightness() {
#if LDR_ENABLED
  static uint16_t buf[LDR_SAMPLES] = {};
  static byte     idx  = 0;
  static bool     full = false;

  buf[idx] = (uint16_t)analogRead(LDR_PIN);
  idx = (idx + 1) % LDR_SAMPLES;
  if (idx == 0) full = true;

  byte     count = full ? LDR_SAMPLES : idx;
  if (count == 0) return;  // no samples yet

  uint32_t sum = 0;
  for (byte i = 0; i < count; i++) sum += buf[i];
  uint16_t avg = (uint16_t)(sum / count);

  uint8_t b = (uint8_t)constrain(
    map((long)avg, (long)LDR_DARK, (long)LDR_BRIGHT,
        (long)BRIGHTNESS_MIN, (long)BRIGHTNESS_MAX),
    (long)BRIGHTNESS_MIN, (long)BRIGHTNESS_MAX
  );
  setBrightness(b);
  DBG_VERBOSE("LDR avg=%u → brightness=%u", (unsigned)avg, (unsigned)b);
#endif
}
