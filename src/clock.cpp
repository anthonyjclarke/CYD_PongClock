#include "clock.h"
#include "display.h"
#include "fonts.h"
#include "config.h"
#include "debug.h"

#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// ── Globals ───────────────────────────────────────────────────────────────────
byte rtc[7];
byte clock_mode = 0;
bool ampm       = AMPM_MODE;

// Touch controller on VSPI (separate from display HSPI)
static SPIClass            touchSPI(VSPI);
static XPT2046_Touchscreen ts(TOUCH_CS, 255); // 255 = no IRQ pin (polling mode)

void initTouch() {
  touchSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1); // match display rotation
  DBG_INFO("Touch initialised (VSPI CLK=%d MISO=%d MOSI=%d CS=%d)",
           TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
}

// Touch state
static uint32_t touchDownMs    = 0;
static bool     touchWasDown   = false;
static uint8_t  brightnessStep = 0;
static int16_t  touchDownX     = 0;  // raw X saved at first contact

// Date auto-display
static byte next_display_date  = 255; // invalid sentinel, set on first get_time

// ── Time ──────────────────────────────────────────────────────────────────────
void get_time() {
  // Use ezTime Timezone directly — localtime() uses UTC; myTZ applies local offset
  rtc[0] = myTZ.second();
  rtc[1] = myTZ.minute();
  rtc[2] = myTZ.hour();
  rtc[3] = myTZ.weekday() - 1;  // ezTime: 1=Sun → 0=Sun to match tm_wday
  rtc[4] = myTZ.day();
  rtc[5] = myTZ.month();
  rtc[6] = (byte)(myTZ.year() % 100);
}

// ── Touch + run_mode ──────────────────────────────────────────────────────────
// Polls touch hardware on every call.
// Saves position at first contact; handles long-press brightness in-place.
// Does NOT clear touchWasDown on release — checkTap() owns that transition.
bool run_mode() {
  bool touching = ts.touched();

  if (touching) {
    TS_Point p = ts.getPoint();
    if (p.z >= TOUCH_PRESSURE_MIN) {
      uint32_t now = millis();

      if (!touchWasDown) {
        // First frame of a new touch
        touchWasDown = true;
        touchDownMs  = now;
        touchDownX   = p.x;
        DBG_INFO("Touch down: raw x=%d y=%d z=%d", p.x, p.y, p.z);

      } else if ((now - touchDownMs) >= TOUCH_LONG_PRESS_MS) {
        // Held long enough → brightness cycle; consume so it fires once
        touchWasDown = false;
        const uint8_t levels[4] = {60, 120, 180, 255};
        brightnessStep = (brightnessStep + 1) % BRIGHTNESS_STEPS;
        setBrightness(levels[brightnessStep]);
        DBG_INFO("Long press: brightness step=%d pwm=%d",
                 brightnessStep, levels[brightnessStep]);
        delay(TOUCH_DEBOUNCE_MS);
      }
    }
  }
  // NOTE: do NOT reset touchWasDown here on !touching —
  //       checkTap() detects the release and owns that flag.
  return true;
}

// Call once per mode loop iteration — detects short tap after finger lifts.
// Returns 0=none, -1=prev mode, +1=next mode
static int8_t checkTap() {
  if (!touchWasDown) return 0;  // no touch in progress
  if (ts.touched())  return 0;  // still held — not a tap yet

  // Finger has lifted — classify as tap using saved touch-down position
  touchWasDown = false;
  DBG_INFO("Touch tap: raw x=%d (mid=%d) → %s",
           touchDownX, TOUCH_X_MID,
           touchDownX < TOUCH_X_MID ? "PREV mode" : "NEXT mode");

  return (touchDownX < TOUCH_X_MID) ? -1 : 1;
}

// ── Font helpers (index mapping) ──────────────────────────────────────────────
static byte charIndex5x7(char c) {
  if (c >= 'A' && c <= 'Z') return (byte)(c & 0x1F);
  if (c >= 'a' && c <= 'z') return (byte)((c - 'a') + 41);
  if (c >= '0' && c <= '9') return (byte)((c - '0') + 31);
  if (c == ' ')  return 0;
  if (c == '.')  return 27;
  if (c == '\'') return 28;
  if (c == ':')  return 29;
  if (c == '>')  return 30;
  return 0;
}

