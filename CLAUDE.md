# PongClock_CYD

## Project

A faithful port of Nick Hall's Pong Clock (v7.5) to the ESP32 CYD (Cheap Yellow Display). The original ran on an Arduino driving two Sure Electronics 2416 LED panels (combined 48×16 LEDs). This version emulates a 48×32 virtual LED matrix (double-height, two stacked panels) on the ILI9341 320×240 TFT — each virtual LED is a 6×6 rounded rectangle in amber, giving a retro LED panel aesthetic.

Clock modes: **Slide** (digits slide in/out, date permanently below), **Pong** (game score = time, date permanently at bottom), **Digits** (large 10×14 font), **Word Clock** (time in words, date on third line), **Invaders** (space invaders scroll left↔right, time shown above — ported from Richard Shipman's PongClock v2.40, https://github.com/RichardShipman/PongClock). Mode switching via touchscreen. Time from NTP via WiFi. Animated splash screen on boot.

HTTP web server (port 80, `src/web.cpp`): serves a two-tab SPA from LittleFS (`data/`) — **Clock tab** runs all five clock modes live in the browser as a pixel-exact JavaScript reimplementation on an HTML5 canvas inside a CSS CYD device mockup; **Config tab** exposes all runtime settings (mode, brightness, 12/24h, LED colours, timezone, NTP server, date interval, LDR). Full API: `GET /screenshot.bmp` streams the sprite buffer as a 24-bit BMP (288×192, RGB332→BGR888); `GET /api/info` returns JSON (firmware, mode, brightness, uptime, heap, IP); `GET/POST /api/config` reads and applies partial JSON config patches, persisting all changes to NVS; `POST /api/wifi-reset` erases WiFiManager credentials and restarts into AP mode. `handleNotFound()` serves any file present in LittleFS (MIME type auto-detected by extension), so any asset added to `data/` is automatically available with no handler registration. Server is skipped silently if WiFi is offline.

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
data/                        ← LittleFS web assets (pio run -t uploadfs)
  index.html                 — SPA: Clock tab (CYD mockup + canvas) + Config tab
  clock.js                   — JS port of all 5 modes + fonts; async/await timing
  style.css                  — CYD PCB mockup CSS; canvas 288×192 scaled 1.5× via CSS
  pong-logo.png              — Atari Pong® logo bitmap; served as /pong-logo.png
src/
  main.cpp      — setup(), loop(), initDisplay/WiFi/Time/Web; loads NVS config at boot
  display.cpp   — plot(), cls(), fade_down/up, setLedColours(), sprite management; ledColourChanged flag
  clock.cpp     — all clock modes + font rendering + touch
  web.cpp       — WebServer: generic LittleFS fallback (mimeFor()), /api/info, /api/config, /api/wifi-reset; config change summary log
  config_nvs.cpp — NVS persistence (Preferences namespace "pclock")
include/
  config.h      — compile-time defaults (WEB_SERVER_PORT, colours, timing, etc.)
  config_nvs.h  — RuntimeConfig struct + loadConfig()/saveConfig()/resetConfigToDefaults()
  display.h     — plot/cls/fade/setLedColours declarations, extern tft + sprite
  clock.h       — clock mode declarations
  web.h         — initWeb(), webLoop() declarations
  fonts.h       — myfont(5×7), mybigfont(10×14), mytinyfont(3×5) in PROGMEM; invader_sprites[3][2][2][5] (Richard Shipman)
  debug.h       — leveled debug macros
  secrets.h     — WiFi credentials (gitignored)
```

Rendering uses a full-matrix `TFT_eSprite` (288×192px, 8-bit depth). `plot()` writes into the sprite; the sprite is pushed to screen at the end of each logical update via `pushSprite()`. This eliminates per-pixel SPI calls and prevents flicker on slide animations. TFT_eSPI auto-expands RGB332→RGB565 on push — visually transparent with only two amber tones in use.

Runtime config is stored in NVS (Preferences namespace `"pclock"`) and loaded at boot before `initDisplay()` so brightness, colours, clock mode, and ampm are applied from the first frame. The `RuntimeConfig` struct (`config_nvs.h`) holds all settings; `saveConfig()` is called by the web handler on every POST to `/api/config`.

**Two-step flash required:** `pio run -t upload` (firmware) + `pio run -t uploadfs` (LittleFS web assets). The `uploadfs` step is only needed when files in `data/` change.

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
- `web.cpp` must use `fs::File` (namespace-qualified) when opening LittleFS files — TFT_eSPI headers pull in a conflicting unqualified `File` symbol. Unqualified `File` will not compile.
- Changing clock mode via WebUI saves to NVS and switches the browser clock immediately, but the physical CYD display only switches on the next touch or restart. This is a known limitation (see CHANGELOG.md To Do).
- `ledColourChanged` (bool, `display.cpp`) is set by `setLedColours()` after a WebUI colour change. Each mode loop checks it on the next iteration, resets to `false`, then forces a full repaint of on-state pixels (Slide redraws all digits; Pong triggers restart; Digits/Word/Invaders invalidate their minute-cache sentinel). The off-state pixels are handled immediately by `setLedColours()` itself via `cls()`+`pushMatrix()`.
- `pong-logo.png` in `data/` must be a trimmed black-on-white PNG (no white padding). CSS `filter: invert(1)` + `mix-blend-mode: screen` renders it white-on-transparent on the dark UI. White padding in the source PNG produces a visible dark rectangle after inversion — trim with `convert -trim` (ImageMagick) before adding to `data/`.

## Flashing Notes

- Port: `/dev/cu.usbserial-*` (CP2102)
- Upload speed: 230400 (921600 can fail on some cables)
- First flash requires two steps: `pio run -t upload` then `pio run -t uploadfs`
- After flash: check Serial for `Display initialised 320x240`, `WiFi connected`, `LittleFS mounted`, and `Web server started — http://…` then verify clock shows on screen within ~10 s of NTP sync.
- Open `http://<device-ip>/` in a browser to confirm the WebUI loads and the live clock canvas is animating.

## Rules

- Global rules: `~/.claude/CLAUDE.md`
- Type rules: `~/.claude/rules/cyd-esp32.md`
