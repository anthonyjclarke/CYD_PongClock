# PongClock_CYD

## Project

Port of Nick Hall's Pong Clock (v7.5) to the CYD: a 48×32 virtual amber LED matrix
(6 px per LED, 288×192 sprite) on the ILI9341, five touch-switched modes, NTP time,
and a web UI with a JS replica of every mode plus a config tab. v0.8.0 adds the
ESP Web Tools installer. Features, API and layout are in `README.md`.

## Hardware

- Board: ESP32-2432S028R (CYD 2.8″), env `myclock_cyd`, no PSRAM.
- Touch: own pins (CLK 25, MISO 39, MOSI 32, CS 33), but the **same VSPI peripheral
  as the display** — see *Shared SPI bus* below.
- Backlight GPIO 21 on LEDC channel `BACKLIGHT_LEDC_CH` (0).

## Architecture decisions

- Matrix sprite is **8-bit** (`setColorDepth(8)` before `createSprite()`): 16-bit at
  288×192 needs 110 KB contiguous heap, which fails without PSRAM.
- Web UI source is `data/`, but it is **not** a filesystem image: `tools/embed_web.py`
  (pre-build) writes the gitignored `src/web_assets.h`; `web.cpp` serves it with
  `send_P`. LittleFS is not mounted. New UI files need an entry in `WEB_FILES` and a
  route in `initWeb()`.
- Improv runs in a core-0 FreeRTOS task (`improvTask`, `main.cpp`), not from `loop()`:
  modes block for up to ~10 s (`display_date()`), longer than ESP Web Tools' 1.5 s.
- NVS namespace `"pclock"`, keys: `mode bright ampm onR onG onB offR offG offB tz ntp
  dateInt ldr fwver`. `fwver` is logged only — never reset settings on a version
  change, because every installer Update changes the version.

## Web installer and releases

- Release images come only from CI on a `v*` tag on `main`; never publish a local build.
- Never put `firmware-merged.bin` in a manifest (it fills NVS with 0xFF).
- `PROJECT_NAME` and `partitions_custom.csv` are frozen; renaming turns Update into Install.
- Never put Improv back in `lib_deps` — `lib/ImprovWiFi` is the patched copy-in.
- `improvTick()` must run at least every ~1 s (the task does it every 20 ms).
- Platform pinned to `espressif32@6.12.0` (arduino-esp32 2.0.17): use the 2.0.x LEDC
  channel API (`ledcSetup`/`ledcAttachPin`/`ledcWrite(ch, …)`), not 3.x `ledcAttach`.
- Before the next release, clear *Tests owed* in `docs/WEB_INSTALLER.md` (RUNBOOK 5b).

## Known Issues / Quirks

- Touch SPI ≤ 2.5 MHz; guard reads with `tirqTouched() && touched()`.
- `slideanim()` renders row-by-row from PROGMEM font data — do not simplify to
  whole-char calls or the overlap during the transition corrupts.
- `mybigfont` is 10×14 stored as 20 bytes per glyph; see `ht1632_putbigchar()`.
- `myTZ.setLocation()` calls timezoneapi.io and reliably fails right after WiFi
  connects. Always fall back to `myTZ.setPosix(NTP_POSIX_FALLBACK)`.
- TFT_eSPI defines `FS_NO_GLOBALS`; `WebServer.h` needs bare `FS`, hence
  `using fs::FS;` after the TFT_eSPI include in `main.cpp` and `web.cpp`.
- `ledColourChanged` (`display.cpp`) makes each mode force a full repaint of lit
  pixels on its next pass after a WebUI colour change.
- `pong-logo.png` must be a trimmed black-on-white PNG; the CSS inverts it, and white
  padding shows as a dark box.
- WiFiManager portal timeout is 0 (stays open) when no SSID is saved, else `WIFI_TIMEOUT_S`.
- `debugLevel` in `debug.h` is compile-time only; there is no `/api/debug` endpoint.
- `_CFGLOG` macro in `web.cpp`: `_L` clashes with `ctype.h`.
- **Shared SPI bus — latent, not a live bug (checked 11-09-2026).** Display and
  touch both run on VSPI: without `-DUSE_HSPI_PORT`, TFT_eSPI 2.5.43 creates
  `SPIClass(VSPI)`, and `initTouch()` (`clock.cpp`) creates a second one on pins
  25/32/39. `-DTOUCH_CS=33` also makes TFT_eSPI drive GPIO 33 during `tft.init()`.
  - *Why it works:* TFT_eSPI always uses SPI transactions on the ESP32, nothing
    draws or touches SPI from a second task (the Improv task uses Serial only),
    and `tft.init()` runs before `initTouch()`. The bus
    reads MISO from whichever `begin()` ran last, which is touch's GPIO 39.
  - *It breaks if* anything reads back from the panel (`readPixel`, `readRect`,
    `readcommand`), `tft.init()` is called again after `initTouch()`, or
    TFT_eSPI's own `getTouch()` is used.
  - *Fix:* add `-DUSE_HSPI_PORT` (MOSI 13 / MISO 12 / SCLK 14 / CS 15 are HSPI's
    native pins), remove `-DTOUCH_CS=33`, give the touch CS its own name in
    `config.h` (`clock.cpp` uses `TOUCH_CS` for the constructor and
    `touchSPI.begin()`), then test touch on hardware.
  - Checked against arduino-esp32 2.0.17 source, which is what the pinned
    `espressif32@6.12.0` now uses.
  - Found while porting CYD_AnimatedPixelClock, which now uses `USE_HSPI_PORT`.
    There the shared bus was **not** the cause of dead touch — that was a faulty
    board — so do not treat it as a known failure.

## Flashing Notes

- Port `/dev/cu.usbserial-*` (CP2102); upload speed 230400 (921600 fails on some cables).
- Boot log to check: `Running from app0`, `Display initialised 320x240`,
  `WiFi connected`, `Web server started — http://…`.

## Rules

- Global rules: `~/.claude/CLAUDE.md` · Type rules: `~/.claude/rules/cyd-esp32.md`