static byte charIndex3x5(char c) {
  if (c >= 'A' && c <= 'Z') return (byte)(c & 0x1F);
  if (c >= 'a' && c <= 'z') return (byte)(c & 0x1F); // lowercase maps same as uppercase
  if (c >= '0' && c <= '9') return (byte)((c - '0') + 31);
  if (c == ' ')  return 0;
  if (c == '.')  return 27;
  if (c == '\'') return 28;
  if (c == '!')  return 29;
  if (c == '?')  return 30;
  return 0;
}

// ── putChar — 5×7 ─────────────────────────────────────────────────────────────
void putChar(byte x, byte y, char c) {
  byte idx = charIndex5x7(c);
  for (byte col = 0; col < 5; col++) {
    byte dots = pgm_read_byte_near(&myfont[idx][col]);
    for (byte row = 0; row < 7; row++) {
      if (x + col < LED_WIDTH && y + row < LED_HEIGHT) {
        plot(x + col, y + row, (dots & (1 << row)) != 0);
      }
    }
  }
}

// ── putTinyChar — 3×5 ────────────────────────────────────────────────────────
void putTinyChar(byte x, byte y, char c) {
  byte idx = charIndex3x5(c);
  for (byte col = 0; col < 3; col++) {
    byte dots = pgm_read_byte_near(&mytinyfont[idx][col]);
    for (byte row = 0; row < 5; row++) {
      if (x + col < LED_WIDTH && y + row < LED_HEIGHT) {
        plot(x + col, y + row, (dots & (1 << row)) != 0);
      }
    }
  }
}

// ── putBigChar — 10×14 ────────────────────────────────────────────────────────
// Only renders digits '0'-'9'.
// Each glyph: bytes 0-9 = top 8 pixel-rows (MSB=topmost), bytes 10-19 = bottom 6 rows.
void putBigChar(byte x, byte y, char c) {
  if (c < '0' || c > '9') return;
  byte idx = (byte)(c - '0');

  for (byte col = 0; col < 10; col++) {
    // Top half: 8 rows
    byte dots = pgm_read_byte_near(&mybigfont[idx][col]);
    for (byte row = 0; row < 8; row++) {
      if (x + col < LED_WIDTH && y + row < LED_HEIGHT) {
        plot(x + col, y + row, (dots & (128 >> row)) != 0);
      }
    }
    // Bottom half: 6 rows (stored in bytes 10-19)
    dots = pgm_read_byte_near(&mybigfont[idx][col + 10]);
    for (byte row = 0; row < 6; row++) {
      if (x + col < LED_WIDTH && y + 8 + row < LED_HEIGHT) {
        plot(x + col, y + 8 + row, (dots & (128 >> row)) != 0);
      }
    }
  }
}

// ── drawDateRow ───────────────────────────────────────────────────────────────
// Renders date as e.g. "WED 18 MAR" using tinyfont at row 9 (rows 9-13).
// Only writes to LED rows 9-13 — safe to call any time during Slide mode without
// disturbing the time digits on rows 0-6.
static void drawDateRow() {
  static const char *dayAbbr[7]  = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  static const char *monAbbr[12] = {"JAN","FEB","MAR","APR","MAY","JUN",
                                     "JUL","AUG","SEP","OCT","NOV","DEC"};

  // Guard against uninitialised rtc (e.g. before first get_time)
  if (rtc[3] > 6 || rtc[5] < 1 || rtc[5] > 12 || rtc[4] < 1 || rtc[4] > 31) return;

  const char *d = dayAbbr[rtc[3]];
  const char *m = monAbbr[rtc[5] - 1];

  // Build "WED 18 MAR" (10 chars, space-padded day for single digits)
  char str[11];
  str[0] = d[0]; str[1] = d[1]; str[2] = d[2];
  str[3] = ' ';
  str[4] = (rtc[4] >= 10) ? ('0' + rtc[4] / 10) : ' ';
  str[5] = '0' + rtc[4] % 10;
  str[6] = ' ';
  str[7] = m[0]; str[8] = m[1]; str[9] = m[2];
  str[10] = '\0';

  // Centre in 48px: 10 chars × 4px spacing → span = 9×4+3 = 39px, x_start = (48-39)/2 = 4
  for (byte i = 0; i < 10; i++) {
    putTinyChar(4 + i * 4, 9, str[i]);
  }
}

