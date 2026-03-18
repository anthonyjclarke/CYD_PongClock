# Changelog

All notable changes documented here.
Format: `## [version] YYYY-MM-DD` with `### Added / Changed / Fixed` subsections.

---

## [To do]

### Font fixes
- Audit `mybigfont` 10×14 digits — check `1` and `7` proportions visually on device

### Next to do
- WebUI

### Phase 4 — Timer mode
- Countdown timer as 5th clock mode (`NUM_MODES` → 5)
- Touch: tap = start/stop, long-press = reset, left/right = adjust minutes
- No extra hardware

### Phase 5 — Sensor integration
- External I2C temperature sensor (BME280 recommended — temp + humidity + pressure)
- Requires Phase 3 display rows to show alongside time
- Display in Slide mode row 3 or dedicated mode

### Future
- Add Webui to adjust all setting, Timezone, Wifi, Debug etc
- 

## [0.3.0] 2026-03-18

### Added
- Animated boot splash: typewriter reveal of "PONG / CLOCK / v0.3" into the LED matrix
  sprite, 900 ms hold, then CRT-collapse exit (rows fold inward top+bottom simultaneously,
  16 passes × ~35 ms ≈ 560 ms); total sequence ≈ 2.3 s; version string driven by
  `FW_VERSION` in `config.h` — update each release
- Phase 3: 48×32 matrix (doubled height) — `LED_HEIGHT` 16→32, `LED_Y_OFFSET` 72→24,
  sprite 288×96→288×192; all four modes fully re-centred in the expanded canvas
- Pong: full 32-row play field — ball bounces wall-to-wall; `BAT_HEIGHT=8` constant
  (config.h); bat clamp, AI midpoint, endpoint prediction, and collision logic all
  derived from `LED_HEIGHT`/`BAT_HEIGHT` so they stay correct if height changes again
- Pong: date permanently displayed at bottom of field (rows 27-31, "WED 18 MAR") redrawn
  every frame using the same logic as the score — ball cannot permanently erase it
- Word Clock: third line added showing date ("WED 18 MAR") at Y=21; three lines centred
  as a block (6-row margins top and bottom); periodic full-screen date interruption removed
- `display_date()` centred in 32-row matrix (y1=8, y2=16); previously sat in top half only

### Changed
- Slide mode: time row Y=0→9, date row Y=9→18; combined block perfectly centred
  (9-row margins top and bottom) in the 32-row matrix
- Digits mode: big font Y=1→9, colon dots rows 4,5,10,11 → 12,13,18,19; centred
- Word Clock: top phrase Y=2→6, bottom phrase Y=9→13
- `drawDateRow()` now takes a `byte y` parameter (was hardcoded to row 9)
- `pong_setup()` intro text centred at Y=13; separator line now spans all 32 rows

---

## [0.2.0] 2026-03-18

### Fixed
- Auto date display interval simplified to a single `DATE_DISPLAY_MINS` constant in `config.h`
  (was two constants `NEXT_DATE_MIN`/`NEXT_DATE_MAX` driving a random range); interval is now
  fixed and predictable — default 10 minutes
- `mytinyfont` N glyph (index 14): redrawn as full top bar + two full-height verticals
  `{0x1F, 0x01, 0x1F}` — top row spans all 3 cols, col1 top-only connector; affects Word
  Clock number strings ("one", "nine", "nineteen", "twenty nine", etc.)
- Word Clock: right-edge clipping on 12-character strings (e.g. "thirty seven") — centering
  formula `(48-(len-1)*4)/2` ignored the 3 px width of the last character; corrected to
  `(45-(len-1)*4)/2` so all strings fit within the 48-LED boundary
- Slide mode blank on first power-up: `digits_old` is now initialised from the actual current
  time at entry and all six digits are drawn directly to the sprite before the animation loop
  starts; previously `digits_old={99,…}` meant digits only appeared after the first seconds tick
  and caused a spurious '9' ghost during the initial slide animation
- Font rendering upside-down: `putChar`, `putTinyChar`, and `slideanim` now use `1<<row`
  (bit0=top, LSB=top-row convention of original myfont/mytinyfont data); was using `64>>row` / `16>>row`
- NTP time not applied to display: `get_time()` now reads from `myTZ` (ezTime Timezone object)
  directly instead of POSIX `localtime()` which has no knowledge of the ezTime timezone
- `myTZ` exported from `main.cpp` (removed `static`) so `clock.cpp` can access it via `extern`
- `set_next_date()` wrap bug: when current minute + interval >= 60, fell back to an absolute minute
  value (0–15) rather than a relative one, causing up to ~55 min delay near the hour boundary;
  now uses `% 60` to correctly wrap to the next hour

### Added
- Auto-brightness via LDR (GPIO 34): `updateBrightness()` in `display.cpp` reads the CYD's
  light sensor, maintains an 8-sample rolling average, and maps the result to backlight PWM
  via `map(avg, LDR_DARK, LDR_BRIGHT, BRIGHTNESS_MIN, BRIGHTNESS_MAX)`; sampled every
  `LDR_UPDATE_MS` (5 s default); disable with `LDR_ENABLED=0` in `config.h`
- `tickHousekeeping()` called at the top of every mode loop: runs `events()` (ezTime NTP
  re-sync housekeeping) on every iteration and gates `updateBrightness()` behind
  `LDR_UPDATE_MS`; previously `events()` was only called between mode switches
- Slide mode: date permanently displayed on the second row of the LED matrix (rows 9-13)
  as "WED 18 MAR" in tinyfont, centred in the 48-LED-wide matrix; refreshes every second
  so midnight rollover is handled automatically; replaces the previous periodic full-screen
  date interruption which fired every 10-15 minutes
- Status messages on screen during WiFi and NTP init (`showStatus()` in `main.cpp`)
- Per-mode serial debug on each time change: `[Slide] HH:MM:SS`, `[Digits] HH:MM`, etc.
  Controlled by `DEBUG_SLIDE_TIME` / `DEBUG_DIGITS_TIME` / `DEBUG_PONG_TIME` / `DEBUG_WORD_TIME`
  defines in `config.h` — set to 0 to suppress
- Mode-entry `DBG_INFO` in `slide()` and `digits()` showing time + NTP status code
- Timezone abbreviation and UTC offset logged at boot (e.g. `tz=AEDT offset=+1100`) to diagnose DST issues

---

## [0.1.0] 2026-03-17

### Added
- Full port of Nick Hall Pong Clock v7.5 to ESP32 CYD (ILI9341 320×240)
- Sprite-based rendering — full 288×96 matrix sprite, pushed once per frame (no per-pixel SPI flicker)
- Retro amber LED aesthetic: 6×6 rounded pixel per virtual LED, dark amber background
- All four clock modes: Slide, Pong, Digits, Word Clock
- Correct `slideanim()` — pixel-row-level render of departing and arriving character
- Correct `mybigfont` 10×14 data and `mytinyfont` 3×5 data (restored from original)
- NTP time sync via ezTime (`Australia/Sydney` default, change in `config.h`)
- WiFiManager captive portal for credential entry on first boot
- XPT2046 touch input for mode switching (left tap = back, right tap = next, long press = brightness)
- `config.h` constants for all tuneable values
- CLAUDE.md and README.md with full project documentation

### Changed
- Replaced Arduino HT1632C display driver with TFT_eSPI + virtual LED matrix layer
- Replaced DS1307 RTC with NTP/ezTime
- Replaced physical buttons with capacitive touch zones
- Replaced raw `Serial.print` debug calls with leveled `DBG_*` macros


