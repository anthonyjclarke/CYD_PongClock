#include "config_nvs.h"
#include "config.h"
#include "debug.h"
#include <Preferences.h>
#include <string.h>

RuntimeConfig rtCfg;

static const char* NVS_NS = "pclock";  // NVS namespace

void loadConfig() {
  Preferences prefs;
  prefs.begin(NVS_NS, true);  // read-only

  rtCfg.clockMode    = prefs.getUChar("mode",   0);
  rtCfg.brightness   = prefs.getUChar("bright", BRIGHTNESS_DEFAULT);
  rtCfg.ampm         = prefs.getBool ("ampm",   AMPM_MODE);
  rtCfg.ledOnR       = prefs.getUChar("onR",    COLOUR_LED_ON_R);
  rtCfg.ledOnG       = prefs.getUChar("onG",    COLOUR_LED_ON_G);
  rtCfg.ledOnB       = prefs.getUChar("onB",    COLOUR_LED_ON_B);
  rtCfg.ledOffR      = prefs.getUChar("offR",   COLOUR_LED_OFF_R);
  rtCfg.ledOffG      = prefs.getUChar("offG",   COLOUR_LED_OFF_G);
  rtCfg.ledOffB      = prefs.getUChar("offB",   COLOUR_LED_OFF_B);

  prefs.getString("tz",  rtCfg.timezone,  sizeof(rtCfg.timezone));
  prefs.getString("ntp", rtCfg.ntpServer, sizeof(rtCfg.ntpServer));

  // First-boot defaults for strings
  if (rtCfg.timezone[0]  == '\0') strlcpy(rtCfg.timezone,  NTP_TIMEZONE,   sizeof(rtCfg.timezone));
  if (rtCfg.ntpServer[0] == '\0') strlcpy(rtCfg.ntpServer, "pool.ntp.org", sizeof(rtCfg.ntpServer));

  rtCfg.dateInterval = prefs.getUChar("dateInt", DATE_DISPLAY_MINS);
  rtCfg.ldrEnabled   = prefs.getBool ("ldr",     LDR_ENABLED);

  prefs.end();

  // Clamp to valid ranges
  if (rtCfg.clockMode >= NUM_MODES) rtCfg.clockMode = 0;

  DBG_INFO("Config loaded: mode=%d bright=%d ampm=%d tz=%s",
           rtCfg.clockMode, rtCfg.brightness, rtCfg.ampm, rtCfg.timezone);
}

void saveConfig() {
  Preferences prefs;
  prefs.begin(NVS_NS, false);  // read-write

  prefs.putUChar("mode",    rtCfg.clockMode);
  prefs.putUChar("bright",  rtCfg.brightness);
  prefs.putBool ("ampm",    rtCfg.ampm);
  prefs.putUChar("onR",     rtCfg.ledOnR);
  prefs.putUChar("onG",     rtCfg.ledOnG);
  prefs.putUChar("onB",     rtCfg.ledOnB);
  prefs.putUChar("offR",    rtCfg.ledOffR);
  prefs.putUChar("offG",    rtCfg.ledOffG);
  prefs.putUChar("offB",    rtCfg.ledOffB);
  prefs.putString("tz",     rtCfg.timezone);
  prefs.putString("ntp",    rtCfg.ntpServer);
  prefs.putUChar("dateInt", rtCfg.dateInterval);
  prefs.putBool ("ldr",     rtCfg.ldrEnabled);

  prefs.end();
  DBG_INFO("Config saved");
}

void resetConfigToDefaults() {
  rtCfg.clockMode    = 0;
  rtCfg.brightness   = BRIGHTNESS_DEFAULT;
  rtCfg.ampm         = AMPM_MODE;
  rtCfg.ledOnR       = COLOUR_LED_ON_R;
  rtCfg.ledOnG       = COLOUR_LED_ON_G;
  rtCfg.ledOnB       = COLOUR_LED_ON_B;
  rtCfg.ledOffR      = COLOUR_LED_OFF_R;
  rtCfg.ledOffG      = COLOUR_LED_OFF_G;
  rtCfg.ledOffB      = COLOUR_LED_OFF_B;
  strlcpy(rtCfg.timezone,  NTP_TIMEZONE,   sizeof(rtCfg.timezone));
  strlcpy(rtCfg.ntpServer, "pool.ntp.org", sizeof(rtCfg.ntpServer));
  rtCfg.dateInterval = DATE_DISPLAY_MINS;
  rtCfg.ldrEnabled   = LDR_ENABLED;
  saveConfig();
  DBG_INFO("Config reset to defaults");
}