// ── slideanim ─────────────────────────────────────────────────────────────────
// Renders one frame (sequence 0-8) of the slide transition.
// Old char slides DOWN (start at row 0, descends), new char slides UP (enters from top).
//
//  seq  rows of old char visible  start_y  new char rows visible  start_y
//   0   0-6 (all 7)               0        none                   -
//   1   0-5 (6)                   1        none                   -
//   2   0-4 (5)                   2        row 6 only             0
//   3   0-3 (4)                   3        rows 5-6               0
//   4   0-2 (3)                   4        rows 4-6               0
//   5   0-1 (2)                   5        rows 3-6               0
//   6   row 0 only                6        rows 2-6               0
//   7   none                      -        rows 1-6               0
//   8   none                      -        rows 0-6 (all 7)       0
void slideanim(byte x, byte y, byte sequence, char old_c, char new_c) {
  byte old_idx = charIndex5x7(old_c);
  byte new_idx = charIndex5x7(new_c);

  // Draw old char (sliding down)
  if (sequence < 7) {
    byte max_row    = 6 - sequence;  // highest row index to draw
    byte draw_y     = y + sequence;  // y offset for row 0 of the old char

    for (byte col = 0; col < 5; col++) {
      byte dots = pgm_read_byte_near(&myfont[old_idx][col]);
      for (byte row = 0; row <= max_row; row++) {
        if (x + col < LED_WIDTH && draw_y + row < LED_HEIGHT) {
          plot(x + col, draw_y + row, (dots & (1 << row)) != 0);
        }
      }
    }
  }

  // Blank the gap row just above where the old char now sits
  if (sequence >= 1 && sequence <= 7) {
    byte blank_y = y + sequence - 1;
    for (byte col = 0; col < 5; col++) {
      if (x + col < LED_WIDTH && blank_y < LED_HEIGHT) {
        plot(x + col, blank_y, false);
      }
    }
  }

  // Draw new char (sliding in from top)
  if (sequence >= 2) {
    byte min_row = 6 - (sequence - 2);  // first row of new char to draw
    byte draw_y  = y;

    for (byte col = 0; col < 5; col++) {
      byte dots = pgm_read_byte_near(&myfont[new_idx][col]);
      byte sy = 0;
      for (byte row = min_row; row <= 6; row++) {
        if (x + col < LED_WIDTH && y + sy < LED_HEIGHT) {
          plot(x + col, y + sy, (dots & (1 << row)) != 0);
        }
        sy++;
      }
    }
    (void)draw_y; // used implicitly via y above
  }
}

// ── Date helpers ─────────────────────────────────────────────────────────────
void set_next_date() {
  get_time();
  next_display_date = (rtc[1] + DATE_DISPLAY_MINS) % 60;
}

bool check_show_date() {
  if (next_display_date == 255) { set_next_date(); return false; }
  if (rtc[1] == next_display_date) {
    fade_down();
    display_date();
    fade_down();
    set_next_date();
    return true;
  }
  return false;
}

// ── flashing_cursor ──────────────────────────────────────────────────────────
void flashing_cursor(byte xpos, byte ypos, byte w, byte h, byte repeats) {
  for (byte r = 0; r <= repeats; r++) {
    for (byte x = 0; x <= w; x++)
      for (byte y = 0; y <= h; y++)
        plot(x + xpos, y + ypos, true);
    pushMatrix();
    delay(repeats > 0 ? 400 : 70);

    for (byte x = 0; x <= w; x++)
      for (byte y = 0; y <= h; y++)
        plot(x + xpos, y + ypos, false);
    pushMatrix();
    if (repeats > 0) delay(400);
  }
}

