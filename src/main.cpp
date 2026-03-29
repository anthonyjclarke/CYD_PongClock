#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFiManager.h>
#include <ezTime.h>
#include "config.h"
#include "config_nvs.h"
#include "debug.h"
#include "display.h"
#include "clock.h"
#include "web.h"

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
// Three lines of myfont 5×7 evenly spaced inside the bordered matrix.
// Layout (32 rows):  margin 2 | PONG 7 | gap 3 | CLOCK 7 | gap 3 | version 7 | margin 3
//   "PONG"  y=2   x=12  (4 chars × 6 – 1 = 23px, centred in 48)
//   "CLOCK" y=12  x=9   (5 chars × 6 – 1 = 29px)
//   "vX.X"  y=22  x=12  (same width as PONG)
// Typewriter reveal → 900 ms hold → CRT-collapse exit.
// Draw the outer ring of the LED matrix as lit dots — splash border only.
// Cleared naturally by the CRT-collapse animation and final cls().
static void drawLedBorder() {
  for (byte x = 0; x < LED_WIDTH; x++) {
    plot(x, 0,              true);   // top row
    plot(x, LED_HEIGHT - 1, true);   // bottom row
  }
  for (byte y = 1; y < LED_HEIGHT - 1; y++) {
    plot(0,             y, true);    // left column
    plot(LED_WIDTH - 1, y, true);    // right column
  }
}

static void showSplash() {
  cls();
  drawLedBorder();
  pushMatrix();

  // ── Typewriter: PONG ────────────────────────────────────────────────────────
  const byte xP[4] = {12, 18, 24, 30};
  const char sP[4] = {'P', 'O', 'N', 'G'};
  for (byte i = 0; i < 4; i++) { putChar(xP[i], 2, sP[i]); pushMatrix(); delay(60); }

  // ── Typewriter: CLOCK ───────────────────────────────────────────────────────
  const byte xC[5] = {9, 15, 21, 27, 33};
  const char sC[5] = {'C', 'L', 'O', 'C', 'K'};
  for (byte i = 0; i < 5; i++) { putChar(xC[i], 12, sC[i]); pushMatrix(); delay(60); }

  // ── Typewriter: version ─────────────────────────────────────────────────────
  char verBuf[8];
  snprintf(verBuf, sizeof(verBuf), "v%s", FW_VERSION);
  byte vlen = (byte)strlen(verBuf);
  byte vx   = (byte)((LED_WIDTH - (vlen * 6 - 1)) / 2);
  delay(100);
  for (byte i = 0; i < vlen; i++) { putChar(vx + i * 6, 22, verBuf[i]); pushMatrix(); delay(60); }

  // ── Hold ────────────────────────────────────────────────────────────────────
  delay(900);

  // ── CRT-collapse: rows fold inward from top and bottom simultaneously ────────
  for (byte i = 0; i < LED_HEIGHT / 2; i++) {
    for (byte x = 0; x < LED_WIDTH; x++) {
      plot(x, i,                  false);
      plot(x, LED_HEIGHT - 1 - i, false);
    }
    pushMatrix();
    delay(15);
  }

  // Restore plain background — border only visible during splash
  tft.fillScreen(tft.color565(5, 2, 0));
  cls(); pushMatrix();
}

// ── Status line in bottom margin during init (never touches matrix sprite) ────
static void showStatus(const char* msg) {
  tft.setTextColor(tft.color565(180, 100, 0), tft.color565(20, 8, 0));
  tft.setTextSize(1);
  int16_t x = (tft.width() - (int16_t)(strlen(msg) * 6)) / 2;
  tft.drawString(msg, x > 0 ? x : 0, 220, 2);
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

  // Load NVS config first — values used during display/clock init below
  loadConfig();

  initDisplay();

  // Apply persisted brightness and LED colours (initDisplay sets BRIGHTNESS_DEFAULT)
  setBrightness(rtCfg.brightness);
  setLedColours(rtCfg.ledOnR, rtCfg.ledOnG, rtCfg.ledOnB,
                rtCfg.ledOffR, rtCfg.ledOffG, rtCfg.ledOffB);

  // Apply persisted clock state
  clock_mode = rtCfg.clockMode;
  ampm       = rtCfg.ampm;

  showSplash();
  initTouch();
  initWiFi();
  initTime();
  initWeb();

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
    case 4: invaders();   break;
    default: clock_mode = 0; break;
  }
}
