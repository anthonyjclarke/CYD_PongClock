#pragma once
// config_nvs.h — Runtime configuration backed by NVS (Preferences)
// Shadows the compile-time constants in config.h with user-editable values.
// Call loadConfig() once in setup(); saveConfig() whenever values change.

#include <stdint.h>

struct RuntimeConfig {
  uint8_t  clockMode;       // 0=Slide 1=Pong 2=Digits 3=WordClock 4=Invaders
  uint8_t  brightness;      // backlight PWM 0-255
  bool     ampm;            // false=24h  true=12h
  uint8_t  ledOnR;          // LED-on colour
  uint8_t  ledOnG;
  uint8_t  ledOnB;
  uint8_t  ledOffR;         // LED-off colour
  uint8_t  ledOffG;
  uint8_t  ledOffB;
  char     timezone[40];    // Olson timezone string e.g. "Australia/Sydney"
  char     ntpServer[64];   // NTP server hostname
  uint8_t  dateInterval;    // minutes between periodic date displays
  bool     ldrEnabled;      // auto-brightness from LDR
};

extern RuntimeConfig rtCfg;

// Load settings from NVS; populate rtCfg. Call once after Serial.begin().
void loadConfig();

// Persist rtCfg to NVS.
void saveConfig();

// Reset rtCfg to compile-time defaults and persist.
void resetConfigToDefaults();