void levelbar(byte xpos, byte ypos, byte xbar, byte ybar) {
  for (byte x = 0; x <= xbar; x++)
    for (byte y = 0; y <= ybar; y++)
      plot(x + xpos, y + ypos, true);
}

// ── display_date ─────────────────────────────────────────────────────────────
void display_date() {
  cls(); pushMatrix();

  const char *days_full[7] = {"Sunday","Monday","Tuesday","Wed","Thursday","Friday","Saturday"};
  const char *suffix[4]    = {"st","nd","rd","th"};
  const char *months[12]   = {"January","February","March","April","May","June",
                               "July","August","Sept","October","November","December"};

  byte dow   = rtc[3];
  byte date  = rtc[4];
  byte month = rtc[5] - 1;

  flashing_cursor(0, 0, 5, 7, 1);

  byte i = 0;
  while (days_full[dow][i]) {
    flashing_cursor(i * 6, 0, 5, 7, 0);
    putChar(i * 6, 0, days_full[dow][i]);
    pushMatrix();
    i++;
  }
  if (i * 6 < 48) flashing_cursor(i * 6, 0, 5, 7, 1);
  else delay(300);

  flashing_cursor(0, 8, 5, 7, 0);

  char buf[5];
  itoa(date, buf, 10);

  byte s = 3;
  if (date == 1 || date == 21 || date == 31) s = 0;
  else if (date == 2 || date == 22)          s = 1;
  else if (date == 3 || date == 23)          s = 2;

  putChar(0, 8, buf[0]);
  byte sx = 6;
  if (date > 9) { sx = 12; flashing_cursor(6, 8, 5, 7, 0); putChar(6, 8, buf[1]); pushMatrix(); }
  flashing_cursor(sx, 8, 5, 7, 0);
  putChar(sx, 8, suffix[s][0]); pushMatrix();
  flashing_cursor(sx + 6, 8, 5, 7, 0);
  putChar(sx + 6, 8, suffix[s][1]); pushMatrix();
  flashing_cursor(sx + 12, 8, 5, 7, 1);

  cls();
  putChar(0, 0, buf[0]);
  putChar(6, 0, date > 9 ? buf[1] : ' ');
  putChar(sx, 0, suffix[s][0]);
  putChar(sx + 6, 0, suffix[s][1]);
  flashing_cursor(sx + 12, 0, 5, 7, 0);

  i = 0;
  while (months[month][i]) {
    flashing_cursor(i * 6, 8, 5, 7, 0);
    putChar(i * 6, 8, months[month][i]);
    pushMatrix();
    i++;
  }
  if (i * 6 < 48) flashing_cursor(i * 6, 8, 5, 7, 2);
  else delay(1000);
  delay(3000);
}

// ── Housekeeping — called from every mode loop ────────────────────────────────
// events()          — ezTime NTP housekeeping; designed to be called frequently,
//                     no-op unless a sync packet is pending or re-sync timer fires.
// updateBrightness() — LDR rolling average → setBrightness(); gated by LDR_UPDATE_MS.
static void tickHousekeeping() {
  events();

  static uint32_t lastLDR = 0;
  uint32_t now = millis();
  if (now - lastLDR >= LDR_UPDATE_MS) {
    lastLDR = now;
    updateBrightness();
  }
}

