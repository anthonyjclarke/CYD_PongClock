#pragma once
// clock.h — Pong Clock mode declarations

#include <Arduino.h>
#include <time.h>
#include <ezTime.h>

extern Timezone myTZ;  // defined in main.cpp — used by get_time()

// ── State ─────────────────────────────────────────────────────────────────────
extern byte  rtc[7];      // [0]=sec [1]=min [2]=hr [3]=dow [4]=day [5]=mon [6]=yr%100
extern byte  clock_mode;  // 0=Slide 1=Pong 2=Digits 3=WordClock
extern bool  ampm;        // false=24h true=12h

// ── Time ──────────────────────────────────────────────────────────────────────
void get_time();

// ── Touch init ────────────────────────────────────────────────────────────────
void initTouch();

// ── Mode control ──────────────────────────────────────────────────────────────
// Returns true while the mode should keep running.
// Returns false when touch input requests a mode change.
bool run_mode();

// ── Clock modes ───────────────────────────────────────────────────────────────
void slide();
void pong();
void digits();
void word_clock();

// ── Date display ─────────────────────────────────────────────────────────────
void display_date();
bool check_show_date();
void set_next_date();

// ── Font rendering ───────────────────────────────────────────────────────────
// 5×7 characters into the matrix
void putChar(byte x, byte y, char c);
// 3×5 tiny characters
void putTinyChar(byte x, byte y, char c);
// 10×14 large digit (0-9 only)
void putBigChar(byte x, byte y, char c);

// ── Slide animation ──────────────────────────────────────────────────────────
// sequence 0-8: renders one frame of old_c sliding out / new_c sliding in
void slideanim(byte x, byte y, byte sequence, char old_c, char new_c);

// ── Pong helpers ─────────────────────────────────────────────────────────────
byte pong_get_ball_endpoint(float bx, float by, float vx, float vy);
void pong_setup();

// ── Misc ─────────────────────────────────────────────────────────────────────
void flashing_cursor(byte xpos, byte ypos, byte w, byte h, byte repeats);
void levelbar(byte xpos, byte ypos, byte xbar, byte ybar);
