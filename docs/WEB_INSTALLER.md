# Web installer – CYD_PongClock

The browser installer at <https://anthonyjclarke.github.io/CYD_PongClock/> is
built by the shared tooling in
[cyd-web-installer](https://github.com/anthonyjclarke/cyd-web-installer). This
file records what is specific to this project and its hardware test results.

---

## Project specifics

| Item                | Value                                        |
|:--------------------|:---------------------------------------------|
| `PROJECT_NAME`      | `CYD_PongClock` (frozen)                     |
| Env / board         | `myclock_cyd` – CYD 2.8″ ESP32-2432S028R     |
| Platform            | `espressif32@6.12.0` (arduino-esp32 2.0.17)  |
| Partitions          | `partitions_custom.csv`, app slot `0x1C0000` |
| OTA                 | None – the `app1` case does not apply        |
| Web UI              | PROGMEM via `tools/embed_web.py` (option C)  |
| Improv              | Core-0 task, `improvTick()` every 20 ms      |
| Setup AP            | `CYD-PongClock`                              |

### Why a task for Improv

The clock modes block for seconds at a time: `display_date()` runs about 10 s
every few minutes, and the splash, boot IP, NTP wait and first-run WiFi portal
all block in `setup()`. Ticking Improv from `loop()` would miss ESP Web Tools'
1.5 s Connect window, so a FreeRTOS task on core 0 services it instead and no
clock code has to change.

### Settings across an Update

All settings are in NVS namespace `pclock` and WiFi is in the ESP WiFi NVS,
so an Update (four parts, no erase) keeps both. Before 0.8.0 a version change
reset the clock mode to Slide; 0.8.0 keeps it.

### Build sizes

| Build                                 | `firmware.bin`            |
|:--------------------------------------|:--------------------------|
| 0.7.3, unpinned (arduino-esp32 3.x)   | 1,169,888 B – 63.7 % slot |
| 0.8.0-dev, 6.12.0, PROGMEM UI         | 1,033,424 B – 56.0 % slot |

---

## Smoke test (RUNBOOK 5a)

**Passed 09-10-2026** – CYD 2.8″ ESP32-2432S028R, ESP32-D0WD-V3 rev 3.1,
MAC `b0:cb:d8:da:ae:8c`, macOS desktop Chrome, CI preview of `623f82f`
(`0.8.0-dev`) served from localhost.

| Step                                   | Result                              |
|:---------------------------------------|:------------------------------------|
| CI Firmware run 37897362433            | Green; `build` OK, publish skipped  |
| `pio run -t erase`, then Install + erase | Flashed, splash shows `v0.8.0`    |
| Configure WiFi (Improv)                | Joined WiFi, restarted              |
| Connect again                          | "Connected to PongClock" 0.8.0-dev  |
| Boot log after reset                   | `Running from app0`, WiFi, NTP OK   |
| Web UI from PROGMEM                    | `/`, CSS, JS, PNG byte-exact; 404 OK |

Boot log (after the dialog was closed):

```text
[INFO] Improv: listening on Serial as PongClock-CBB0
[INFO] === CYD_PongClock v0.8.0-dev starting ===
[INFO] Running from app0
[INFO] Config loaded: mode=0 bright=180 ampm=0 tz=Australia/Sydney
[INFO] WiFiManager portal timeout: 0 s (no saved credentials)
*wm:Connecting to SAVED AP: …
[INFO] WiFi connected: 192.168.1.95
[INFO] Time synced: 18:18:08 09-Oct-2026  status=2  tz=AEDT offset=+1100
[INFO] Web server started — http://192.168.1.95/
[INFO] Free heap: 191412 bytes
```

## Release check – Update from the live page (case 2)

**Passed 09-10-2026** on the same board (MAC `b0:cb:d8:da:ae:8c`), live page
<https://anthonyjclarke.github.io/CYD_PongClock/> serving v0.8.0 (tag run
37898791330).

1. The board was set to mode 1 (Pong) and brightness 150 through
   `POST /api/config`. Those values survived a reset.
2. It was flashed from PlatformIO as `0.8.0-dev.0` (app only, version edit not
   committed). The mode stayed Pong across that version change, which
   confirms the 0.8.0 change that stopped resetting it.
3. Connect on the live page offered **Update CYD_PongClock** with no erase.
4. After the Update: `/api/info` says `0.8.0`, and `/api/config` still has
   mode 1 and brightness 150. The boot log shows `Running from app0`,
   `Config loaded: mode=1 bright=150` and `WiFi connected`.

An earlier Update (0.8.0-dev → 0.8.0) also worked and kept WiFi, but every
setting was still at its default then, so it proved nothing about settings.
Mode and brightness changed **by touch** are never saved to NVS – only web UI
changes are – so they reset on any reboot, Update or not.

Found in passing, not caused by this work: `initWiFi()` reads `WiFi.SSID()`
before the WiFi driver starts, so it always sees no saved SSID and sets the
portal timeout to 0. A board whose saved network is down then waits in the
portal indefinitely instead of going offline after `WIFI_TIMEOUT_S`.

---

## Tests owed

Smoke-tested only. Run these on the next real work on this project, or before
the next release, and tick them off with date and board MAC.

- [ ] Case 1 – fresh install, erased, on each remaining board
- [x] Case 2 – Update on a provisioned board (settings kept) – 09-10-2026, `b0:cb:d8:da:ae:8c`
- [ ] Case 3 – Update from `app1` (only if the project has OTA) – N/A, no OTA
- [ ] Case 4 – wrong board image, then reinstall (multi-env only) – N/A, one env