// ── SLIDE MODE ────────────────────────────────────────────────────────────────
void slide() {
  byte digits_old[6];
  byte digits_new[6];
  // x positions for each digit: secs-ones, secs-tens, mins-ones, mins-tens, hrs-ones, hrs-tens
  const byte xpos[6] = {42, 36, 24, 18, 6, 0};

  get_time();
  DBG_INFO("Slide mode entry: %02d:%02d:%02d  date %02d/%02d/%02d  NTP status=%d",
           rtc[2], rtc[1], rtc[0], rtc[4], rtc[5], rtc[6], (int)timeStatus());

  {
    byte hours = rtc[2];
    if (ampm) { if (hours > 12) hours -= 12; if (hours < 1) hours += 12; }
    digits_old[0] = rtc[0] % 10;
    digits_old[1] = rtc[0] / 10;
    digits_old[2] = rtc[1] % 10;
    digits_old[3] = rtc[1] / 10;
    digits_old[4] = hours  % 10;
    digits_old[5] = hours  / 10;
  }

  cls();
  // Draw current time immediately so display is populated from the first frame
  for (byte i = 0; i < 6; i++) {
    char ch[2];
    itoa(digits_old[i], ch, 10);
    if (ampm && i == 5 && digits_old[5] == 0) ch[0] = ' ';
    putChar(xpos[i], 0, ch[0]);
  }
  putChar(12, 0, ':');
  putChar(30, 0, ':');
  drawDateRow();
  pushMatrix();

  byte old_secs = rtc[0];
  byte old_mins = rtc[1];

  while (run_mode()) {
    tickHousekeeping();

    int8_t tap = checkTap();
    if (tap != 0) {
      clock_mode = (clock_mode + NUM_MODES + tap) % NUM_MODES;
      fade_down();
      return;
    }

    get_time();
    // Date is permanently visible on row 2 — no periodic full-screen interruption needed

    if (rtc[0] == old_secs) { delay(20); continue; }
    old_secs = rtc[0];
#if DEBUG_SLIDE_TIME
    if (rtc[1] != old_mins) {
      static const char *_dayA[7]  = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
      static const char *_monA[12] = {"JAN","FEB","MAR","APR","MAY","JUN",
                                       "JUL","AUG","SEP","OCT","NOV","DEC"};
      old_mins = rtc[1];
      DBG_INFO("[Slide] %02d:%02d:%02d %s %02d %s",
               rtc[2], rtc[1], rtc[0],
               _dayA[rtc[3]], rtc[4], _monA[rtc[5] - 1]);
    }
#endif

    byte hours = rtc[2];
    if (ampm) { if (hours > 12) hours -= 12; if (hours < 1) hours += 12; }

    digits_new[0] = rtc[0] % 10;
    digits_new[1] = rtc[0] / 10;
    digits_new[2] = rtc[1] % 10;
    digits_new[3] = rtc[1] / 10;
    digits_new[4] = hours  % 10;
    digits_new[5] = hours  / 10;

    for (byte i = 0; i < 6; i++) {
      if (digits_old[i] == digits_new[i]) continue;

      char old_ch[2], new_ch[2];
      itoa(digits_old[i], old_ch, 10);
      itoa(digits_new[i], new_ch, 10);
      // 12-hour mode: blank leading hours-tens zero
      if (ampm && i == 5) {
        if (digits_new[5] == 0) new_ch[0] = ' ';
        if (digits_old[5] == 0) old_ch[0] = ' ';
      }

      for (byte seq = 0; seq <= 8; seq++) {
        slideanim(xpos[i], 0, seq, old_ch[0], new_ch[0]);
        // Redraw colons each frame so they aren't overwritten by adjacent digit
        putChar(12, 0, ':');
        putChar(30, 0, ':');
        pushMatrix();
        delay(SLIDE_DELAY);
      }
    }

    // Refresh date row every second — handles midnight rollover and restores after any cls()
    drawDateRow();
    pushMatrix();

    for (byte i = 0; i < 6; i++) digits_old[i] = digits_new[i];
  }
  fade_down();
}

// ── PONG MODE ─────────────────────────────────────────────────────────────────
#define BAT1_X  2
#define BAT2_X  45

