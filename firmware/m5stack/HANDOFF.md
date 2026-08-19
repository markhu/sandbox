# ClickLogger Project Handoff

**Last updated**: 2026-08-18, ~15:50
**Purpose**: M5StickC Plus firmware that listens (mic) to a Rain Bird DC latching
solenoid valve, detects each open/close "click," and tries to determine
which click is which (open vs. close) using whatever sensing modality works.

---

## Current physical setup (IMPORTANT - don't lose this)

- **The M5StickC Plus is rubber-banded directly to the CORRECT solenoid**,
  side-by-side/parallel orientation (mic and solenoid pointing the same
  direction, tight contact). This is the current mount - **do not switch to
  the popsicle-stick "mic facing actuator port" bridge** without discussion;
  the user is deliberately keeping this position because it's the best shot
  for Hall-sensor sensing (see below).
- Earlier in the session we spent a long time testing with the **WRONG
  solenoid** (a different unit was rubber-banded to the sensor). All
  acoustic/positioning conclusions from before ~14:45 today are about the
  WRONG device and should be disregarded or re-verified before relying on
  them.
- Test irrigation controller: Rain Bird app (`com.rainbird.rainbird2dev`) on
  a Pixel 7a, adb serial `34181JEHN20500`. The "aa BAT-PRO-6 DC53" device's
  Program 1 has a 20-second manual-run station. Tapping "Run Now" triggers
  it (via Bluetooth from phone to the BAT-PRO controller - can drop and need
  reconnecting, watch for a "Connecting..." modal on screen).
