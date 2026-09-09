# ClickLogger Project Handoff

**Last updated**: 2026-09-09, end of day
**Purpose**: M5StickC Plus firmware that listens (mic) to a Rain Bird DC latching
solenoid valve, detects each open/close "click," and tries to determine
which click is which (open vs. close) using whatever sensing modality works.

---

## Current physical setup (IMPORTANT - don't lose this)

- **THREE solenoids now bundled together**, all on the same BAT-PRO-6 DC53
  controller, permanent paint-marker-labeled on the bench (do NOT relabel -
  see "numerological aesthetics" discussion, 2026-09-09: decided against
  relabeling or adding a 4th "D0" unit for now - current scheme is fine):
  - **D1** = Master Valve (MV1 in the app's Stations list) - fires
    alongside EVERY station that runs, confirmed 2026-09-09 (not just
    Station 001 as originally assumed).
  - **D2** = Station 001's own solenoid (the original "station valve").
  - **D3** = Station 002's own solenoid (newly added 2026-09-09).
  - The BAT-PRO-6 can handle up to 6 regular stations + the MV if a 4th+
    solenoid is added later.
- TMAG5273 sensor position unchanged from 2026-09-08 (between D1/D2's
  plunger openings) - **not yet re-optimized for the 3-solenoid case**;
  today's results (see below) suggest current position already
  differentiates all 3, but a repositioning experiment (mid-point among
  all 3, not just D1/D2) is still open for a future session.
- The M5StickC Plus mic was **repositioned mid-session on 2026-09-09**
  for better coupling (previous position was fine for D1/D2 but marginal
  for D3) - amplitudes measurably improved afterward (e.g. D3-related
  peaks went from ~16-19k to ~20-23k), though this did NOT fully resolve
  the acoustic reliability issues described below.

- Earlier in the session (before ~14:45 on 2026-08-18) we spent a long time
  testing with the **WRONG solenoid** (a different unit was rubber-banded to
  the sensor). All acoustic/positioning conclusions from that period are
  about the WRONG device and should be disregarded or re-verified before
  relying on them.
- Test irrigation controller: Rain Bird app (`com.rainbird.rainbird2dev`).
  **Two phones now used across sessions** - be sure to use the right
  `--device` serial with `adb-screencap.py`/`adb`:
  - Pixel 7a, serial `34181JEHN20500` - used in earlier sessions.
  - **Moto G (2025), serial `ZT4223C3PL`** - used in the 2026-09-08 session;
    its Rain Bird account shows a DIFFERENT device list (ESP-2Wire/ESP-ME3
    controllers) than the Pixel's - the "BAT-PRO-6 DC53 BLE" device (our
    actual test rig) is near the top of its device list, `a Site 01`,
    6 Stations, BLE. Its Program 1 / Station 001 is the 20-second manual-run
    program used for all testing.
  - **The device enforces a 7-second MINIMUM station runtime** ("Runtimes
    less than 7 seconds are not supported... to ensure the valve has time
    to respond to the command") - useful to know if a future session wants
    faster test cycling than the current 20s program.
- Automation flow used this session (reproducible tap sequence from the
  BAT-PRO-6 DC53 BLE device's main screen, Moto G, `adb-screencap.py
  --device ZT4223C3PL`):
  ```
  tap 548 1344   # "Manual Water" button
  tap 360 443    # "Program Run" option
  tap 360 315    # select "Program 1"
  tap 360 1450   # "Start Program" button
  ```
  Then wait ~28-30s for the full cycle (open burst + ~20s dwell + close
  burst) before navigating back (tap ~48,118 = back arrow) to the device
  screen for the next cycle. BLE sometimes needs ~4s to reconnect after
  navigating back into the device from Home - screenshot-check before
  re-tapping "Manual Water" if a tap sequence seems to silently no-op.
- Automation script: `~/git/monorepo/utils/adb-screencap.py` (this went
  missing once mid-session in an earlier week, unrelated to this project,
  then was restored by the user - if it disappears again, that's on the
  monorepo side, not ours).

## Current firmware state (volatile - resets on every reboot!)

`THRESH_ON`/`THRESH_OFF`, `SNIPON`/`SNIPOFF`, `HALLON`/`HALLOFF`,
`TMAGON`/`TMAGOFF`, `TMAGSTREAM`/`TMAGSTREAMOFF`, and the **TMAGCAL
calibration itself** are all **RAM-only** and reset to compiled-in defaults
(or "not calibrated") on every reboot/reflash. As of last check (end of
2026-09-09 session):
- `THRESH_ON=7000` `THRESH_OFF=3500` (raised from 4500 on 2026-09-09 after
  two low-amplitude/long-duration/low-frequency false triggers - amplitude
  ~5000-6000, duration ~65ms, freq ~120-185Hz - got swept into burst
  labels as if real clicks; 7000 gives solid margin above those while
  staying safely below every genuine click seen so far, which range
  ~9800-23600). A diagnostic drop to `THRESH_ON=2000` was also tried
  (see below) - made things WORSE (more missed events, likely ambient
  noise confusing the detector), reverted; don't go below ~7000 without
  a real reason.
- `TMAGON` active (6-column x/y/z min/max CSV logging)
- **Firmware was reflashed at the very end of the session** with the
  acoustic-simplification rework (see "Update 2026-09-09" below,
  item 6) - this wipes all RAM-only state, so as of the actual end of
  session the board is: `THRESH_ON=7000`, `TMAGON` on, log cleared,
  **TMAG NOT calibrated** ("run TMAGCAL"). Running a fresh `TMAGCAL`
  against this new firmware is the actual next task, not "mid-rebuild" -
  no old calibration survives a reflash regardless.
- `HALLON`/A3144E - still unplugged/unused, same as before (superseded by
  TMAG5273).
- Snippet mode (`SNIPON`) - not re-enabled this session; see "Recommended
  next steps" re: likely removal now that acoustic precision is being
  deprioritized entirely.

**After any reflash, re-run** (adjust TMAGCAL's expected classes/labels if
the rig has changed):
```
THRESH 7000
TMAGON
CLEAR
CYCLESTART        (NEW 2026-09-09 - see burst_label fix below; run this
                  once right before EVERY repeated full test cycle, not
                  just once per session, or labels can misalign)
TMAGCAL 8        (or larger, e.g. 16-24, for a sturdier no-correlation threshold)
(then run enough full open/close cycles to satisfy it - watch STATUS)
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
  actually reject snaps - confirmed with real test data. **Update
  (2026-09-04): this code has since been fully removed from the firmware**
  (superseded by the external Hall Effect Unit - see below).

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
  **Update (2026-09-04):** the external sensor has arrived and is now
  plugged into the Grove connector (Port A) on the M5StickC Plus. It's the
  **M5Stack Hall Effect Unit, SKU U084** (3× A3144E Hall switches + a
  74HC08D AND gate, HY2.0-4P Grove cable, includes a test magnet). This is
  a **digital, active-LOW switch output**, not analog/I2C - it pulls the
  Grove data line LOW when an S-pole approaches the front face (or N-pole
  the back face) and goes HIGH otherwise, with an onboard status LED.
  Because it's a threshold switch rather than a magnitude/polarity ADC
  reading, it can't report field strength or discriminate "positive" vs
  "negative" field the way `hallRead()` conceptually could - it only fires
  on one specific pole direction per face. This is actually still useful
  for the open/close theory (a coil-polarity-reversal event might trip the
  switch on only one of the two click types), but needs real-world
  testing to see whether the module's dual-face/3-sensor design ends up
  triggering on both directions in practice. Hall-effect sensing as a
  concept is being **reactivated** with this external unit.
  **Update (2026-09-04, later same day):** the built-in ESP32 `hallRead()`
  approach has now been **fully removed** from the firmware (not just
  shelved) - all `hallBaseline`/`hallMinEvent`/`hallMaxEvent` tracking, the
  `HALL_ACTIVITY_THRESHOLD` deviation-based finger-snap/noise rejection
  filter, and the 3-column `hall_baseline,hall_min,hall_max` CSV output are
  gone. `ClickLogger.ino` now reads the external Grove Hall Effect Unit
  instead: a digital `digitalRead()` on `PIN_HALL_DATA` (GPIO33, Grove Port
  A), sampled once at event start and on every chunk during the ACTIVE
  state, latching a single `hallTriggeredEvent` bool if it ever goes LOW
  during the click. `HALLON` now logs one CSV column, `hall_triggered`
  (0/1), instead of the old three. The on-screen per-event indicator shows
  `H` (triggered) or `-` (not triggered) in place of the old signed
  deviation number. NOTE: removing the old noise-rejection filter means
  there is currently **no** finger-snap/noise rejection at all (the old one
  was confirmed unreliable anyway) - a habitual smoke-test snap will get
  logged as a real click again until/unless a new rejection scheme is
  built on top of the external switch. Compiles clean (`pio run`); **not
  yet flashed/tested on real hardware** - next session should reflash,
  verify with the included test magnet (touch it to the unit and confirm
  `H` appears + red LED lights), then re-run real solenoid clicks and check
  `hall_triggered` in `/clicks.csv` against the timing-based ground truth.
  **Update (2026-09-08):** flashed and bench-tested with the included
  magnet. First attempt used the wrong Port A data pin (GPIO32) - the
  sensor's own LED lit up correctly when the magnet was applied, but
  `hall_triggered` stayed `0` in every logged click. Cross-checked against
  M5Stack's own `LIMIT`/`PIR` Grove-unit example sketches (bundled with the
  M5StickCPlus library), which both use **GPIO33** for Port A's digital
  signal pin, not GPIO32 - firmware corrected accordingly (`PIN_HALL_DATA`
  is now 33) and reflashed. Retest confirmed working: `hall_triggered=1`
  with the magnet held against the unit, `0` with it removed, across many
  logged events.
  **Update (2026-09-08, later):** while bench-testing, found a real
  positioning result on the actual solenoid (not just the test magnet): a
  specific spot/orientation for the A3144E made its LED flicker on the
  solenoid's OFF click but NOT its ON click - the first evidence of ANY
  magnetic-domain signal correlating with a specific click type on this
  hardware. However, this is a purely transient/transition signal - it
  doesn't address a separate, real gap: there's currently no way to sense
  the valve's RESTING state (open vs closed) independent of catching a
  click event. Discussed whether this actually matters (see "state
  tracking" note below) and, wanting more sensing resolution than the
  A3144E's single on/off threshold, ordered/received a **TMAG5273** 3-axis
  I2C magnetometer breakout (SDA=GPIO32, SCL=GPIO33 confirmed via I2C scan
  finding it at the default address 0x35, matching M5Stack's own
  ACCEL_ADXL345/TVOC_SGP30 example sketches for Grove Port A's I2C pin
  convention). NOTE: the A3144E and TMAG5273 share the same 2 physical
  Grove Port A pins - swap in/out one at a time, power off first (don't
  hot-swap - see reasoning in session transcript).
  **Update (2026-09-08, later still):** TMAG5273 support added to
  `ClickLogger.ino` as an ADDITIONAL/alternate sensing path - the A3144E
  digital-switch code (`PIN_HALL_DATA`/`HALLON`/`HALLOFF`) was
  **explicitly kept in place as a fallback**, per direct instruction, not
  removed. New serial commands: `TMAGON`/`TMAGOFF` (adds 6 CSV columns per
  click event: `x_min,x_max,y_min,y_max,z_min,z_max` in mT) and
  `TMAGSTREAM`/`TMAGSTREAMOFF` (continuously prints live X/Y/Z to Serial
  every ~150ms, independent of click detection - meant for the bench
  positioning search, the numeric equivalent of watching the A3144E's LED).
  Driver implemented directly against TI's register map (verified via
  SparkFun's open-source Arduino library source, not guessed): continuous
  XYZ+temperature measurement mode, ±40mT range (default), manufacturer-ID
  check (`0x5449`, "TI") gates whether `tmagOk` gets set at all - if the ID
  check fails, all TMAG reads are treated as unavailable rather than
  silently logging garbage. Bench-verified: ambient baseline reads a stable
  ~(-0.2, 0, -0.4) mT via `TMAGSTREAM`, consistent with Earth's field plus
  local metal - sane, working sensor. **Not yet tested against the actual
  solenoid** - that's the immediate next step (see below).
  **Update (2026-09-08, later still again):** the specific board in hand is
  an **Adafruit TMAG5273 (A2) breakout, product 6490** (±133mT/±266mT
  range variant, STEMMA QT/Grove) - the user also has several **A1**
  boards (product 6489, ±40mT/±80mT) on hand for A/B comparison, plus a
  spare StickC-Plus SE for hardware-level A/B testing. This matters: same
  16-bit ADC resolution is spread across a wider range on the A2, so it's
  ~3.3x coarser per-LSB than the A1 for weak fields. Bench baseline
  readings here are tiny (sub-mT to ~1.5mT) - well within even the A1's
  narrower range - so **the A1 variant is likely the better choice for
  this specific weak-signal application** and worth trying once bring-up
  work allows.
  Found and fixed a real bug from this: the firmware's initial
  implementation hardcoded the A1's ±40mT range assumption, which would
  have silently under-reported this A2 board's true field strength by
  ~3.3x (right shape, wrong absolute scale) without any error - not a
  crash, just quietly wrong numbers, so it wouldn't have been obvious
  without cross-checking. Fixed by auto-detecting the variant from the
  DEVICE_ID register (bits[1:0]: 1=A1, 2=A2 - confirmed against both
  Sparkfun's and Adafruit's own open-source TMAG5273 driver source, not
  guessed) and setting the active mT-per-LSB scale accordingly at boot
  (`tmagRangeMt`, no longer a hardcoded constant). Verified after the fix:
  ambient baseline now reads correctly as ~(-0.7, 0, -1.4) mT (previously
  mis-scaled to ~(-0.2, 0, -0.4) mT under the wrong 40mT assumption) -
  ~3.3x ratio confirms the fix is working as intended. This firmware will
  now correctly auto-scale if an A1 board ever gets swapped in instead, no
  code change needed.
  **Update (2026-09-08, latest):** added a live on-screen positioning
  display (`TMAGSTREAM`/`TMAGSTREAMOFF`), so bench positioning experiments
  don't require a tethered laptop/serial monitor - the Stick's own screen
  shows "TMAG LIVE" plus 3 rows (X/Y/Z), each with a live numeric readout
  and a bipolar bar graph (green=positive, orange=negative, centered at
  zero, scaled to the sensor's active +/-range). Bench-verified working
  on real hardware (visually confirmed by the user). NOTE: if a real click
  event fires while this view is active, it'll briefly flash back to the
  normal click-history view (`logClickEvent()`'s own screen redraw) before
  the next periodic TMAG update restores the live view - not worth
  avoiding for a bench tool. **Still not yet tested against the actual
  solenoid** - that remains the next real step.

  **Update (2026-09-08, later - major session): tested against the actual
  solenoid, with big results.** Several discoveries in sequence:

  1. **A second solenoid was added to the same controller** (a "regular"/
     station valve, wired alongside the original "master valve" solenoid
     already under test) - see physical setup at top of this doc. This
     turned into a much richer experiment than originally planned.

  2. **Master valve vs station valve fire in a STAGGERED sequence, not
     simultaneously** - discovered via the mic's acoustic click timestamps
     (both solenoids audible on the one mic since physically close
     together): opening a program fires the **master valve first, then
     ~2.0s later the station valve**; closing does the REVERSE order -
     **station valve first, then ~1.0s later the master valve**. Plausible
     explanation: this controller is battery-powered with limited
     capacitor-based drive current, so it can't fire two latching
     solenoids simultaneously - has to stagger them to let capacitors
     recharge between actuations. (Sensible design either way: master
     valve opens first to build line pressure before the zone valve opens;
     zone valve closes first to cut flow before the master valve shuts off
     supply.)

  3. **Fixed a real timestamp-precision bug this revealed**: the CSV's ISO
     timestamp only has whole-SECOND resolution (from the RTC), so
     measuring a ~1-2s inter-click gap by diffing consecutive rows'
     timestamps is only accurate to +/-1s - not good enough to trust
     "1.0s" vs "2.0s" without corroboration. Fixed by exposing the
     firmware's already-existing millisecond-precision internal gap
     calculation (`millis()`-based, previously only used internally for
     the timing prediction) as a new base CSV column, `gap_ms` (or "NA" for
     the first event since boot). Use this column, not timestamp deltas,
     for any future short-gap timing analysis.

  4. **Added burst grouping** to correctly label BOTH solenoids' events
     (the original per-event timing-based '1'/'0' prediction, using a
     single ~25s threshold, mislabels the SECOND event of a staggered pair
     as "close" even when it's actually the second half of an "open"
     transition - it only looked at the gap since the immediately-prior
     event, not burst membership). New `BURST_GAP_MS=5000` constant
     groups events firing within 5s of each other into one "burst" (the
     burst's own open/close type is decided once, at the burst's first
     event, using the original 25s-threshold logic; subsequent same-burst
     events inherit that type rather than re-deriving it from their own
     much-shorter gap). Exposed as a new base CSV column, `burst_label`
     (format `<O|C><0-based position>`, e.g. "O0"=master-open, "O1"=
     station-open, "C0"=station-close, "C1"=master-close) - purely
     timing-derived, meaningful even without any TMAG sensor attached, and
     generalizes to any number of staggered solenoids with zero code
     changes (a single-valve rig always produces just "O0"/"C0").

  5. **Repositioned the TMAG5273 physically between the two solenoids'
     plunger openings** (previously mounted near/on the master valve
     only) - at the user's suggestion, explicitly to avoid hardcoding
     "the signal is on the Z axis" and instead lean into using all 3 axes
     to potentially distinguish BOTH solenoids' states from one sensor.
     This worked spectacularly: with the sensor at this position, the
     4 distinct burst-label classes (O0/O1/C0/C1) separate CLEANLY across
     all 3 axes with zero overlap across 3 repeated test cycles - Y
     roughly separates open-bursts (~0-1.6mT) from close-bursts
     (~3.5-4.4mT), while X and Z sign further separate which valve fired
     within the burst. See exact per-class means in "Current firmware
     state" above.

  6. **Generalized `TMAGCAL` from a binary open/close classifier into an
     N-class nearest-centroid classifier**, keyed by `burst_label` instead
     of the old single-bit timing prediction - this is what let the same
     command/architecture handle 4 classes (2-solenoid rig) with zero
     hardcoded assumptions about axis OR number of solenoids; a
     single-valve rig naturally degenerates back to exactly 2 classes
     ("O0"/"C0"), the original binary case. Default calibration sample
     count raised from 4 to 8 events (~2 full cycles on a 2-solenoid rig).
     `STATUS` now prints all calibrated classes with sample count + mean
     vector.

  7. **Added "no-correlation" flagging** (`TMAG_NO_CORRELATION = "NC"`) -
     per user's explicit preference, this does NOT reject/discard events
     (unlike the old removed `hallRead()`-based rejection filter) - every
     event still gets fully logged/counted/displayed, just flagged if its
     magnetic vector doesn't land close to ANY calibrated class (a real
     solenoid actuation should always resemble one of the calibrated
     clusters; a finger-snap or other spurious acoustic trigger, lacking
     any accompanying magnetic deviation, should sit near ambient baseline
     instead, far from every real cluster). The distance threshold
     (`tmagRejectThresholdSq`) is grounded in real data: `TMAG_REJECT_MARGIN`
     (2.5x) times the largest intra-class spread actually observed during
     calibration - not a guessed absolute number like the old filter.
     **Validated working, both directions, same session**: a full
     post-calibration solenoid cycle classified all 4 events correctly
     (zero false "NC" flags), and two deliberate test finger-snaps both
     got correctly flagged "NC" (vectors near ambient baseline, nowhere
     near any real click cluster).

  8. **On-screen history line updated**: swapped the acoustic frequency
     field for `burst_label()` (per user request - "audio data is no
     longer as interesting" now that the TMAG classifier works this
     well). Line now reads `<dur> <indicator> <burst_label> <hallBuf>`.

  9. **A1 vs A2 sensitivity discussion, resolved for now**: given the
     rock-solid, wide-margin separation achieved at this sensor position
     (see class means above - none anywhere near the A2's ±133mT range
     limit, huge gaps between clusters relative to intra-cluster jitter),
     **decided NOT to order A1 boards** - sensitivity isn't the bottleneck
     here. Revisit only if a future experiment (e.g. much weaker/farther
     signals) hits a resolution wall with the A2.

  **Not yet done** (as of 2026-09-08): broader-sample recalibration
  (`TMAGCAL 16`+) for a sturdier no-correlation threshold; testing the
  adjacent-solenoid acoustic/magnetic interference scenario (see next
  steps); further resting-state (static open vs. closed, no click)
  testing at this new mid-position - the earlier resting-state test was
  done at the OLD single-solenoid sensor position and hasn't been
  repeated here.

  **Update (2026-09-09): 3rd solenoid added (D3/Station 002); found and
  fixed a real burst-label collision bug; confirmed TMAG differentiates
  all 3 solenoids; found a new, unresolved acoustic reliability gap;
  decided to deprioritize acoustic precision going forward.**

  1. **A 3rd solenoid (D3) was added on a separate station (Station 002)**
     - not wired in parallel with D1/D2's Station 001 output. First
       attempt to test it failed silently: adding Station 002 to a
       program in the Rain Bird app via one phone didn't actually sync to
       the physical controller before running - a BLE app sync bug
       (separate from anything in this firmware), worked around by
       reconnecting/re-adding from a second phone.

  2. **Real bug found: `burst_label` position-reset caused label
     collisions on multi-station rigs.** The 2026-09-08 burst-grouping
     design reset `currentBurstPosition` to 0 on every new burst (gap >
     `BURST_GAP_MS`=5000ms) and derived that burst's open/close type from
     comparing the gap against `TIMING_GAP_THRESHOLD_MS`=25000ms. On a
     2-station chained program run, the real inter-station transition gap
     turned out to be **~18s** - long enough to start a new burst, but
     short enough (under the 25s threshold) to get mislabeled as a
     continuation of the previous burst's type. Result: Station 002's
     events got the exact same labels (`C0`/`C1`) already used for
     Station 001's own valves, silently averaging two physically
     different solenoids' magnetic signatures into one `TMAGCAL` class.
     **Fixed properly** (not just a threshold tweak, since the fragile
     "is this gap long enough to mean X" comparison was the root problem
     and would only resurface with more solenoids/longer chains):
     - Burst TYPE (open vs close) is now decided by **strict
       alternation** (first burst since the last reset = open, then
       flips every new burst) instead of comparing against a time
       threshold - robust to any inter-station transition duration.
     - `burst_label` now embeds a **monotonically-increasing burst-
       sequence index** that only resets on `CLEAR` or the new
       `CYCLESTART` command (see below), so different stations' bursts
       can never collide regardless of program size or timing. Format
       changed from `<O|C><position>` (e.g. `O0`) to
       `<O|C><burstSeq>_<position>` (e.g. `O0_0`, `O0_1`, `C1_0`, `C1_1`,
       `O2_0`, `C3_0`...).
     - New serial command **`CYCLESTART`**: resets burst numbering/
       alternation WITHOUT touching the log file (unlike `CLEAR`, which
       now also calls the same reset). Run this once right before each
       repeated full test cycle so the same burst-seq numbers - and
       hence the same labels - recur cycle to cycle (required for
       `TMAGCAL` to average multiple samples of the same real-world
       class together).
     - See `ClickLogger.ino`'s updated top-of-file doc comment
       (`burst_label` section) for the full writeup.

  3. **Confirmed: TMAG DOES differentiate all 3 solenoids.** Isolated
     single-station test data (Station 002/D3 run alone, clean capture):
     ```
     D3 opening (paired with D1 master): mean=(0.81, -1.37, 0.13)
     D3 closing (paired with D1 master): mean=(0.07,  1.72, 2.05)
     ```
     compared against D2's signatures from Station 001 (2026-09-08 data):
     ```
     D2 opening: mean=(-0.59, 0.36, 0.78)
     D2 closing: mean=( 1.49, 0.52, 1.53)
     ```
     Clearly separated on multiple axes - this directly answers the
     original "can one TMAG5273 differentiate 3 bundled solenoids"
     question: **yes**, at least at the current sensor position, without
     needing to reposition per-solenoid.

  4. **New, unresolved acoustic reliability gap in CHAINED (multi-station)
     runs specifically.** Across ~5 chained (Station 001 -> Station 002)
     test cycles, at least one expected click event went undetected
     (never crossed `THRESH_ON` at all - not logged-then-rejected, a
     total miss) in every single cycle - but which specific event went
     missing was NOT consistent: sometimes D3's closing click, sometimes
     D2's own opening click, sometimes the whole D3-opening burst. The
     SAME events were captured cleanly and consistently when D3 was run
     in ISOLATION (Station 002 alone, no chaining). Ruled out /
     investigated this session:
     - **Mic positioning**: repositioned mid-session for better general
       coupling (see physical setup above) - measurably raised
       amplitudes but did NOT fix the chained-run misses.
     - **Lowering `THRESH_ON`** to 2000 (near noise floor) as a
       diagnostic: made things WORSE (more distinct events missed than
       before), suggesting ambient noise/hum confuses the detector at
       low thresholds rather than revealing weak-but-real signals -
       reverted to 7000.
     - **Valve actually staying open/not closing on schedule**: seemed
       possible early on (a delayed-but-real click was once logged
       ~9.5 minutes after its expected time), but the user directly,
       physically confirmed (felt/heard) D3 close on schedule during a
       chained run where the log showed nothing at all - so this is
       almost certainly a **pure detection gap, not a real valve/mechanical
       problem**.
     - **Not yet tried**: raw waveform inspection via `SNIPON` during a
       chained run (would require `capturingSnippet` to trigger, which
       needs `THRESH_ON` to fire first - so this only helps for events
       that DO cross threshold weakly, not total misses); direct human
       observation timed precisely against the log in real-time for
       multiple chained cycles in a row to build a event-by-event pattern
       (only tried once this session).
     - **Working theory, not yet confirmed**: something specific to
       running two stations back-to-back in one BLE-triggered program
       (as opposed to two independently-triggered manual runs) may
       intermittently affect acoustic transmission or timing in a way
       that doesn't reduce to a single root cause easily diagnosable
       remotely - possibly worth a fresh physical inspection (connector
       flex, mounting tightness, mic cable strain) next session.

  5. **Decision (2026-09-09, end of session): deprioritize acoustic
     PRECISION entirely going forward.** Given the above reliability gap,
     and the already-long-standing finding that acoustic features
     (duration, frequency, amplitude-based classification) have never
     reliably distinguished open/close (see "Ruled out" section above),
     the plan for next session is to stop trying to extract precise
     acoustic features and instead treat ANY click/snap/pop/tap-like
     transient (i.e. just a peak crossing `THRESH_ON`) as nothing more
     than a **trigger to go sample the TMAG5273** - full classification
     authority shifts to the TMAG nearest-centroid classifier (already
     implemented) and its existing no-correlation ("NC") flagging (to
     reject spurious non-solenoid triggers like finger snaps). This is
     a natural extension of the no-correlation design's own philosophy
     (already log-everything, TMAG-decides), just leaning into it fully
     rather than half-relying on acoustic duration/frequency as
     meaningful signal. **IMPLEMENTED and flashed same session** (see
     below) - no longer a next-step.

  6. **Acoustic-simplification implemented (2026-09-09, same session,
     after the decision above).** Removed entirely: the zero-crossing-
     rate frequency estimate (`est_freq_hz` CSV column, `readMicChunk()`'s
     ZC counting), and the duration-based acoustic classification digit
     (`classifyClick()`/`CLICK_CLASSIFY_MS`). `readMicChunk()` now just
     returns peak amplitude - pure trigger, no signal analysis.
     **CSV format changed** (5th time this project's format has evolved -
     see `ClickLogger.ino` top-of-file doc comment for the authoritative
     version, always cross-check there, not just here):
     ```
     <ISO8601 timestamp>,<duration_ms>,<peak_amplitude>,<classification>,<gap_ms>,<burst_label>[,x_min,x_max,y_min,y_max,z_min,z_max if TMAGON]
     ```
     `classification` is now just `<timing digit>` + optional TMAG class
     (e.g. `"0O0_0"`, `"1NC"`) - no more 2nd acoustic-guess digit.
     `duration_ms`/`peak_amplitude` are still logged, but purely as raw
     diagnostic context now, not decision inputs. Compiles clean
     (`pio run`), reflashed, board reconfigured
     (`THRESH_ON=7000`/`TMAGON`/`CLEAR`'d), verified booting + reading the
     TMAG5273 correctly. **No new TMAGCAL calibration was run/saved
     against this new firmware before the session ended** - do that first
     next session (see item 1 below).

  7. **Important clarification surfaced in end-of-session discussion,
     worth remembering**: everything calibrated/tested so far (both
     2026-09-08 and 2026-09-09) is **transient click-event classification
     only** - the TMAG vector captured during the brief moment a solenoid
     actuates. This is EMPIRICALLY GOOD at the current sensor position for
     these 3 solenoids bundled this close together, but:
     - It is **not a physical guarantee** that generalizes to other
       geometries (e.g. the HQ-style straight-line spaced-out test
       mentioned as a future goal) - would need re-verification, possibly
       re-positioning, at that point.
     - It does **NOT** demonstrate **static/resting-state** discernment
       (i.e. reading whether a given valve is currently open or closed
       from a steady-state field reading, independent of catching a
       click) - that remains a distinct, still-untested question (see
       item 4 in "Recommended next steps" below, carried over from
       2026-09-08).
     - The classifier has **zero cold-start/from-first-principles
       capability** - it's a pure empirical nearest-centroid model with no
       built-in physics. It cannot classify anything meaningful until
       `TMAGCAL` has been run against real labeled events from that exact
       sensor position/solenoid configuration. Move the sensor, add a
       solenoid, or change the rig, and recalibration from scratch is
       required - there is no way around this with the current approach.

## Git state (IMPORTANT - nothing committed yet as of end of 2026-09-09)

Both `firmware/m5stack/ClickLogger/src/ClickLogger.ino` (burst-label fix +
`CYCLESTART` command + acoustic-simplification, combined - two logically
separate changes, never split into separate commits) and this
`HANDOFF.md` are **modified but uncommitted** on the `iot-feasibility`
branch as of end of session. Decide next session whether to commit (and
whether to split into 2 commits: burst-label fix, then acoustic-
simplification) before doing more firmware work on top, to avoid a messy
combined diff.

## Recommended next steps

0. ~~Implement the acoustic-simplification rework~~ **DONE this session**
   (see item 6 above) - flashed and verified booting; just needs fresh
   `TMAGCAL` data against the new firmware (item 1 below is now the real
   top priority).
1. **(TOP PRIORITY) Run a fresh `TMAGCAL`** - no production calibration
   was saved at the end of the 2026-09-09 session (repeatedly reset while
   debugging, and the acoustic-simplification reflash wipes RAM-only
   calibration state anyway). Board is currently freshly flashed +
   reconfigured (`THRESH_ON=7000`, `TMAGON`, log cleared) but
   **NOT calibrated**. Run `CYCLESTART` before EVERY repeated cycle (not
   just once per session - see burst_label fix above) and `TMAGCAL 16` or
   more (4+ full cycles) for a sturdier no-correlation threshold across
   all 3 solenoids. Keep hands off the rig entirely once `TMAGCAL` is
   armed - accidental handling/finger-snaps during collection will
   corrupt both the burst timing AND the calibration means.
2. **Investigate the chained-run acoustic reliability gap** (see
   2026-09-09 update above) if it still matters once acoustic precision
   is deprioritized - it may become moot if the plan is just "any peak
   triggers a TMAG sample" and missed triggers are rare/tolerable, but
   worth a fresh physical inspection (connector flex, mounting tightness)
   if it keeps happening.
3. **Test the adjacent-solenoid interference scenario** the user flagged
   earlier (relevant to "other labs have clamped arrays" - see prior
   session notes): does a DIFFERENT nearby solenoid actuating (not one of
   the ones on this rig) cause false acoustic triggers or corrupt the TMAG
   classification? Not yet tested.
4. **Repeat the resting-state (static open vs. closed) test** at the
   current mid-position sensor placement, now with 3 solenoids - might
   reveal a richer "current state of ALL valves" static signal, not just
   click-transition detection.
5. Decide whether/when to move the A3144E digital-switch code path back
   into active use, or remove it - TMAG5273 has clearly superseded it for
   this project's purposes as of 2026-09-09's 3-solenoid confirmation.
6. **Likely ripe for removal now**, given the 2026-09-09 decision to
   deprioritize acoustic precision entirely: raw snippet capture
   (`SNIPON`), the acoustic duration-based classification digit, and
   possibly the A3144E path. Not urgent - nothing is currently broken by
   keeping it all in place, but worth a cleanup pass once the acoustic-
   simplification rework (item 0) is done and settled.
7. Consider a TMAG5273 repositioning experiment now that there are 3
   solenoids (current position was optimized for D1/D2 only on
   2026-09-08) - today's data suggests it already works for D3 too, but a
   true "centered among all 3" position hasn't been tried.

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
                    AND burst-cycle numbering (see CYCLESTART)
CYCLESTART        - reset burst-cycle numbering only, log file untouched.
                    Run once right before EVERY repeated full test cycle
                    (see burst_label fix, 2026-09-09) so labels line up
                    cycle to cycle - required for TMAGCAL to average
                    multiple samples of the same real-world class.
STATUS            - clickCount, thresholds, SPIFFS usage, TMAG calibration
                    state (classes + means + no-correlation threshold, or
                    "calibrating"/"not calibrated")
THRESH <n>        - set THRESH_ON (THRESH_OFF becomes n/2)
SERVE / STOP      - manual WiFi+HTTP server on/off (5 min auto-timeout)
PUSHNOW           - trigger an auto-push cycle immediately
SNIPON / SNIPOFF  - raw PCM snippet capture to /snippets.bin (experimental)
HALLON / HALLOFF  - A3144E Hall-sensor digital-switch column in CSV
                    (experimental; sensor currently unplugged - see above)
TMAGON / TMAGOFF  - adds 6-column x/y/z min/max to CSV per click event
TMAGSTREAM / TMAGSTREAMOFF - live X/Y/Z on Serial AND the Stick's own
                    screen (bar graph), for untethered bench positioning
TMAGCAL <n>       - auto-calibrate the N-class nearest-centroid classifier
                    from the next <n> click events (default 8), using
                    burst_label as ground truth. Also sets the
                    no-correlation ("NC") flagging threshold - see
                    ClickLogger.ino's top comment block for full details.
```

Front button: toggles WiFi/SERVE on/off (also acts as a ~17ms click-test
event itself - suppressed for 600ms after press so it doesn't get logged as
a real click).