void pong_setup() {
  cls(); pushMatrix();

  byte i = 0;
  const char intro1[] = "Ready";
  while (intro1[i]) {
    delay(25);
    putTinyChar((i * 4) + 12, 4, intro1[i]);
    pushMatrix();
    i++;
  }
  for (byte f = 0; f < 2; f++) {
    putTinyChar(34, 4, '?'); pushMatrix(); delay(200);
    putTinyChar(34, 4, ' '); pushMatrix(); delay(400);
  }

  cls();
  i = 0;
  const char intro2[] = "Play Pong!";
  while (intro2[i]) {
    putTinyChar((i * 4) + 6, 4, intro2[i]);
    pushMatrix();
    i++;
  }
  delay(800);
  cls();

  get_time();
  byte offset = random(0, 2);
  for (byte j = 0; j < 16; j++) {
    if (j % 2 == 0) { plot(24, j + offset, true); pushMatrix(); delay(30); }
  }
  delay(120);
}

byte pong_get_ball_endpoint(float bx, float by, float vx, float vy) {
  while (bx > BAT1_X && bx < BAT2_X) {
    bx += vx; by += vy;
    if (by <= 0)  { vy = -vy; by = 0; }
    if (by >= 15) { vy = -vy; by = 15; }
  }
  return (byte)by;
}

