# PongClock_CYD

## Project

A faithful port of Nick Hall's Pong Clock (v7.5) to the ESP32 CYD (Cheap Yellow Display). The original ran on an Arduino driving two Sure Electronics 2416 LED panels (combined 48×16 LEDs). This version emulates that exact 48×16 virtual LED matrix on the ILI9341 320×240 TFT — each virtual LED is a 6×6 rounded rectangle in amber, giving a retro LED panel aesthetic.

Clock modes: **Slide** (digits slide in/out), **Pong** (game score = time), **Digits** (large 10×14 font), **Word Clock** (time in words). Mode switching via touchscreen. Time from NTP via WiFi.

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
| Virtual LEDs    | 48 wide × 16 tall                  |
| Pixel size      | 6px per LED                        |
| Matrix px       | 288 × 96                           |
| X offset        | 16px (centres 288px in 320px)      |
| Y offset        | 72px (centres 96px in 240px)       |
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

Rendering uses a full-matrix `TFT_eSprite` (288×96px). `plot()` writes into the sprite; the sprite is pushed to screen at the end of each logical update via `pushSprite()`. This eliminates per-pixel SPI calls and prevents flicker on slide animations.

## Touch Input

Single tap left third → previous mode. Single tap right third → next mode. Long press anywhere → brightness cycle (4 steps). No physical buttons — CYD has none.

## Known Issues / Quirks

- `TFT_RST=-1` is correct — no reset pin on CYD.
- Touch SPI must stay ≤ 2.5 MHz or reads are unreliable.
- XPT2046 needs `tirqTouched() && touched()` guard before reading.
- Slide animation: `slideanim()` renders pixel-row-by-row from PROGMEM font data directly — do not simplify to whole-char calls or the overlap during transition will corrupt.
- `mybigfont` is 10 wide × 14 tall, stored as 20 bytes per glyph (2 rows of 8-bit column data per column pair). See `ht1632_putbigchar()` for decode logic.

## Flashing Notes

- Port: `/dev/cu.usbserial-*` (CP2102)
- Upload speed: 230400 (921600 can fail on some cables)
- After flash: check Serial for `Display initialised 320x240` and `WiFi connected` then verify clock shows on screen within ~10 s of NTP sync.

## Rules

- Global rules: `~/.claude/CLAUDE.md`
- Type rules: `~/.claude/rules/cyd-esp32.md`
