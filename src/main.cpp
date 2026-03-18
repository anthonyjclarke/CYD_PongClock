#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFiManager.h>
#include <ezTime.h>
#include "config.h"
#include "debug.h"
#include "display.h"
#include "clock.h"

TFT_eSPI tft;

Timezone myTZ;

// ── Display init ──────────────────────────────────────────────────────────────
void initDisplay() {
  tft.init();
  tft.setRotation(SCREEN_ROTATION);

  // Backlight via LEDC
  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, 0); // start dark

  initColours(); // sets up sprite and colour constants
  clsNow();

  ledcWrite(0, BRIGHTNESS_DEFAULT);
  currentBrightness = BRIGHTNESS_DEFAULT;

  DBG_INFO("Display initialised %dx%d rotation=%d",
           tft.width(), tft.height(), SCREEN_ROTATION);
}

// ── Startup splash screen ─────────────────────────────────────────────────────
// Typewriter reveal → 900 ms hold → CRT-collapse exit. Total ≈ 2.3 s.
//
// Layout in 32-row LED matrix — 3-row margins top and bottom:
//   "PONG"  myfont 5×7    row  3  x=12
//   "CLOCK" myfont 5×7    row 13  x=9
//   "v0.3"  tinyfont 3×5  row 24  x=16
static void showSplash() {
  cls();

  // ── Typewriter: PONG ────────────────────────────────────────────────────────
  // "PONG" span = 3×6+5 = 23px → x = (48-23)/2 = 12
  const byte xP[4] = {12, 18, 24, 30};
  const char sP[4] = {'P','O','N','G'};
  for (byte i = 0; i < 4; i++) { putChar(xP[i], 3, sP[i]); pushMatrix(); delay(60); }

  // ── Typewriter: CLOCK ───────────────────────────────────────────────────────
  // "CLOCK" span = 4×6+5 = 29px → x = (48-29)/2 = 9
  const byte xC[5] = {9, 15, 21, 27, 33};
  const char sC[5] = {'C','L','O','C','K'};
  for (byte i = 0; i < 5; i++) { putChar(xC[i], 13, sC[i]); pushMatrix(); delay(60); }

  // ── Typewriter: version ─────────────────────────────────────────────────────
  // "v" FW_VERSION e.g. "v0.3" — span = 3×4+3 = 15px → x = (48-15)/2 = 16
  const char* ver = "v" FW_VERSION;
  byte vlen = (byte)strlen(ver);
  byte vx   = (byte)((LED_WIDTH - ((vlen - 1) * 4 + 3)) / 2);
  delay(100);
  for (byte i = 0; i < vlen; i++) { putTinyChar(vx + i * 4, 24, ver[i]); pushMatrix(); delay(60); }

  // ── Hold ────────────────────────────────────────────────────────────────────
  delay(900);

  // ── CRT-collapse: rows fold inward from top and bottom simultaneously ────────
  // 16 passes × (SPI ~20ms + 15ms delay) ≈ 560ms
  for (byte i = 0; i < LED_HEIGHT / 2; i++) {
    for (byte x = 0; x < LED_WIDTH; x++) {
      plot(x, i,                  false);
      plot(x, LED_HEIGHT - 1 - i, false);
    }
    pushMatrix();
    delay(15);
  }

  cls(); pushMatrix();  // clean slate for WiFi status
}

// ── Status line on screen (below matrix, for init messages) ───────────────────
static void showStatus(const char* msg) {
  tft.setTextColor(tft.color565(180, 100, 0), tft.color565(5, 2, 0));
  tft.setTextSize(1);
  // centre in the ~240px below/above matrix
  int16_t x = (tft.width() - (int16_t)(strlen(msg) * 6)) / 2;
  tft.drawString(msg, x > 0 ? x : 0, 20, 2);
}

// ── WiFi init ─────────────────────────────────────────────────────────────────
void initWiFi() {
  showStatus("Connecting WiFi...");
  WiFiManager wm;
  wm.setConfigPortalTimeout(WIFI_TIMEOUT_S);

  if (!wm.autoConnect(WIFI_AP_NAME)) {
    DBG_WARN("WiFi: connect timeout, continuing offline");
    showStatus("WiFi offline");
  } else {
    DBG_INFO("WiFi connected: %s", WiFi.localIP().toString().c_str());
    showStatus("WiFi OK");
  }
}

// ── Time init ─────────────────────────────────────────────────────────────────
void initTime() {
  showStatus("Syncing NTP...");
  // ezTime: non-blocking sync with timeout
  waitForSync(NTP_SYNC_TIMEOUT_S);

  if (timeStatus() == timeNotSet) {
    DBG_WARN("Time: NTP sync failed — clock will show boot time");
  }

  if (!myTZ.setLocation(F(NTP_TIMEZONE))) {
    // timezoneapi.io HTTP lookup failed (common immediately after WiFi connect).
    // Apply POSIX rule directly — correct DST, no network required.
    DBG_WARN("Time: Olson lookup failed — applying POSIX fallback: %s", NTP_POSIX_FALLBACK);
    myTZ.setPosix(NTP_POSIX_FALLBACK);
  }

  DBG_INFO("Time synced: %s  status=%d  tz=%s offset=%s",
           myTZ.dateTime("H:i:s d-M-Y").c_str(), (int)timeStatus(),
           myTZ.dateTime("T").c_str(),   // e.g. AEST or AEDT
           myTZ.dateTime("O").c_str());  // e.g. +1000 or +1100
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  DBG_INFO("=== PongClock CYD starting ===");

  initDisplay();
  showSplash();
  initTouch();
  initWiFi();
  initTime();

  DBG_INFO("Free heap: %d bytes", ESP.getFreeHeap());

  // Seed RNG for pong ball direction
  randomSeed(analogRead(34)); // GPIO34 = LDR on CYD, floating = good entropy
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
  // ezTime housekeeping (NTP re-sync)
  events();

  switch (clock_mode) {
    case 0: slide();      break;
    case 1: pong();       break;
    case 2: digits();     break;
    case 3: word_clock(); break;
    default: clock_mode = 0; break;
  }
}
