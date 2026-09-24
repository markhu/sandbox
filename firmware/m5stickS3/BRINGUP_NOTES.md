# M5StickS3 bring-up notes - dark screen issue

## Status

Board: M5Stack StickS3 (SKU K150, ESP32-S3-PICO-1-N8R8). Screen is dark on
boot; `M5.Display.setBrightness(255)` and other app-level display calls run
without error, but nothing appears.

## Ruled out

- **Not a wrong/outdated library.** `M5Unified@^0.2.23` (already resolved
  into `.pio/libdeps/m5stack-stickc-s3/` by the current `platformio.ini`)
  has real, dedicated `board_M5StickS3` support - confirmed by reading the
  actual library source already on disk, not just release notes:
  - `M5GFX/src/lgfx/boards.hpp:37` - `board_M5StickS3 = 26` (real enum
    value, not a stub).
  - `M5GFX/src/M5GFX.cpp` around line 2916 - a full autodetect + panel
    init branch specifically for this board (see below).
  - `M5Unified/src/M5Unified.inl` lines ~103, 2491, 2669, 2887, 3613 -
    additional StickS3-specific handling (e.g. PA/speaker control via
    M5PM1 register writes).
  No need to switch `lib_deps` to the M5Unified GitHub main branch - the
  pinned registry version already has what's needed.

- **Not a flipped-backlight-polarity issue.** `M5GFX.cpp` line ~2980 calls:
  ```cpp
  _set_pwm_backlight(GPIO_NUM_38, 7, 256, false, 16);
  ```
  The 4th parameter is `invert` (signature confirmed at `M5GFX.cpp:1016`
  and `M5GFX.h:198`), passed as `false` - StickS3's backlight is standard
  active-HIGH PWM on GPIO38, not inverted. This was worth checking (a
  related M5Stack board, `m5stampS3A` in this same repo, has a documented
  "peripheral power rail off by default" gotcha), but it doesn't apply
  here - the backlight polarity itself is normal.

## Likely root cause: M5PM1 chip autodetection over I2C

Read the exact detection sequence in `M5GFX.cpp` (~line 2916 onward). Board
autodetection for the StickS3 works like this:

1. Check for I2C pull-ups on **SDA=GPIO47 / SCL=GPIO48**.
2. If present, try to read the **M5PM1 power-management chip's device ID
   register at I2C address `0x6E`** (`m5pm1_i2c_addr`, confirmed at
   `M5GFX.cpp:217`).
3. **Only if that read succeeds** does the library:
   - Set `board = board_t::board_M5StickS3`.
   - Toggle the M5PM1's GPIO2 - source comment: `"PM1_G2 -- L3B Enable,
     LCD Power On"` - i.e. this is the actual LCD panel POWER rail, not
     just brightness.
   - Configure the SPI bus (MOSI=39, SCLK=40, DC=45, spi_3wire), init a
     `Panel_ST7789` (135x240, CS=41, RST=21), and finally call
     `_set_pwm_backlight(GPIO_NUM_38, ...)`.

**If the M5PM1 I2C probe fails for any reason, this entire branch is
skipped silently** - no error is raised, the panel object is simply never
created, LCD power is never enabled, and the backlight code never even
runs. This would look exactly like "screen is dark" while every app-level
display call executes without complaint (they're operating on a panel that
was never actually powered/initialized).

## Next steps to try (in order)

1. **Check what `M5.getBoard()` actually reports** after `M5.begin()` (the
   existing `main.cpp` already logs this via `Serial.printf("Board id:
   %d\n", ...)`). Compare the printed value against `26`
   (`board_M5StickS3`). If it's `0` (unknown/generic), autodetection
   failed - the M5PM1 I2C probe above is where to focus.
2. **Add a raw I2C scan before `M5.begin()`** to confirm the M5PM1 chip
   actually acknowledges at `0x6E` on the SDA=47/SCL=48 bus. If it doesn't
   show up, that points to a hardware/soldering/bus-contention issue
   upstream of anything M5Unified can fix in software, or a bus timing/
   init-order problem if something else touches that I2C bus before
   `M5.begin()` runs.
3. **Check `M5.config()`** for anything overriding I2C/autodetect defaults
   (e.g. explicitly setting a board id, or disabling internal I2C) that
   might be short-circuiting the probe before it runs.

## Source references (exact files/lines read this session)

- `.pio/libdeps/m5stack-stickc-s3/M5GFX/src/M5GFX.cpp`
  - ~line 2916: autodetect + M5PM1 probe + LCD-power-on + panel/backlight init
  - line 1016: `_set_pwm_backlight()` definition (confirms `invert` param)
  - line 217: `m5pm1_i2c_addr = 0x6E`
- `.pio/libdeps/m5stack-stickc-s3/M5GFX/src/M5GFX.h:198` -
  `_set_pwm_backlight()` declaration/default args
- `.pio/libdeps/m5stack-stickc-s3/M5GFX/src/lgfx/boards.hpp:37` -
  `board_M5StickS3 = 26`
- `.pio/libdeps/m5stack-stickc-s3/M5Unified/src/M5Unified.inl` - lines
  ~103, 2491, 2669, 2887, 3613 (other StickS3-specific handling)
- `firmware/m5stampS3A/platformio.ini` (this repo) - the analogous
  "peripheral power rail off by default" precedent that prompted checking
  for a backlight-polarity issue here (ultimately ruled out, see above)