- Automation script: `~/git/monorepo/utils/adb-screencap.py` (this went
  missing once mid-session, unrelated to this project, then was restored by
  the user - if it disappears again, that's on the monorepo side, not ours).

## Current firmware state (volatile - resets on every reboot!)

`THRESH_ON`/`THRESH_OFF`, `SNIPON`/`SNIPOFF`, `HALLON`/`HALLOFF` are all
**RAM-only** and reset to compiled-in defaults on every reboot/reflash. As of
last check:
- `THRESH_ON=4500` `THRESH_OFF=2250` (default compiled-in is 6000/3000 -
  4500 has been our working value for this correct-solenoid/tight-contact
  setup)
- `HALLON` was just re-enabled (Hall-sensor research columns being logged)
- Snippet mode (`SNIPON`) - NOT currently re-enabled after the last reboot;
  turn back on if doing raw-audio spectral analysis again.

**After any reflash, re-run**:
```
THRESH 4500
HALLON
(SNIPON if doing spectral/snippet analysis)
CLEAR
```

## Infrastructure already running

- **`push_receiver.py`** (in this dir) running in background on this Mac,
  port 8090, PID may vary - check with `ps aux | grep push_receiver`. Logs
  to `push_received.log` (CSV pushes) and `snippets_received.bin` (raw audio
  snippets, when SNIPON was on). This Mac's LAN IP was `192.168.1.82` as of
  last check, but can change - `secrets.h`'s `PUSH_HOST` needs to match.
- Board auto-pushes `/clicks.csv` (and `/snippets.bin` if SNIPON) to this
  receiver every 5 min (`PUSH_INTERVAL_MS`), clearing the on-device file on
  success - so historical data lives in `push_received.log`, not just
  on-device.
- Board's WiFi network: `orcYard` (see `ClickLogger/src/secrets.h`, gitignored).
- Manual retrieval: `SERVE` over serial (or front-button tap) turns WiFi on,
  then `curl http://<board-ip>/clicks.csv` and `/snippets.bin`. IP shown on
  device screen/serial when SERVE starts.

## What's been tried, and the verdict

### ✅ Working / reliable
- **Timing-based open/close prediction**: a click starting >25s
  (`TIMING_GAP_THRESHOLD_MS`) after the previous one is the START of a cycle
  (valve opening, digit `'1'`); one arriving sooner is the END (valve
  closing, digit `'0'`). **100% consistent across every test, every
  physical mount** - this is the one thing that actually works. Valve is
  normally-closed, opens briefly, per the user (an irrigation engineer) -
  digit convention was flipped once and then corrected to match this model.
- Push/serve/button WiFi infrastructure, screen UI (WiFi/USB glyphs, mixed
  small/normal font per line, Hall+freq+indicator display) - all solid,
  described in the `.ino`'s top comment block.

### ❌ Ruled out / not working
- **Acoustic classification** (duration, peak amplitude, ZCR frequency
  estimate, real-FFT spectral centroid, dominant bin, band-energy ratios):
  tested across 3 physical mounts (parallel/vibration-coupled, mic-facing-
  actuator-port popsicle-stick bridge, and tight-parallel-on-correct-
  solenoid) with dozens of samples each. **No feature reliably separates
  open from close.** Duration itself has a rough bimodal split (~17-18ms vs
  ~23-24ms) but per the timing-based ground truth, this split does NOT
  consistently track which click is open vs. close (near-even split across
  all 4 possible agree/disagree combinations in one test).
- **Hall-effect sensor** (`hallRead()`, built into ESP32, GPIO36/39, free/
  unused by M5StickCPlus lib): implemented to test the theory that a DC
  latching solenoid reverses coil current direction between open/close,
  which should show opposite magnetic polarity. **Real click events show
  large bidirectional deviation from baseline (both very negative min AND
  very positive max in almost every event)**, not a clean single-direction
  polarity per class. Worse: **finger snaps produce Hall deviations equal to
  or LARGER than real clicks** (145-216 vs 47-171), meaning the sensor is
  picking up mechanical shock/vibration jostling the ADC, not (only) true
  magnetic field changes. This makes both the open/close correlation theory
  AND the finger-snap noise-rejection filter unreliable as currently
  implemented. `HALL_ACTIVITY_THRESHOLD=30` in the firmware does NOT
  actually reject snaps - confirmed with real test data.

### 🤔 Unresolved / open questions
- Is there a way to isolate genuine magnetic signal from mechanical shock in
  the Hall reading? (E.g. some kind of differential timing signature, since
  a shock is instantaneous but a coil pulse might have a characteristic
  rise/decay shape - untested.)
- With the CORRECT solenoid now in tight contact, initial real data showed
  much longer durations (81-122ms) and much lower frequencies (262-1302Hz)
  than the wrong-solenoid data, with peaks near clipping (~24000+/32767) -
  worth checking for actual waveform clipping via a snippet capture, not
  yet done on the correct solenoid.
- For reliable magnetic detection on the StickC Plus, external hardware
  connected via the top GPIO header or the bottom Grove/I2C (Port A)
  connector is vastly superior to the built-in `hallRead()` sensor (see
  above: it's swamped by mechanical shock, not just magnetic field). An
  external magnetometer or dedicated Hall-effect breakout, mounted away
  from the shock path and read over I2C/GPIO, hasn't been tried yet.
  **Update:** an external Hall-effect sensor has been ordered. Hall-effect
  sensing as a concept is being **retained**, but the built-in
  `hallRead()` approach is **shelved** (not actively worked on) until a
  higher-precision external sensor is deployed to the test lab. Don't rip
  out the existing Hall research code (see item 4 below) in the meantime.

## Recommended next steps

1. Decide whether to keep chasing the Hall-sensor angle (user wants to,
   as of last message) or accept the timing-based classifier as the
   practical answer and move the project toward "production" (i.e. stop
   experimenting, ship the timing-based label as the real classification).
   **Status: shelved pending external Hall-effect sensor hardware (on
   order) — not abandoned.**
2. If continuing Hall-sensor work: need a fundamentally different approach
   to separate shock from magnetism (current min/max-deviation approach is
   confirmed insufficient). Once the external sensor arrives, prefer wiring
   it via GPIO header or Grove/I2C (Port A) rather than relying on the
   built-in `hallRead()`.
3. Consider whether raw snippet capture (`SNIPON`) should be re-enabled to
   check for clipping on the correct solenoid at close/tight contact.
4. Eventually: strip out whichever experimental code doesn't pan out
   (snippet capture + Hall research columns were explicitly designed to be
   removable once a decision is made - see comments in `ClickLogger.ino`
   top block). Keep the Hall-effect research code/columns in place for now
   since the approach is shelved, not rejected.

## Key files

- `ClickLogger/src/ClickLogger.ino` - all firmware logic, extensively
  commented (read the top-of-file doc comment for command reference and
  log format).
- `ClickLogger/src/secrets.h` - WiFi + push target config, gitignored (real
  values only on this machine).
- `push_receiver.py` - test HTTP receiver, background process on this Mac.
- `serial_cmd.py` - **use this for all serial commands** (STATUS/DUMP/
  CLEAR/THRESH/HALLON/etc.) instead of hand-writing pyserial snippets.
  Doesn't touch esptool/chip_id, so it never resets the board. E.g.:
  `python3 serial_cmd.py STATUS DUMP`, `python3 serial_cmd.py "THRESH 4500" HALLON CLEAR`,
  `python3 serial_cmd.py --listen 20` (just listen, no commands).
- `health_check.py` - serial connectivity check (NOTE: this resets the board
  via esptool chip_id! Don't use during an active data-collection run - use
  `serial_cmd.py` instead for non-disruptive serial reads.
- Ad-hoc test scripts used this session (in `/tmp`, NOT committed/permanent -
  recreate if needed): `few_samples.py` (3-cycle test), `short_loop.py`
  (10-cycle test), `confirm_sequence.py` (3-cycle with precise tap
  timestamps). All follow the same pattern: subprocess-call
  `adb-screencap.py --device 34181JEHN20500 --tap "Run Now"`, sleep ~26-32s
  between cycles (empirically enough for the ~5s BLE latency + 20s run +
  margin).

## Serial command reference (as of this writing)

```
DUMP              - print /clicks.csv
CLEAR             - erase clicks.csv AND snippets.bin, reset session counter
STATUS            - clickCount, thresholds, SPIFFS usage
THRESH <n>        - set THRESH_ON (THRESH_OFF becomes n/2)
SERVE / STOP      - manual WiFi+HTTP server on/off (5 min auto-timeout)
PUSHNOW           - trigger an auto-push cycle immediately
SNIPON / SNIPOFF  - raw PCM snippet capture to /snippets.bin (experimental)
HALLON / HALLOFF  - Hall-sensor research columns in CSV (experimental)
```

Front button: toggles WiFi/SERVE on/off (also acts as a ~17ms click-test
event itself - suppressed for 600ms after press so it doesn't get logged as
a real click).
