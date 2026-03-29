# PongClock CYD

A port of Nick Hall's classic **Pong Clock** to the **ESP32-2432S028R (Cheap Yellow Display)**. The original drove two Sure Electronics 2416 LED panels (48×16 combined). This version emulates a 48×32 virtual LED matrix on the ILI9341 320×240 TFT — each virtual LED is a 6×6 rounded rectangle in amber, giving a retro LED panel aesthetic.

Original project: http://123led.wordpress.com/

---

## Gallery — Device

|                                             |                                           |
|:-------------------------------------------:|:-----------------------------------------:|
| ![Slide mode](Images/mode-slide.jpg)        | ![Pong mode](Images/mode-pong.jpg)        |
| **Slide** — digits slide on each tick       | **Pong** — AI game, score = HH:MM         |
| ![Digits mode](Images/mode-digits.jpg)      | ![Word Clock mode](Images/mode-word.jpg)  |
| **Digits** — large 10×14 font, HH:MM        | **Word Clock** — time in words + date     |

---

## Gallery — WebUI

Open `http://<device-ip>/` in any browser. The WebUI runs all four clock modes live in JavaScript — pixel-exact amber LED rendering on a CSS CYD device mockup. No plugins or app required.

|                                                    |                                                  |
|:--------------------------------------------------:|:------------------------------------------------:|
| ![Slide WebUI](Images/webui-slide.jpg)             | ![Pong WebUI](Images/webui-pong.jpg)             |
| **Slide** — browser clock, animated digits         | **Pong** — independent browser game             |
| ![Digits WebUI](Images/webui-digits.jpg)           | ![Word Clock WebUI](Images/webui-word.jpg)       |
| **Digits** — big font with flashing colon          | **Word Clock** — words + date                    |

| ![Config UI](Images/webui-config.jpg) |
|:-------------------------------------:|
| **Config tab** — all settings, WiFi reset |

---

## Gallery — All Modes in Action

| ![All modes demo](Images/modes-demo.gif) |
|:----------------------------------------:|
| **All four modes cycling** — Slide · Pong · Digits · Word Clock |

---

## Features

- NTP time sync via WiFi (ezTime — timezone + POSIX DST fallback in `config.h`)
- WiFiManager captive portal for first-run credential entry
- Animated splash screen on boot (typewriter reveal + CRT-collapse exit, ≈2.3 s)
- Four clock modes with touch-to-switch; date always on screen
- Long-press anywhere for brightness cycle (4 levels); optional LDR auto-brightness
- Retro amber LED look — 6×6 rounded pixel per virtual LED, 48×32 matrix
- **Full browser WebUI** — all four clock modes reimplemented in JavaScript; runs independently in any browser at `http://<device-ip>/`
- **Config WebUI** — timezone, brightness, LED colours, 12/24h, NTP server, date interval, LDR, WiFi reset; all settings persisted to NVS (survive reboot)

---

## Clock Modes

| # | Mode           | Description                                                           |
|:--|:---------------|:----------------------------------------------------------------------|
| 0 | **Slide**      | Each digit slides up on change — HH:MM:SS; date row permanently below |
| 1 | **Pong**       | AI pong — score = HH / MM; date permanently at bottom of field        |
| 2 | **Digits**     | Large 10×14 font — HH:MM with flashing colon                          |
| 3 | **Word Clock** | Time in words + date (e.g. "TWENTY PAST / FIVE / WED 18 MAR")        |

Clock starts in the last-saved mode (default: **Slide** on first boot).

---

## Touch Controls

The touchscreen is divided into two halves:

```
┌─────────────────┬─────────────────┐
│                 │                 │
│   Tap here      │   Tap here      │
│  ◄ PREV mode    │   NEXT mode ►   │
│                 │                 │
└─────────────────┴─────────────────┘
         Hold anywhere ≥ 0.6 s
              = Brightness cycle
```

| Touch                  | Action                                                |
|:-----------------------|:------------------------------------------------------|
| Short tap — left half  | Previous mode                                         |
| Short tap — right half | Next mode                                             |
| Hold ≥ 600 ms          | Cycle brightness (4 levels: dim → mid → bright → max) |

---

## Web Interface

When WiFi is connected a web server starts on port 80. Open `http://<device-ip>/` — the IP is printed to serial at boot.

### Endpoints

| Endpoint            | Method    | Description                                          |
|:--------------------|:----------|:-----------------------------------------------------|
| `/`                 | GET       | Browser WebUI — live clock + config (served from LittleFS) |
| `/clock.js`         | GET       | JavaScript clock engine                              |
| `/style.css`        | GET       | WebUI stylesheet                                     |
| `/screenshot.bmp`   | GET       | Current display as 24-bit BMP (288×192), download    |
| `/api/info`         | GET       | JSON — firmware, mode, brightness, uptime, heap, IP  |
| `/api/config`       | GET       | JSON — all runtime settings                          |
| `/api/config`       | POST      | Apply + persist settings (partial JSON patch)        |
| `/api/wifi-reset`   | POST      | Erase WiFi credentials, restart into AP mode         |

