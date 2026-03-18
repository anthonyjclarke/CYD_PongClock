# PongClock_CYD

## Project

A faithful port of Nick Hall's Pong Clock (v7.5) to the ESP32 CYD (Cheap Yellow Display). The original ran on an Arduino driving two Sure Electronics 2416 LED panels (combined 48×16 LEDs). This version emulates a 48×32 virtual LED matrix (double-height, two stacked panels) on the ILI9341 320×240 TFT — each virtual LED is a 6×6 rounded rectangle in amber, giving a retro LED panel aesthetic.

Clock modes: **Slide** (digits slide in/out, date permanently below), **Pong** (game score = time, date permanently at bottom), **Digits** (large 10×14 font), **Word Clock** (time in words, date on third line). Mode switching via touchscreen. Time from NTP via WiFi. Animated splash screen on boot.

## Hardware

- Board: `ESP32-2432S028R (CYD)`
- Display: ILI9341 · 240×320 (used landscape = 320×240) · SPI
- Touch: XPT2046 · separate SPI CS (GPIO 33)
- No PSRAM on standard CYD

## Pin Assignments

All standard CYD — see `~/.claude/rules/cyd-esp32.md`. No non-standard pins used.

Touch CS: GPIO 33 (separate VSPI bus from display).

## Display Geometry

| Parameter       | Value                              |
|:----------------|:-----------------------------------|
| Virtual LEDs    | 48 wide × 32 tall                  |
| Pixel size      | 6px per LED                        |
| Matrix px       | 288 × 192                          |
| X offset        | 16px (centres 288px in 320px)      |
| Y offset        | 24px (centres 192px in 240px)      |
| Sprite depth    | 8-bit RGB332 (55KB; 16-bit is 110KB — too large without PSRAM) |
| LED on colour   | Amber `color565(255, 140, 0)`      |
| LED off colour  | Dark amber `color565(20, 8, 0)`    |

## Architecture

```
src/
  main.cpp      — setup(), loop(), initDisplay/WiFi/Time
  display.cpp   — plot(), cls(), fade_down/up, sprite management
  clock.cpp     — all clock modes + font rendering + touch
include/
  config.h      — all tuneable constants
  display.h     — plot/cls/fade declarations, extern tft + sprite
  clock.h       — clock mode declarations
  fonts.h       — myfont(5×7), mybigfont(10×14), mytinyfont(3×5) in PROGMEM
  debug.h       — leveled debug macros
  secrets.h     — WiFi credentials (gitignored)
```

Rendering uses a full-matrix `TFT_eSprite` (288×192px, 8-bit depth). `plot()` writes into the sprite; the sprite is pushed to screen at the end of each logical update via `pushSprite()`. This eliminates per-pixel SPI calls and prevents flicker on slide animations. TFT_eSPI auto-expands RGB332→RGB565 on push — visually transparent with only two amber tones in use.

## Touch Input

Screen split into left/right halves at `TOUCH_X_MID`. Single tap left half → previous mode. Single tap right half → next mode. Long press anywhere (≥600ms) → brightness cycle (4 steps). No physical buttons — CYD has none.

## Known Issues / Quirks

- `TFT_RST=-1` is correct — no reset pin on CYD.
- Touch SPI must stay ≤ 2.5 MHz or reads are unreliable.
- XPT2046 needs `tirqTouched() && touched()` guard before reading.
- Slide animation: `slideanim()` renders pixel-row-by-row from PROGMEM font data directly — do not simplify to whole-char calls or the overlap during transition will corrupt.
- `mybigfont` is 10 wide × 14 tall, stored as 20 bytes per glyph (2 rows of 8-bit column data per column pair). See `ht1632_putbigchar()` for decode logic.
- Sprite must use `setColorDepth(8)` before `createSprite()` — 16-bit at 288×192 requires 110KB contiguous heap which the ESP32 cannot satisfy without PSRAM. 8-bit = 55KB, fits comfortably.
- `myTZ.setLocation("Australia/Sydney")` makes an HTTP call to timezoneapi.io which reliably fails immediately after WiFi connect. Always set `NTP_POSIX_FALLBACK` and call `myTZ.setPosix()` as fallback — it is offline and DST-correct.

## Flashing Notes

- Port: `/dev/cu.usbserial-*` (CP2102)
- Upload speed: 230400 (921600 can fail on some cables)
- After flash: check Serial for `Display initialised 320x240` and `WiFi connected` then verify clock shows on screen within ~10 s of NTP sync.

## Rules

- Global rules: `~/.claude/CLAUDE.md`
- Type rules: `~/.claude/rules/cyd-esp32.md`
