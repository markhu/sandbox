# PlatformIO ESP32 Blink Quickstart

This is a minimal, friendly starting point for your ESP32 DevKit (WROOM) using PlatformIO + Arduino framework.

## What you get

- `platformio.ini` configured for `esp32dev` board (ESP32-WROOM-32)
- `src/main.cpp` simple blink + serial log at 115200 baud
- Fast upload speed (460800) and debug log level set to 3

## Folder structure

```text
platformio-blink/
  platformio.ini      # Project configuration
  src/
    main.cpp          # Your application code
  .pio/               # (Generated) build artifacts - auto-created
  include/            # (Create if you add headers)
  lib/                # Local project-specific libraries
  test/               # Unity-based tests (optional)
```

## Basic commands (run inside this folder)

```bash
pio run                 # Build
pio run -t upload       # Build + flash
pio device monitor -b 115200   # Serial monitor
pio run -t upload -t monitor   # Flash then open monitor (chain targets)
```

If the port isn't auto-detected, specify it:

```bash
pio run -t upload --upload-port /dev/cu.SLAB_USBtoUART
pio device monitor -b 115200 -p /dev/cu.SLAB_USBtoUART
```

### VS Code GUI equivalents

- Ant head icon (bottom bar) opens PlatformIO Home
- Checkmark: Build
- Arrow (rightwards): Upload
- Plug icon: Serial Monitor
- Trash can: Clean

## Adjusting the LED pin

If your board's LED is different, build with a custom macro:

```bash
pio run -t upload -DLED_PIN=5
```

Or add to `build_flags` in `platformio.ini`:

```ini
build_flags = -DCORE_DEBUG_LEVEL=3 -DLED_PIN=5
```

## Adding a library

Search and install (example: WiFi manager):

```bash
pio lib search "WiFi manager"
pio lib install "WiFiManager"
```

Include in code:

```cpp
#include <WiFiManager.h>
```

## Switching to ESP-IDF framework later

Add another environment in `platformio.ini`:

```ini
[env:esp32-idf]
platform = espressif32
board = esp32dev
framework = espidf
monitor_speed = 115200
```

Then build that environment:

```bash
pio run -e esp32-idf
```

## Common issues

- Permission / port busy: close other serial apps (screen, miniterm) before uploading.
- Failed to connect: hold BOOT, tap EN, release BOOT; retry upload.
- Garbage serial output: confirm 115200 baud; try 74880 for early ROM messages.

## Next ideas

- Add WiFi connectivity (scan, connect)
- Serve a tiny web page
- Read a sensor (DHT22, BME280, etc.)
- Use deep sleep + wakeup timer
- Migrate to ESP-IDF environment for advanced features

## Wi-Fi credentials (build-time vs NVS)

This project supports three tiers of Wi-Fi credential handling:

1. Build-time injection (environment variables) – `WIFI_SSID` / `WIFI_PASS` -> generates `include/generated_secrets.h` (gitignored) when `USE_WIFI` is defined.
2. NVS persistence – enable `USE_WIFI_NVS` to load/save credentials in flash (Preferences API). First successful connection using build-time or provisioned creds persists them.
3. Serial provisioning – add `ALLOW_WIFI_PROVISION` to allow entering creds at runtime over serial (no recompilation). Format: `wifi:MySSID:MyPassword` followed by Enter. To clear: `wifi:clear`.

Optional macro: `CLEAR_WIFI_NVS` (one build) to erase stored creds on boot.

### Enabling flags example

```ini
build_flags = \
  -DCORE_DEBUG_LEVEL=3 \
  -DLED_PIN=2 \
  -DUSE_OLED -DOLED_ADDR=0x3C -DOLED_WIDTH=128 -DOLED_HEIGHT=64 \
  -DUSE_WIFI -DUSE_WIFI_NVS -DALLOW_WIFI_PROVISION
```

To clear stored credentials once:

```ini
build_flags = ${env:esp32dev.build_flags} -DCLEAR_WIFI_NVS
```
Remove `-DCLEAR_WIFI_NVS` after that flash.

### Runtime provisioning

Open serial monitor (115200) and type:

```text
wifi:MyNetwork:SuperSecretPass
```
If using OLED you'll briefly see status update. After first connect (with NVS enabled) credentials are stored; you can remove `USE_WIFI` build-time env injection afterward.

### Security notes

- NVS stores plaintext in flash (like most basic embedded approaches). For higher security use ESP-IDF + flash encryption + secure boot.
- Avoid committing any generated `generated_secrets.h` (already gitignored).
- For shared devices prefer serial provisioning over embedding creds in binary.

Enjoy hacking! 🚀

## Optional extensions

### Async web server & JSON API

Enable with:

```ini
build_flags = ${env:esp32dev.build_flags} -DUSE_ASYNC_WEB
```

Routes:

- `/` dashboard (HTML + JS auto-refresh)
- `/api/status` returns JSON: `{led,count,uptime_ms,ip?}`
- `/api/toggle` POST toggles LED (204 No Content)

### mDNS hostname

Enable with:

```ini
build_flags = ${env:esp32dev.build_flags} -DUSE_MDNS -DMDNS_HOST=\"esp32-blink\"
```

Then browse to: `http://esp32-blink.local/` (macOS / Linux Avahi by default). Adds `_http._tcp` service.

### BLE advertising

Enable with:

```ini
build_flags = ${env:esp32dev.build_flags} -DUSE_BLE_ADV -DBLE_ADV_NAME=\"ESP32Blink\"
```

Advertises a name and manufacturer data `LED=0|1` updating every ~2s. You can view with nRF Connect or LightBlue.

### Combining

You can stack these flags; typical combo:

```ini
build_flags = \
  -DCORE_DEBUG_LEVEL=3 -DLED_PIN=2 -DUSE_WIFI -DUSE_WIFI_NVS -DALLOW_WIFI_PROVISION \
  -DUSE_OLED -DOLED_ADDR=0x3C -DOLED_WIDTH=128 -DOLED_HEIGHT=64 \
  -DUSE_ASYNC_WEB -DUSE_MDNS -DMDNS_HOST=\"esp32-blink\" -DUSE_BLE_ADV -DBLE_ADV_NAME=\"ESP32Blink\"
```

### Notes

- Async server requires libraries `ESPAsyncWebServer-esphome` and `AsyncTCP` (already added in `platformio.ini`).
- BLE + Wi-Fi concurrency increases RAM usage slightly; current build footprint leaves ample headroom.
- Adjust refresh logic or add authentication if exposing beyond local LAN.