void pong() {
  float bx, by, vx, vy;
  byte  ex = 10, ey = 10;
  int   bat1_y = 5, bat2_y = 5;
  int   bat1_t = 5, bat2_t = 5;
  byte  bat1_upd = 1, bat2_upd = 1;
  byte  bat1miss = 0, bat2miss = 0;
  byte  restart = 1;
  // Saved score chars — redrawn every frame so the ball never wipes the score
  char  sc_h0 = '0', sc_h1 = '0', sc_m0 = '0', sc_m1 = '0';

  pong_setup();

  while (run_mode()) {
    tickHousekeeping();

    int8_t tap = checkTap();
    if (tap != 0) {
      clock_mode = (clock_mode + NUM_MODES + tap) % NUM_MODES;
      fade_down(); return;
    }

    if (restart) {
      if (check_show_date()) pong_setup();
      plot(ex, ey, false);

      get_time();
      byte mins  = rtc[1];
      byte hours = rtc[2];
      if (ampm) { if (hours > 12) hours -= 12; if (hours < 1) hours += 12; }

      char buf[3];
      itoa(hours, buf, 10);
      if (hours < 10) { buf[1] = buf[0]; buf[0] = '0'; }
      sc_h0 = buf[0]; sc_h1 = buf[1];
      itoa(mins, buf, 10);
      if (mins < 10) { buf[1] = buf[0]; buf[0] = '0'; }
      sc_m0 = buf[0]; sc_m1 = buf[1];
      putTinyChar(14, 0, sc_h0); putTinyChar(18, 0, sc_h1);
      putTinyChar(28, 0, sc_m0); putTinyChar(32, 0, sc_m1);
      pushMatrix();

#if DEBUG_PONG_TIME
      DBG_INFO("[Pong] %02d:%02d:%02d", rtc[2], rtc[1], rtc[0]);
#endif
      bx = 23; by = random(4, 12);
      vx = (random(0, 2) > 0) ?  1.0f : -1.0f;
      vy = (random(0, 2) > 0) ?  0.5f : -0.5f;
      bat1miss = bat2miss = 0;
      restart = 0;
      delay(400);
    }

    get_time();

    if (rtc[0] == 59 && rtc[1] < 59) bat1miss = 1;
    if (rtc[0] == 59 && rtc[1] == 59) bat2miss = 1;

    // Basic AI — track ball when close
    if ((int)bx == random(30, 41)) bat1_t = (int)by;
    if ((int)bx == random(8, 19))  bat2_t = (int)by;

    // Left bat predict
    if ((int)bx == 23 && vx < 0) {
      byte ep = pong_get_ball_endpoint(bx, by, vx, vy);
      if (bat1miss) {
        bat1miss = 0;
        bat1_t = (ep > 8) ? random(0, 3) : 8 + random(0, 3);
      } else {
        byte s = (ep <= 5) ? 0 : ep - 5;
        byte e = (ep <= 10) ? ep : 10;
        bat1_t = random(s, e + 1);
      }
    }
    // Right bat predict
    if ((int)bx == 25 && vx > 0) {
      byte ep = pong_get_ball_endpoint(bx, by, vx, vy);
      if (bat2miss) {
        bat2miss = 0;
        bat2_t = (ep > 8) ? random(0, 3) : 8 + random(0, 3);
      } else {
        byte s = (ep <= 5) ? 0 : ep - 5;
        byte e = (ep <= 10) ? ep : 10;
        bat2_t = random(s, e + 1);
      }
    }

    // Move bats
    if (bat1_y > bat1_t && bat1_y > 0)  { bat1_y--; bat1_upd = 1; }
    if (bat1_y < bat1_t && bat1_y < 10) { bat1_y++; bat1_upd = 1; }
    if (bat2_y > bat2_t && bat2_y > 0)  { bat2_y--; bat2_upd = 1; }
    if (bat2_y < bat2_t && bat2_y < 10) { bat2_y++; bat2_upd = 1; }

    // Draw bat 1
    if (bat1_upd) {
      for (byte i = 0; i < 16; i++) {
        bool on = (i >= bat1_y && i < bat1_y + 6);
        plot(BAT1_X - 1, i, on);
        plot(BAT1_X - 2, i, on);
      }
      bat1_upd = 0;
    }
    // Draw bat 2
    if (bat2_upd) {
      for (byte i = 0; i < 16; i++) {
        bool on = (i >= bat2_y && i < bat2_y + 6);
        plot(BAT2_X + 1, i, on);
        plot(BAT2_X + 2, i, on);
      }
      bat2_upd = 0;
    }

    bx += vx; by += vy;

    if (by <= 0)  { vy = -vy; by = 0; }
    if (by >= 15) { vy = -vy; by = 15; }

    // Bat 1 collision
    if ((int)bx == BAT1_X && bat1_y <= (int)by && (int)by <= bat1_y + 5) {
      vx = -vx;
      if (random(0, 3) != 0) {
        byte fl = (bat1_y <= 1) ? 0 : (bat1_y >= 8) ? 1 : (byte)random(0, 2);
        if (fl == 0) { bat1_t += random(1, 3); if (vy < 2.0f)   vy += 0.2f; }
        else         { bat1_t -= random(1, 3); if (vy > 0.2f)   vy -= 0.2f; }
      }
    }
    // Bat 2 collision
    if ((int)bx == BAT2_X && bat2_y <= (int)by && (int)by <= bat2_y + 5) {
      vx = -vx;
      if (random(0, 3) != 0) {
        byte fl = (bat2_y <= 1) ? 0 : (bat2_y >= 8) ? 1 : (byte)random(0, 2);
        if (fl == 0) { bat2_t += random(1, 3); if (vy < 2.0f) vy += 0.2f; }
        else         { bat2_t -= random(1, 3); if (vy > 0.2f) vy -= 0.2f; }
      }
    }

    // Draw ball
    byte px = (byte)(bx + 0.5f);
    byte py = (byte)(by + 0.5f);
    plot(ex, ey, false);
    plot(px, py, true);
    ex = px; ey = py;
    // Redraw score every frame so ball can never erase it
    putTinyChar(14, 0, sc_h0); putTinyChar(18, 0, sc_h1);
    putTinyChar(28, 0, sc_m0); putTinyChar(32, 0, sc_m1);
    pushMatrix();

    if ((int)bx == 0 || (int)bx == 47) restart = 1;

    delay(PONG_BALL_DELAY);
  }
  fade_down();
}