### `/api/config` fields

| Field           | Type    | Description                                    |
|:----------------|:--------|:-----------------------------------------------|
| `mode`          | int     | 0=Slide 1=Pong 2=Digits 3=WordClock            |
| `brightness`    | int     | Backlight PWM 0–255                            |
| `ampm`          | bool    | false=24h true=12h                             |
| `ledOnR/G/B`    | int     | LED-on colour (default amber 255/140/0)        |
| `ledOffR/G/B`   | int     | LED-off colour (default dark amber 20/8/0)     |
| `timezone`      | string  | Olson timezone e.g. `"Australia/Sydney"`       |
| `ntpServer`     | string  | NTP server hostname                            |
| `dateInterval`  | int     | Minutes between periodic date displays         |
| `ldrEnabled`    | bool    | Auto-brightness from LDR sensor                |

Example `/api/info` response:

```json
{
  "firmware": "0.5",
  "mode": 1,
  "modeName": "Pong",
  "brightness": 180,
  "uptime": 3742,
  "freeHeap": 187456,
  "ip": "192.168.1.26"
}
```

The server is offline-safe: `initWeb()` is a no-op if WiFi did not connect.

---

## Hardware

- **Board:** ESP32-2432S028R (CYD — Cheap Yellow Display)
- **Display:** ILI9341 · 320×240 · SPI (HSPI — MOSI=13, SCLK=14, CS=15, DC=2)
- **Touch:** XPT2046 · dedicated VSPI bus (CLK=25, MISO=39, MOSI=32, CS=33)
- **Backlight:** GPIO 21 via LEDC PWM
- **No hardware modifications required** — standard CYD pinout throughout

---

## Build & Flash

```bash
# Build only
pio run

# Build and upload firmware
pio run --target upload

# Upload web assets to LittleFS (required once, then only when data/ files change)
pio run --target uploadfs

# Serial monitor (115200 baud)
pio device monitor --baud 115200
```

> **Note:** Both `upload` and `uploadfs` are required on first flash. After that, `uploadfs` is only needed when files in `data/` change.

---

## First Boot

1. Flash firmware (`pio run -t upload`) then web assets (`pio run -t uploadfs`).
2. The screen shows **"Connecting WiFi..."** — a `CYD-PongClock` access point appears.
3. Connect your phone to `CYD-PongClock` and enter your WiFi credentials in the captive portal.
4. Device reboots, connects, and shows **"Syncing NTP..."**.
5. Once synced, the clock starts in the last-saved mode (Slide on first boot).
6. The web server starts — IP is shown in serial (`[INFO] Web server started — http://…`).
7. Open `http://<device-ip>/` in a browser for the WebUI.

If WiFi times out (60 s) the clock continues offline; the web server is skipped.

---

## Configuration

### Via WebUI (recommended)

Open `http://<device-ip>/` → **Config** tab. All settings are applied immediately and persisted to NVS (survive reboot). Changes to mode, brightness, and LED colours are reflected on the physical display on the next touch or restart.

### Via `config.h` (compile-time defaults)

All compile-time constants are in [`include/config.h`](include/config.h). These serve as the factory defaults loaded on first boot before any NVS values exist.

#### Time & Location

| Constant              | Default              | Purpose                      |
|:----------------------|:---------------------|:-----------------------------|
| `NTP_TIMEZONE`        | `"Australia/Sydney"` | Olson timezone name          |
| `NTP_POSIX_FALLBACK`  | `"AEST-10AEDT,…"`   | Offline DST-correct fallback |
| `NTP_SYNC_TIMEOUT_S`  | `20`                 | Seconds to wait for NTP sync |
| `AMPM_MODE`           | `0`                  | 0 = 24 h, 1 = 12 h           |

#### Display

| Constant               | Default   | Purpose                           |
|:-----------------------|:----------|:----------------------------------|
| `BRIGHTNESS_DEFAULT`   | `180`     | Startup backlight PWM (0–255)     |
| `BRIGHTNESS_STEPS`     | `4`       | Levels cycled by long-press       |
| `COLOUR_LED_ON_R/G/B`  | 255/140/0 | Amber — lit LED colour            |
| `COLOUR_LED_OFF_R/G/B` | 20/8/0    | Dark amber — unlit LED colour     |

#### Animation

| Constant          | Default | Purpose                             |
|:------------------|:--------|:------------------------------------|
| `SLIDE_DELAY`     | `20`    | ms per frame in Slide animation     |
| `PONG_BALL_DELAY` | `20`    | ms per frame in Pong mode           |
| `FADE_DELAY`      | `25`    | ms per step in mode-transition fade |

#### Touch Calibration

| Constant              | Default | Purpose                                 |
|:----------------------|:--------|:----------------------------------------|
| `TOUCH_PRESSURE_MIN`  | `200`   | Minimum Z to register a touch           |
| `TOUCH_X_LEFT`        | `200`   | Raw X at left screen edge               |
| `TOUCH_X_RIGHT`       | `3800`  | Raw X at right screen edge              |
| `TOUCH_LONG_PRESS_MS` | `600`   | Hold duration (ms) for brightness cycle |

