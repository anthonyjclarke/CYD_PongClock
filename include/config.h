#pragma once
// config.h — PongClock CYD user-tuneable constants

// ── Display ───────────────────────────────────────────────────────────────────
#define SCREEN_ROTATION     1           // 1 = landscape (320×240)
#define BRIGHTNESS_DEFAULT  180         // 0–255 backlight PWM
#define BRIGHTNESS_STEPS    4           // number of brightness levels cycled on long-press

// ── Virtual LED Matrix ────────────────────────────────────────────────────────
// Emulates 2× Sure 2416 panels side-by-side: 48 cols × 16 rows
#define LED_WIDTH           48
#define LED_HEIGHT          32
#define PIXEL_SIZE          6           // px per virtual LED (48*6=288 wide, 32*6=192 tall)
#define LED_X_OFFSET        16          // (320 - 288) / 2 = 16
#define LED_Y_OFFSET        24          // (240 - 192) / 2 = 24

// ── Colours ───────────────────────────────────────────────────────────────────
// Amber LED-on / dark amber LED-off for retro look
// Computed at runtime via tft.color565() — see display.cpp for actual uint16_t values
#define COLOUR_LED_ON_R     255
#define COLOUR_LED_ON_G     140
#define COLOUR_LED_ON_B     0
#define COLOUR_LED_OFF_R    20
#define COLOUR_LED_OFF_G    8
#define COLOUR_LED_OFF_B    0

// ── Pixel shape ───────────────────────────────────────────────────────────────
// PIXEL_SIZE 6: draw a 4×4 rounded rect centred in the 6×6 cell for glow look
#define PIXEL_INNER         4           // inner lit square size
#define PIXEL_MARGIN        1           // margin each side: (6-4)/2

// ── Touch ─────────────────────────────────────────────────────────────────────
// XPT2046 is on a dedicated VSPI bus (separate from the display HSPI)
#define TOUCH_SCLK          25          // VSPI CLK
#define TOUCH_MISO          39          // VSPI MISO (input-only GPIO — never output)
#define TOUCH_MOSI          32          // VSPI MOSI
// TOUCH_CS = 33 is set via build flag -DTOUCH_CS=33 in platformio.ini

#define TOUCH_PRESSURE_MIN  200         // minimum Z pressure threshold
#define TOUCH_X_LEFT        200         // raw X at left edge (calibrate if needed)
#define TOUCH_X_RIGHT       3800        // raw X at right edge
#define TOUCH_X_MID         ((TOUCH_X_LEFT + TOUCH_X_RIGHT) / 2)  // split point for L/R tap
#define TOUCH_LONG_PRESS_MS 600         // ms held to trigger long-press (brightness)
#define TOUCH_DEBOUNCE_MS   300         // ms between tap events

// ── WiFi ──────────────────────────────────────────────────────────────────────
#define WIFI_AP_NAME        "CYD-PongClock"
#define WIFI_TIMEOUT_S      60

// ── NTP / Time ────────────────────────────────────────────────────────────────
#define NTP_TIMEZONE        "Australia/Sydney"
// POSIX fallback used when the ezTime Olson name lookup (timezoneapi.io HTTP call) fails.
// Must be kept in sync with NTP_TIMEZONE manually.
// Australia/Sydney: AEST=UTC+10 standard, AEDT=UTC+11 DST
//   DST starts first Sunday in October at 02:00
//   DST ends   first Sunday in April    at 03:00
#define NTP_POSIX_FALLBACK  "AEST-10AEDT,M10.1.0,M4.1.0/3"
#define NTP_SYNC_TIMEOUT_S  20

// ── Clock behaviour ───────────────────────────────────────────────────────────
#define NUM_MODES           4           // 0=Slide 1=Pong 2=Digits 3=WordClock
#define AMPM_MODE           0           // 0=24 h, 1=12 h
#define BAT_HEIGHT          8           // pong bat height in LED rows
#define SLIDE_DELAY         20          // ms per slide animation frame
#define PONG_BALL_DELAY     20          // ms per pong frame
#define FADE_DELAY          25          // ms per fade brightness step
#define DATE_DISPLAY_MINS   10          // minutes between automatic date displays (Pong/Digits/Word modes)

// ── Auto-brightness (LDR) ─────────────────────────────────────────────────────
// Set LDR_ENABLED=0 to disable auto-brightness and use long-press steps only.
// When enabled, the long-press cycle still works but is overridden on the next LDR sample.
#define LDR_ENABLED         0           // 1 = auto-brightness from LDR; 0 = long-press only
#define LDR_PIN             34          // GPIO34 — ADC input-only, CYD light sensor
#define LDR_DARK            200         // ADC ≤ this value → BRIGHTNESS_MIN
#define LDR_BRIGHT          2500        // ADC ≥ this value → BRIGHTNESS_MAX
#define BRIGHTNESS_MIN      15          // minimum backlight PWM in auto mode (0–255)
#define BRIGHTNESS_MAX      255         // maximum backlight PWM in auto mode (0–255)
#define LDR_SAMPLES         8           // rolling-average depth (smooths ADC noise)
#define LDR_UPDATE_MS       5000        // ms between LDR samples

// ── Per-mode time debug ───────────────────────────────────────────────────────
// When enabled, each mode prints current time to serial on every time change.
// Set to 0 to suppress; non-zero enables. DEBUG_LEVEL must be >= 3 (INFO).
#define DEBUG_SLIDE_TIME    1   // print HH:MM:SS on each seconds tick in Slide mode
#define DEBUG_DIGITS_TIME   1   // print HH:MM on each minutes tick in Digits mode
#define DEBUG_PONG_TIME     1   // print HH:MM on each restart (minute boundary) in Pong mode
#define DEBUG_WORD_TIME     1   // print word-clock phrase on each minutes tick in Word mode

// ── Debug ─────────────────────────────────────────────────────────────────────
#ifndef DEBUG_LEVEL
  #define DEBUG_LEVEL 3
#endif
