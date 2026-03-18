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
    DBG_WARN("Time: timezone lookup failed, falling back to UTC");
    myTZ.setLocation(F("UTC"));
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