If taps aren't registering on the correct side, check serial for raw `x=` values and adjust `TOUCH_X_LEFT` / `TOUCH_X_RIGHT`.

#### Date Display

| Constant            | Default | Purpose                                          |
|:--------------------|:--------|:-------------------------------------------------|
| `DATE_DISPLAY_MINS` | `10`    | Minutes between date displays (Pong/Digits/Word) |

#### Auto-Brightness (LDR)

Disabled by default (`LDR_ENABLED=0`). Enable via WebUI Config tab or by setting `LDR_ENABLED=1` in `config.h`.

| Constant         | Default | Purpose                                  |
|:-----------------|:--------|:-----------------------------------------|
| `LDR_ENABLED`    | `0`     | 1 = auto-brightness; 0 = long-press only |
| `LDR_PIN`        | `34`    | ADC pin for LDR (input-only)             |
| `LDR_DARK`       | `200`   | ADC value → `BRIGHTNESS_MIN`             |
| `LDR_BRIGHT`     | `2500`  | ADC value → `BRIGHTNESS_MAX`             |
| `BRIGHTNESS_MIN` | `15`    | Minimum backlight PWM in auto mode       |
| `BRIGHTNESS_MAX` | `255`   | Maximum backlight PWM in auto mode       |

#### Debug

| Constant            | Default | Purpose                                      |
|:--------------------|:--------|:---------------------------------------------|
| `DEBUG_LEVEL`       | `3`     | 1=Error 2=Warn 3=Info 4=Verbose (build flag) |
| `DEBUG_SLIDE_TIME`  | `1`     | Print `[Slide] HH:MM:SS` on each tick        |
| `DEBUG_DIGITS_TIME` | `1`     | Print `[Digits] HH:MM` on each minute        |
| `DEBUG_PONG_TIME`   | `1`     | Print `[Pong] HH:MM:SS` on each rally        |
| `DEBUG_WORD_TIME`   | `1`     | Print `[Word] HH:MM` on each minute          |

---

## Serial Output Reference

Typical boot sequence:

```
[INFO] === PongClock CYD starting ===
[INFO] Config loaded: mode=1 bright=180 ampm=0 tz=Australia/Sydney
[INFO] Display initialised 320x240 rotation=1
[INFO] Display colours initialised, sprite 288x192 depth=8 heap=198232
[INFO] Touch initialised (VSPI CLK=25 MISO=39 MOSI=32 CS=33)
[INFO] WiFi connected: 192.168.1.26
[INFO] Time synced: 16:29:42 17-Mar-2026  status=2  tz=AEDT offset=+1100
[INFO] LittleFS mounted — 48 KB used / 384 KB total
[INFO] Web server started — http://192.168.1.26/
[INFO] Free heap: 184320 bytes
[INFO] Slide mode entry: 16:29:42  NTP status=2
```

NTP `status=2` = synced. `status=0` = not set (offline).

---

## Project Structure

```
CYD_PongClock/
├── platformio.ini
├── partitions_custom.csv
├── README.md
├── CHANGELOG.md
├── CLAUDE.md
├── data/                        ← LittleFS web assets (pio run -t uploadfs)
│   ├── index.html               — SPA: Clock tab + Config tab
│   ├── clock.js                 — JS clock engine (all 4 modes + fonts)
│   └── style.css                — CYD mockup + UI styling
├── include/
│   ├── config.h                 — compile-time defaults
│   ├── config_nvs.h             — RuntimeConfig struct + NVS API
│   ├── display.h
│   ├── clock.h
│   ├── web.h
│   ├── fonts.h                  — PROGMEM font data (5×7, 10×14, 3×5)
│   └── debug.h
├── src/
│   ├── main.cpp
│   ├── display.cpp
│   ├── clock.cpp
│   ├── config_nvs.cpp           — NVS persistence via Preferences
│   └── web.cpp                  — HTTP server + LittleFS serving
└── Images/
    ├── mode-slide.jpg            — device photos
    ├── mode-pong.jpg
    ├── mode-digits.jpg
    ├── mode-word.jpg
    ├── webui-slide.svg           ← placeholder — replace with browser screenshot
    ├── webui-pong.svg            ← placeholder — replace with browser screenshot
    ├── webui-digits.svg          ← placeholder — replace with browser screenshot
    ├── webui-word.svg            ← placeholder — replace with browser screenshot
    └── webui-config.svg          ← placeholder — replace with browser screenshot
```

---

## Credits

- Original Pong Clock by **Nick Hall** — http://123led.wordpress.com/
- Modifications by **Brett Oliver** (v7.x) — https://www.brettoliver.org.uk/Pong_Clock/Pong_Clock.htm
- CYD port using [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) by Bodmer
- CYD port and WebUI by **Anthony Clarke** — https://github.com/anthonyjclarke/CYD_PongClock