// ── DIGITS MODE ───────────────────────────────────────────────────────────────
void digits() {
  get_time();
  DBG_INFO("Digits mode entry: %02d:%02d:%02d  NTP status=%d",
           rtc[2], rtc[1], rtc[0], (int)timeStatus());

  byte mins  = 100;
  byte secs  = 100;
  set_next_date();
  cls(); pushMatrix();

  while (run_mode()) {
    tickHousekeeping();

    int8_t tap = checkTap();
    if (tap != 0) {
      clock_mode = (clock_mode + NUM_MODES + tap) % NUM_MODES;
      fade_down(); return;
    }

    get_time();
    check_show_date();

    byte hours = rtc[2];
    if (ampm) { if (hours > 12) hours -= 12; if (hours < 1) hours += 12; }

    // Flash colon on seconds change
    if (secs != rtc[0]) {
      secs = rtc[0];
      bool dot_on = (secs % 2 == 0);
      plot(23, 4, dot_on); plot(24, 4, dot_on);
      plot(23, 5, dot_on); plot(24, 5, dot_on);
      plot(23, 10, dot_on); plot(24, 10, dot_on);
      plot(23, 11, dot_on); plot(24, 11, dot_on);
      pushMatrix();
    }

    if (mins == rtc[1]) { delay(50); continue; }
    mins = rtc[1];
#if DEBUG_DIGITS_TIME
    DBG_INFO("[Digits] %02d:%02d", rtc[2], rtc[1]);
#endif

    char buf[3];
    itoa(hours, buf, 10);
    if (hours < 10) { buf[1] = buf[0]; buf[0] = (ampm && hours < 10) ? ' ' : '0'; }

    byte offset = (ampm && hours < 10) ? 5 : 0;
    if (offset == 0) putBigChar(0, 1, buf[0]);
    putBigChar(12 - offset, 1, buf[1]);

    itoa(mins, buf, 10);
    if (mins < 10) { buf[1] = buf[0]; buf[0] = '0'; }
    putBigChar(26 - offset, 1, buf[0]);
    putBigChar(38 - offset, 1, buf[1]);
    pushMatrix();
  }
  fade_down();
}

// ── WORD CLOCK ────────────────────────────────────────────────────────────────
void word_clock() {
  const char *numbers[19] = {
    "one","two","three","four","five","six","seven","eight","nine","ten",
    "eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen"
  };
  const char *tens[5] = {"ten","twenty","thirty","forty","fifty"};

  byte old_mins = 100;
  set_next_date();

  while (run_mode()) {
    tickHousekeeping();

    int8_t tap = checkTap();
    if (tap != 0) {
      clock_mode = (clock_mode + NUM_MODES + tap) % NUM_MODES;
      fade_down(); return;
    }

    get_time();
    check_show_date();

    byte mins  = rtc[1];
    byte hours = rtc[2];
    if (hours > 12) hours -= 12;
    if (hours == 0) hours = 12;

    if (mins == old_mins) { delay(50); continue; }
    old_mins = mins;
#if DEBUG_WORD_TIME
    DBG_INFO("[Word] %02d:%02d", rtc[2], rtc[1]);
#endif

    byte md  = mins % 10;
    byte mdt = (mins / 10) % 10;

    char top[14], bot[14];

    if (mins < 10 && mins > 0) {
      strcpy(top, numbers[md - 1]); strcat(top, " PAST");
      strcpy(bot, numbers[hours - 1]);
    } else if (mins == 10) {
      strcpy(top, numbers[9]); strcat(top, " PAST");
      strcpy(bot, numbers[hours - 1]);
    } else if (mdt != 0 && md != 0) {
      strcpy(top, numbers[hours - 1]);
      if (mins >= 11 && mins <= 19) strcpy(bot, numbers[mins - 1]);
      else { strcpy(bot, tens[mdt - 1]); strcat(bot, " "); strcat(bot, numbers[md - 1]); }
    } else if (mdt != 0 && md == 0) {
      strcpy(top, numbers[hours - 1]);
      strcpy(bot, tens[mdt - 1]);
    } else {
      strcpy(top, numbers[hours - 1]);
      strcpy(bot, "O'CLOCK");
    }

    // Centre each string.
    // Total pixel width of len chars = (len-1)*4 + 3; subtract from 48 and halve.
    byte len = 0; while (top[len]) len++;
    byte ox = (45 - (len - 1) * 4) / 2;
    byte i = 0;
    fade_down();
    while (top[i]) { putTinyChar(ox + i * 4, 2, top[i]); i++; }

    len = 0; while (bot[len]) len++;
    ox = (45 - (len - 1) * 4) / 2;
    i = 0;
    while (bot[i]) { putTinyChar(ox + i * 4, 9, bot[i]); i++; }
    pushMatrix();
  }
  fade_down();
}
