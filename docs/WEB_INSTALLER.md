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

Not run yet.

---

## Tests owed

Smoke-tested only. Run these on the next real work on this project, or before
the next release, and tick them off with date and board MAC.

- [ ] Case 1 – fresh install, erased, on each remaining board
- [ ] Case 2 – Update on a provisioned board (settings kept)
- [ ] Case 3 – Update from `app1` (only if the project has OTA) – N/A, no OTA
- [ ] Case 4 – wrong board image, then reinstall (multi-env only) – N/A, one env
