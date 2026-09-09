// ClickLogger.ino
//
// Detects short acoustic "click" events (a DC latching solenoid opening or
// closing) using the M5StickC Plus's onboard PDM microphone, timestamps each
// event with the onboard battery-backed RTC, and appends a CSV log entry to
// internal flash (SPIFFS) so the board can log events completely untethered.
//
// On first boot (or whenever WiFi credentials are present and reachable) it
// syncs the RTC once via NTP, then turns WiFi off entirely and runs the
// detector loop indefinitely.
//
// Log format (one line per click event), appended to /clicks.csv:
//   <ISO8601 timestamp>,<duration_ms>,<peak_amplitude>,<classification>,<gap_ms>,<burst_label>
//
// IMPORTANT (2026-09-09): acoustic PRECISION is no longer trusted for
// classification - duration, frequency, and every other acoustic feature
// tried have never reliably distinguished open vs. close (see HANDOFF.md's
// "ruled out" section, and the 2026-09-09 decision to fully deprioritize
// this). The amplitude-threshold peak detector is kept purely as a
// TRIGGER ("something clicked/snapped/popped/tapped - go sample the
// TMAG5273 now"); duration_ms/peak_amplitude are logged only as raw
// diagnostic context, not as classification input. The old zero-crossing-
// rate frequency estimate and duration-based acoustic guess digit have
// been removed entirely - full classification authority now belongs to
// the TMAG5273 nearest-centroid classifier (see TMAGCAL) and its
// no-correlation ("NC") flagging, which already existed for exactly this
// purpose (reject spurious non-solenoid triggers like finger snaps).
//
// gap_ms is the millisecond-precision time since the previous click event
// (via millis() - the same value the timing-based prediction is computed
// from), or "NA" for the first event since boot. Use THIS, not deltas
// between consecutive rows' ISO timestamps, when measuring short
// inter-click gaps (e.g. staggered master-valve/station-valve firing) -
// the ISO timestamp only has whole-SECOND resolution (from the RTC), so
// timestamp-delta math is only accurate to +/-1s and can't reliably tell a
// 1-second gap from a 2-second one.
//
// burst_label groups events that fired close together (<BURST_GAP_MS
// apart) as one "burst" - e.g. a master valve + station valve on the same
// controller firing ~1-2s apart rather than simultaneously. Format is
// "<O|C><burstSeq>_<position>": 'O'/'C' = this burst is an opening or
// closing transition; burstSeq is a monotonically-increasing count of
// bursts seen since the last CYCLESTART/CLEAR (0, 1, 2, ...); position is
// the 0-based order of this event WITHIN its burst (on the master+station
// rig tested so far: opening bursts go master(0) then station(1); closing
// bursts go station(0) then master(1) - see HANDOFF.md).
//
// IMPORTANT (multi-station fix, 2026-09-09): burst TYPE (open vs close) is
// decided by strict alternation - the first burst since the last
// CYCLESTART/CLEAR is always "open", then it flips every new burst - NOT
// by comparing the gap since the previous event against a fixed time
// threshold. This was a real bug on a 3-solenoid rig where two stations
// ran back-to-back in one program (station A, then station B): the ~18s
// controller transition between stations was long enough to start a new
// burst but NOT long enough to cross the old open/close gap threshold, so
// station B's events got mislabeled as a second "closing" burst and their
// position (0, 1, ...) collided with station A's own labels, silently
// corrupting TMAGCAL. Embedding a ever-incrementing burstSeq in the label
// (rather than resetting position to 0 on every new burst) makes
// collisions impossible regardless of how many stations/solenoids are
// chained in one program or how long each inter-station transition is.
// Run CYCLESTART (or CLEAR) once before each repeated full test cycle so
// the SAME burstSeq numbers - and hence the SAME labels - recur cycle to
// cycle, which TMAGCAL depends on to average samples of the same class.
// A single-valve rig still produces exactly "O0_0"/"C1_0" per cycle.
// Purely timing-derived - meaningful even without a TMAG5273 attached,
// and used as the ground-truth label for TMAGCAL when one is.
//
// classification is a diagnostic indicator "<timing>" with an optional
// variable-length TMAG classification field appended once a TMAG5273
// calibration has completed (see TMAGCAL):
//   - timing digit: '1' if this click started >TIMING_GAP_THRESHOLD_MS after
//     the previous one (predicted START of cycle/valve opening - valve is
//     normally-closed), '0' otherwise (predicted END of cycle/valve
//     returning closed). This has been 100% consistent across every
//     physical setup tested so far.
//   - tmag field (only present once TMAGCAL has completed): the TMAG5273's
//     own classification from its auto-calibrated nearest-centroid
//     classifier (see TMAGCAL) - e.g. "O0_0"/"O0_1"/"C1_0"/"C1_1" on a
//     staggered multi-solenoid rig, or just "O0_0"/"C1_0" on a
//     single-valve rig. A second, physically-independent check against
//     the timing/burst prediction - this is the field that's actually
//     trustworthy for classification (see 2026-09-09 decision above).
//     Can also read "NC" ("No Correlation") - this event's magnetic
//     signature didn't resemble ANY calibrated class closely enough,
//     which a real solenoid actuation should always do but a finger-snap/
//     other spurious acoustic trigger (no accompanying magnetic
//     deviation) would not. NOT rejected/discarded - still fully
//     logged/counted/displayed, just flagged, so nothing is silently
//     thrown away.
//   e.g. "0" = timing predicts open, TMAG not yet calibrated; "0O0_0" =
//   timing predicts open, TMAG classifies it as the first event of
//   opening burst 0 too; "1NC" = timing predicts close, but TMAG says
//   this event doesn't resemble any calibrated class (likely a spurious
//   non-solenoid trigger, e.g. a finger snap).
//
// Serial commands (type into the serial monitor, newline-terminated):
//   DUMP    - print the full contents of /clicks.csv
//   CLEAR   - erase the log file (also resets burst-cycle numbering, see
//             CYCLESTART)
//   CYCLESTART - resets burst-cycle numbering only (leaves the log file
//             alone): the next detected burst becomes burst 0/"opening".
//             Run this once right before each repeated full test cycle
//             (e.g. right before triggering a manual multi-station
//             Program Run) so burst_label values line up cycle to cycle -
//             see the burst_label doc comment above for why this matters
//             on multi-station rigs.
//   STATUS  - print current threshold, click count, free space
//   THRESH <n> - set the ON threshold (peak amplitude, 0-32767)
//   SERVE   - temporarily reconnect WiFi and start a small HTTP server so
//             /clicks.csv can be fetched with `curl` from another machine.
//             Click detection keeps running while serving (experimental -
//             watch for WiFi-induced noise on the mic). Auto-shuts off WiFi
//             again after SERVE_TIMEOUT_MS (or send STOP to end it early).
//   STOP    - end an active SERVE session immediately
//   PUSHNOW - trigger an auto-push cycle immediately (for testing), instead
//             of waiting for the next PUSH_INTERVAL_MS
//   HALLON/HALLOFF - toggle Hall-effect-sensor RESEARCH logging
//             (experimental, off by default). When on, each CSV row gains 1
//             extra column: hall_triggered (0/1) - whether the external
//             Grove Hall Effect Unit (M5Stack SKU U084, digital active-LOW
//             switch output) fired at any point during the click event.
//             Theory being a DC latching solenoid reverses coil current
//             direction between open/close, so the magnetic pulse polarity
//             might reliably distinguish them where acoustic features
//             haven't - this sensor can only detect ONE specific pole
//             direction (unlike the old built-in hallRead(), which read a
//             continuous ADC value), so it may only trigger on one of the
//             two click types rather than both.
//   TMAGON/TMAGOFF - toggle TMAG5273 3-axis I2C magnetometer RESEARCH
//             logging (experimental, off by default; no-ops if the sensor
//             wasn't detected at boot). When on, each CSV row gains 6 extra
//             columns: x_min,x_max,y_min,y_max,z_min,z_max - the min/max
//             field reading (mT) seen per axis during the click event.
//             Unlike the A3144E (a simple on/off threshold switch), this
//             gives continuous signed field data per axis, so we can look
//             for a direction/magnitude signature distinguishing open vs
//             close, not just a single trigger bit. NOTE: the A3144E
//             (PIN_HALL_DATA) and the TMAG5273 share the same two physical
//             Grove Port A pins - they're meant to be swapped in/out one at
//             a time, not connected simultaneously.
//   TMAGSTREAM/TMAGSTREAMOFF - continuously prints the TMAG5273's live X/Y/Z
//             reading to Serial every ~150ms, independent of click
//             detection - for manual bench positioning (watching field
//             values change in real time while moving the sensor around
//             the solenoid or between its resting open/closed states),
//             analogous to watching the A3144E's onboard LED but with
//             actual numbers instead of a single trigger threshold.
//   TMAGCAL <n> - auto-calibrates a TMAG5273-based multi-class classifier
//             from the next <n> click events (default 8 - enough for ~2
//             full cycles on a 2-solenoid rig), using burst-derived labels
//             as ground truth (see burst_label in the log format above).
//             Rather than hardcoding a specific axis/sign or assuming
//             exactly 2 classes (open/close) - which would only be valid
//             for one particular sensor mounting/orientation and a
//             single-solenoid rig - this computes an empirical MEAN
//             VECTOR per distinct burst label seen (e.g. "O0"/"O1"/"C0"/
//             "C1" for a staggered master+station rig), then classifies
//             future events by NEAREST CENTROID. Generalizes to any
//             mounting orientation and any number of staggered solenoids
//             with zero code changes - re-run anytime the sensor gets
//             repositioned or the rig changes. Needs to see at least 2
//             distinct classes to succeed (prints a warning and stays
//             uncalibrated otherwise; prints each class's sample count and
//             mean vector on success - see STATUS for a summary anytime).
//             Once calibrated, every logged event gets an extra
//             classification field (see classification format below) -
//             RAM-only, resets on reboot like THRESH/HALLON/etc.
//
// NOTE: the built-in ESP32 Hall sensor (hallRead()) previously used here was
// removed - it never produced a reliably usable signal (see HANDOFF.md):
// real click events showed large bidirectional swings that couldn't be
// pinned to a consistent open/close polarity, and its "finger-snap/noise
// rejection" filter (comparing deviation-from-baseline to a threshold) was
// confirmed to also fire on/miss finger snaps just as often as real clicks,
// making it unreliable as a noise filter too. That whole
// baseline/deviation-threshold rejection mechanism is gone along with it;
// there is currently no automatic noise rejection - reimplementing that
// with the new external switch sensor (if needed) is future work.
//// Automatic push: every PUSH_INTERVAL_MS (default 5 min, see secrets.h) the
// board also independently wakes WiFi just long enough to POST the entire
// current /clicks.csv to PUSH_HOST:PUSH_PORT/PUSH_PATH, clears the log on a
// successful response, then turns WiFi back off - so it can run fully
// wireless (off USB, on battery) and still get data off periodically
// without needing a manual SERVE/curl. Skipped automatically while a manual
// SERVE session is already active.
//
// Opportunistic RTC resync: rather than a dedicated wake-up, the RTC gets
// re-synced via NTP whenever WiFi is already connected for another reason
// (push, SERVE, button) - but only if it's been more than
// NTP_RESYNC_INTERVAL_MS (default 1 hour) since the last sync. The BM8563
// RTC is accurate enough that hourly is comfortably conservative; this just
// keeps long-running/battery sessions from drifting over many hours.

#include <M5StickCPlus.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <time.h>
#include <SPIFFS.h>
#include <string.h>
#include <Wire.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
// Fallback so the sketch still compiles without secrets.h; NTP sync will
// simply fail/time out and the board will fall back to whatever time the
// RTC already has.
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define GMT_OFFSET_SEC 0
#define DAYLIGHT_OFFSET_SEC 0
#define PUSH_HOST ""
#define PUSH_PORT 8090
#define PUSH_PATH "/click"
#define PUSH_PATH_SNIPPETS "/snippets"
#define PUSH_INTERVAL_MS (5UL * 60UL * 1000UL)
#endif

// ---------------------------------------------------------------------------
// Mic / I2S configuration (PDM mic on M5StickC Plus)
// ---------------------------------------------------------------------------
#define PIN_CLK  0
#define PIN_DATA 34
#define SAMPLE_RATE 44100
#define CHUNK_SAMPLES 256  // ~5.8ms per chunk at 44100Hz

// ---------------------------------------------------------------------------
// External Hall Effect sensor (Grove Port A, M5Stack "Hall Effect Unit"
// SKU U084 - 3x A3144E Hall switches + 74HC08 AND gate). Digital,
// active-LOW output: pulled low when a magnetic field of the right
// polarity/strength is detected in front of the unit, high otherwise. G33
// is Grove Port A's data pin on the M5StickC Plus (confirmed against
// M5Stack's own LIMIT/PIR unit examples, which both use pin 33 for Port
// A's digital signal; G32 is Port A's other pin, unused here).
// INPUT_PULLUP is harmless/defensive even though the module's own output is
// push-pull, not open-collector.
// ---------------------------------------------------------------------------
#define PIN_HALL_DATA 33

static int16_t audioBuf[CHUNK_SAMPLES];

// ---------------------------------------------------------------------------
// Raw snippet capture (calibration/analysis mode) - OFF by default.
//
// Toggled via serial commands SNIPON/SNIPOFF. When on, captures a short raw
// PCM waveform around each detected click event (a couple chunks of
// pre-roll, so we catch the true onset before threshold-crossing, plus the
// event itself) and appends it to /snippets.bin for offline analysis (real
// FFT, spectrograms, etc. in Python - far richer than anything cheap enough
// to compute on-device). NOTE (2026-09-09): acoustic precision has been
// deprioritized entirely (see top-of-file doc comment) - this is now purely
// a legacy/optional diagnostic, not expected to feed back into
// classification like it once might have.
// ---------------------------------------------------------------------------
bool snippetModeOn = false;
const int PREROLL_CHUNKS = 2;
int16_t prerollBuf[PREROLL_CHUNKS][CHUNK_SAMPLES];
int prerollWriteIdx = 0;

const int MAX_SNIPPET_SAMPLES = 2600;  // ~59ms @ 44.1kHz - covers a ~20ms
                                        // click plus generous pre/post margin
int16_t snippetBuf[MAX_SNIPPET_SAMPLES];
int snippetLen = 0;
bool capturingSnippet = false;

const char *SNIPPETS_PATH = "/snippets.bin";

// Appends one chunk's samples into snippetBuf, bounded by capacity.
void appendToSnippet(const int16_t *samples, size_t n) {
    size_t room = MAX_SNIPPET_SAMPLES - snippetLen;
    size_t toCopy = (n < room) ? n : room;
    memcpy(&snippetBuf[snippetLen], samples, toCopy * sizeof(int16_t));
    snippetLen += toCopy;
}

// Writes the just-captured snippet as one record in /snippets.bin:
//   ASCII header line "<iso_timestamp>,<num_samples>\n" followed by
//   num_samples raw little-endian int16 PCM samples. Simple, sequential,
//   trivial to parse in Python (readline, then read count*2 bytes, repeat).
void saveSnippet(const String &isoTs) {
    if (snippetLen == 0) return;
    File f = SPIFFS.open(SNIPPETS_PATH, FILE_APPEND);
    if (!f) {
        Serial.println("  [WARN] could not open snippets file for append");
        return;
    }
    String header = isoTs + "," + String(snippetLen) + "\n";
    f.print(header);
    f.write((const uint8_t *)snippetBuf, snippetLen * sizeof(int16_t));
    f.close();
}

// ---------------------------------------------------------------------------
// Click-detector tuning
// ---------------------------------------------------------------------------
int16_t THRESH_ON  = 6000;   // peak amplitude to start an event
int16_t THRESH_OFF = 3000;   // peak amplitude to consider the event "quiet" again
const int RELEASE_CHUNKS = 3;    // consecutive quiet chunks required to end an event
const uint32_t MIN_CLICK_MS = 2;    // ignore events shorter than this (noise)
const uint32_t MAX_CLICK_MS = 500;  // safety cap; longer than this is not a "click"
const uint32_t REFRACTORY_MS = 150; // ignore new events for this long after one ends

const char *LOG_PATH = "/clicks.csv";

// ---------------------------------------------------------------------------
// On-demand HTTP server ("SERVE" serial command) - lets `curl` fetch
// /clicks.csv without keeping WiFi on all the time. Click detection keeps
// running while serving (experimental).
// ---------------------------------------------------------------------------
WebServer httpServer(80);
bool serveModeActive = false;
uint32_t serveModeUntilMs = 0;
const uint32_t SERVE_TIMEOUT_MS = 5UL * 60UL * 1000UL;  // 5 minutes

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------
enum DetectorState { IDLE, ACTIVE, REFRACTORY };
DetectorState state = IDLE;

uint32_t eventStartMs   = 0;
uint32_t refractoryUntil = 0;
int quietChunkCount     = 0;
int16_t peakThisEvent   = 0;

// Timing-based prediction (diagnostic): a click starting more than
// TIMING_GAP_THRESHOLD_MS after the previous one is assumed to be the
// START of a new cycle - the
// valve is normally-closed and opens briefly, so this is the "opening"
// click ('1'); one arriving sooner is assumed to be the END of the current
// cycle, valve returning closed ('0'). This has been 100% consistent
// across every physical test setup so far, unlike any acoustic feature
// we've tried - logged alongside the acoustic guess so we can see, event by
// event, whether/when they agree.
const uint32_t TIMING_GAP_THRESHOLD_MS = 25000;
uint32_t prevEventStartMs = 0;      // 0 = no previous event yet this boot
char currentTimingPrediction = '0';

// Millisecond-precision gap since the previous click event (computed via
// millis(), same value the timing-based '1'/'0' prediction is based on),
// captured at trigger time and logged alongside the RTC timestamp. The RTC
// only has whole-SECOND resolution, so relying on ISO-timestamp deltas
// between consecutive log rows to measure short inter-click gaps (e.g.
// master valve vs. station valve staggering) is only accurate to +/-1s;
// this field gives the real sub-second precision. 0xFFFFFFFF (logged as
// "NA") means this is the first event since boot - no previous event to
// compare against.
uint32_t currentEventGapMs = 0;

// ---------------------------------------------------------------------------
// Burst grouping - detects multiple solenoids firing in a staggered
// sequence around the same open/close transition (e.g. a master valve +
// station valve on the same controller, discovered to fire ~1-2s apart
// rather than simultaneously - likely capacitor recharge time on this
// battery-powered controller). Events less than BURST_GAP_MS apart are
// considered part of the same burst.
//
// Burst TYPE (open vs close) is decided by strict alternation, NOT by
// comparing the gap since the previous event against a fixed threshold:
// the first burst since the last CYCLESTART/CLEAR is "open", then it
// flips every new burst. This is robust no matter how long a given
// inter-station transition takes (a multi-station program run was found
// to have ~18s between one station's close and the next station's open -
// long enough to start a new burst but easily confusable with a "close"
// gap under any fixed-threshold scheme). See burstLabel() below for how
// this pairs with a monotonic per-cycle burst index to keep every
// station's labels collision-free regardless of program size.
//
// BURST_GAP_MS must sit comfortably above the intra-burst gap (~1-2s
// observed so far) and comfortably below the minimum inter-burst gap
// within one program run (this controller's enforced minimum station
// runtime is 7s). 5000ms gives margin on both sides; it no longer needs
// to also distinguish "new station" from "same station closing" (that's
// now handled by alternation + the burst index), so it doesn't need to be
// anywhere near as large as the longest possible inter-station transition.
// ---------------------------------------------------------------------------
const uint32_t BURST_GAP_MS = 5000;
bool currentBurstIsOpen = true;    // this event's burst's open/close type
int currentBurstPosition = 0;      // 0-based position within its burst

// Monotonically-increasing count of bursts seen since the last
// CYCLESTART/CLEAR (or boot) - embedded in the label so that different
// stations'/solenoids' bursts within one multi-station program run can
// never collide, no matter how many there are or how long the gaps
// between them are. Reset to 0 by resetBurstCycle() (see CYCLESTART/CLEAR
// command handlers below).
int burstSeqIndex = -1;  // -1 = no burst started yet since last reset;
                          // becomes 0 on the first event of a new cycle.

// Resets burst-sequence numbering and open/close alternation back to a
// fresh cycle start ("the next burst detected will be burst #0, and it
// will be labeled as an opening transition"). Call this once before each
// repeated full test cycle (e.g. right before triggering a manual
// Program Run) so the same burstSeq numbers - and hence the same
// burst_label values - recur cycle to cycle, which TMAGCAL depends on to
// average multiple samples of the same real-world class together. Also
// called by CLEAR, since starting a fresh log implies a fresh cycle too.
void resetBurstCycle() {
    burstSeqIndex = -1;
    currentBurstIsOpen = true;
    currentBurstPosition = 0;
}

// Builds this event's burst label, e.g. "O0_0" (position 0 of burst 0 -
// on a master+station rig, the master valve opening), "O0_1" (position 1
// of the same burst - the station valve opening), "C1_0"/"C1_1" similarly
// for burst 1 (a closing burst - station closes first, then master).
// A third station's own single-solenoid open/close a program run later
// becomes burst 2 ("O2_0") and burst 3 ("C3_0") - the ever-incrementing
// burst index guarantees these can never collide with burst 0/1's labels,
// however many stations/solenoids are chained together in one program.
// Degenerates to just "O0_0"/"C1_0" per cycle on a single-valve rig
// (matching the original binary open/close behavior, modulo the new
// explicit burst index suffix).
String burstLabel() {
    return String(currentBurstIsOpen ? "O" : "C") + String(burstSeqIndex) +
           "_" + String(currentBurstPosition);
}

// ---------------------------------------------------------------------------
// Hall-effect sensor correlation (experimental, HALLON/HALLOFF) - reads the
// external Grove Hall Effect Unit (see PIN_HALL_DATA above). Theory: this is
// a DC LATCHING solenoid, which reverses coil current direction to switch
// open vs close - so the magnetic pulse accompanying each actuation might
// trip this switch on only one of the two click types, unlike anything
// we've found acoustically. Since the sensor is a threshold switch (not a
// continuous ADC reading like the old built-in hallRead()), all we track is
// whether it went LOW (triggered) at any point during the click event.
// ---------------------------------------------------------------------------
bool hallModeOn = false;
bool hallTriggeredEvent = false;

// ---------------------------------------------------------------------------
// External TMAG5273 3-axis I2C magnetometer (Grove Port A, SDA=GPIO32,
// SCL=GPIO33 - confirmed via M5Stack's own ACCEL_ADXL345/TVOC_SGP30 example
// sketches, and verified live with an I2C scan on this exact board/unit
// finding it at the expected default address 0x35).
//
// NOTE: this shares the same two physical Grove Port A pins as the A3144E
// digital switch above (PIN_HALL_DATA = GPIO33 is also the I2C SCL line) -
// the two sensors are meant to be swapped in/out one at a time, NOT
// connected simultaneously. Whichever is actually plugged in "wins" that
// pin electrically; the other sensor's code just reads meaningless data
// harmlessly (not logged unless its own *ON command is toggled).
//
// Unlike the A3144E (a simple threshold switch), this gives continuous
// signed X/Y/Z field readings in mT - the goal is to look for either (a) a
// field DIRECTION/magnitude signature during a click that distinguishes
// open vs close, or (b) a static positional difference in the RESTING
// field between the two valve states (open vs closed, no click happening) -
// something the A3144E's single-threshold digital output can't reveal.
// ---------------------------------------------------------------------------
#define TMAG5273_I2C_ADDR 0x35  // confirmed via I2C scan on this exact unit

const uint8_t TMAG_REG_DEVICE_CONFIG_2     = 0x01;
const uint8_t TMAG_REG_SENSOR_CONFIG_1     = 0x02;
const uint8_t TMAG_REG_T_CONFIG            = 0x07;
const uint8_t TMAG_REG_DEVICE_ID           = 0x0D;
const uint8_t TMAG_REG_MANUFACTURER_ID_LSB = 0x0E;
const uint8_t TMAG_REG_X_MSB_RESULT        = 0x12;  // X/Y/Z MSB,LSB = 6
                                                      // consecutive bytes
                                                      // starting here.

const uint16_t TMAG_MANUFACTURER_ID_EXPECTED = 0x5449;  // "TI", per datasheet

// The TMAG5273 comes in two range variants sharing the exact same
// registers/pinout - DEVICE_ID register bits[1:0] tell them apart (verified
// against both TI's SparkFun Arduino library and Adafruit's own
// Adafruit_TMAG5273 driver source, not guessed):
//   0x1 = "X1" variant: +/-40mT / +/-80mT (range bit 0 = narrow/wide)
//   0x2 = "X2" variant: +/-133mT / +/-266mT
// This board is confirmed to be an Adafruit TMAG5273 **A2** (product 6490,
// the X2/wide-range variant) - but we detect it live at boot rather than
// hardcoding that, so this code keeps working correctly if an A1 board
// (product 6489) ever gets swapped in instead. SENSOR_CONFIG_2's range
// bits are left at their power-on-reset default (0 = narrow), so the
// active range is 40mT for X1 or 133mT for X2 - tmagRangeMt is set to
// match whichever was actually detected.
float tmagRangeMt = 40.0f;  // placeholder until tmagInit() detects the
                             // real variant; DO NOT trust readings if
                             // tmagOk is false.

bool tmagOk = false;         // sensor detected + configured OK at boot
bool tmagModeOn = false;     // TMAGON/TMAGOFF: adds 6 CSV columns (x/y/z
                              // min/max) to each logged click event
bool tmagStreamOn = false;   // TMAGSTREAM/TMAGSTREAMOFF: live-prints X/Y/Z
                              // to Serial continuously - for manual bench
                              // positioning (watching values change in real
                              // time as you move the sensor around the
                              // solenoid, no click needed, same idea as
                              // watching the A3144E's LED but with numbers)
uint32_t lastTmagStreamMs = 0;
const uint32_t TMAG_STREAM_INTERVAL_MS = 150;

// Per-event extremes (like hallMinEvent/hallMaxEvent used to be for the old
// built-in hallRead()), tracked per axis while TMAGON is active.
float tmagXMin, tmagXMax, tmagYMin, tmagYMax, tmagZMin, tmagZMax;

// ---------------------------------------------------------------------------
// TMAG5273 auto-calibrated multi-class classifier (TMAGCAL command).
//
// Rather than hardcoding "the discriminating signal is on the Z axis,
// positive=open" (true for one particular bench setup/mounting, but not
// something to assume for every future rig/orientation), this computes
// empirical class-mean vectors in 3D magnetic space from a few real click
// events, using ground-truth labels derived purely from timing (see
// burstLabel() above - "O0"/"O1"/... for staggered opens, "C0"/"C1"/...
// for staggered closes):
//   1. Capture each click's peak signed deviation per axis (whichever of
//      min/max has the larger magnitude) as a representative 3D sample.
//   2. Average those vectors separately per distinct burst-label class
//      seen during calibration (a single-valve rig naturally only ever
//      produces 2 classes, "O0"/"C0" - the exact binary open/close case
//      this started as; a master+station rig produces 4: "O0","O1","C0",
//      "C1"; more staggered solenoids would produce more, with zero code
//      changes needed).
//   3. Classify future events via NEAREST CENTROID: whichever calibrated
//      class mean is closest (Euclidean distance) to the new event's own
//      vector.
// This generalizes across mounting orientations AND number of solenoids
// with zero code changes - just re-run TMAGCAL after remounting/adding
// solenoids.
//
// RAM-only, like THRESH/HALLON/etc - resets on reboot, must be re-run after
// every reflash (consistent with the rest of this project's RAM-only
// settings; see HANDOFF.md).
// ---------------------------------------------------------------------------
const int MAX_TMAG_CLASSES = 8;  // generous headroom beyond the 4 classes
                                   // expected from a 2-solenoid rig
const int MAX_TMAG_CAL_SAMPLES = 64;  // generous headroom for raw
                                        // calibration samples (used to
                                        // measure each class's spread -
                                        // see TMAG_REJECT_MARGIN below)

bool tmagCalibrating = false;
bool tmagCalibrated = false;
int tmagCalRemaining = 0;  // calibration events still needed before we
                            // finalize the classifier

String tmagClassLabels[MAX_TMAG_CLASSES];
float tmagClassSum[MAX_TMAG_CLASSES][3];
int tmagClassCount[MAX_TMAG_CLASSES];
float tmagClassMean[MAX_TMAG_CLASSES][3];  // finalized once calibration
                                             // completes
int tmagNumClasses = 0;

// Raw per-event samples collected during calibration (label + vector),
// kept around just long enough to measure each class's own spread
// (max distance from its mean to any of its own calibration samples)
// once calibration finishes - see TMAG_REJECT_MARGIN below.
String tmagCalSampleLabel[MAX_TMAG_CAL_SAMPLES];
float tmagCalSampleVec[MAX_TMAG_CAL_SAMPLES][3];
int tmagCalSampleCount = 0;

// "No Correlation" flag: classifyTmagVector() returns this instead of a
// real class label when an event's vector doesn't land close to ANY
// calibrated class - e.g. a finger-snap or other spurious acoustic
// trigger, which (unlike a real solenoid actuation) has no accompanying
// magnetic deviation and so should sit near the ambient baseline, far
// from every real click cluster. Unlike the old built-in hallRead()-based
// noise filter (removed - confirmed unreliable, see HANDOFF.md), this
// doesn't discard/reject the event - it still gets logged/counted/shown
// normally, just flagged, so nothing is silently thrown away and the
// classifier's behavior stays fully visible/auditable.
const char *TMAG_NO_CORRELATION = "NC";

// How far (in multiples of the largest intra-class spread actually
// observed during calibration) an event's vector can be from its nearest
// class mean before it gets flagged TMAG_NO_CORRELATION instead of that
// class's label. Grounded in real per-class variance from calibration
// data (not a guessed absolute number, unlike the old hallRead()-based
// filter) - still just a multiplier choice, tune if false-flagging or
// under-flagging shows up in practice.
const float TMAG_REJECT_MARGIN = 2.5f;
float tmagRejectThresholdSq = 1e18f;  // squared distance; effectively
                                        // "never flag" until calibration
                                        // sets a real value

// Most recently classified event's label (e.g. "O0", "C1", TMAG_NO_CORRELATION,
// or "?" if not yet calibrated), exposed as an extra indicator field so it
// can be visually cross-checked against the timing/burst-derived ground
// truth, same as the acoustic duration-based guess already is.
String tmagClassification = "?";

// Returns the peak signed deviation for one axis - whichever of min/max
// has the larger absolute value. Approximates a representative sample of
// the event's field vector without needing new simultaneous-sample
// instrumentation (reuses the existing per-axis min/max tracking).
float tmagPeakSigned(float minVal, float maxVal) {
    return (fabsf(minVal) > fabsf(maxVal)) ? minVal : maxVal;
}

// Starts (or restarts) a calibration run: the next `n` click events will be
// used as training examples (their burst-derived label taken as ground
// truth), after which per-class means are computed automatically. Safe to
// call again anytime (e.g. after remounting/repositioning the sensor, or
// adding another solenoid to the rig) - resets any previous calibration.
void startTmagCalibration(int n) {
    tmagCalibrating = true;
    tmagCalibrated = false;
    tmagCalRemaining = n;
    tmagNumClasses = 0;
    tmagCalSampleCount = 0;
    for (int i = 0; i < MAX_TMAG_CLASSES; i++) {
        tmagClassLabels[i] = "";
        tmagClassSum[i][0] = tmagClassSum[i][1] = tmagClassSum[i][2] = 0;
        tmagClassCount[i] = 0;
    }
}

// Finds the class slot for a given label, creating a new one if this
// label hasn't been seen yet this calibration run. Returns -1 if out of
// slots (MAX_TMAG_CLASSES exceeded - extremely unlikely for any real rig).
int findOrCreateTmagClass(const String &label) {
    for (int i = 0; i < tmagNumClasses; i++) {
        if (tmagClassLabels[i] == label) return i;
    }
    if (tmagNumClasses >= MAX_TMAG_CLASSES) return -1;
    tmagClassLabels[tmagNumClasses] = label;
    return tmagNumClasses++;
}

// Feeds one click event's peak vector into the ongoing calibration
// (accumulating into whichever class its burst label identifies), and
// finalizes per-class means (plus the no-correlation distance threshold)
// once enough events have been seen. Safe to call unconditionally per
// event; no-ops if a calibration isn't currently running.
void feedTmagCalibration(const String &label, float vecX, float vecY,
                          float vecZ) {
    if (!tmagCalibrating) return;

    int idx = findOrCreateTmagClass(label);
    if (idx >= 0) {
        tmagClassSum[idx][0] += vecX;
        tmagClassSum[idx][1] += vecY;
        tmagClassSum[idx][2] += vecZ;
        tmagClassCount[idx]++;
    }
    if (tmagCalSampleCount < MAX_TMAG_CAL_SAMPLES) {
        tmagCalSampleLabel[tmagCalSampleCount] = label;
        tmagCalSampleVec[tmagCalSampleCount][0] = vecX;
        tmagCalSampleVec[tmagCalSampleCount][1] = vecY;
        tmagCalSampleVec[tmagCalSampleCount][2] = vecZ;
        tmagCalSampleCount++;
    }

    tmagCalRemaining--;
    if (tmagCalRemaining > 0) return;  // still collecting

    tmagCalibrating = false;

    if (tmagNumClasses < 2) {
        Serial.println("[WARN] TMAG calibration failed - saw fewer than 2 "
                        "distinct classes (check DUMP for burst labels). "
                        "Not calibrated.");
        return;
    }

    Serial.printf("TMAG calibration complete: %d class(es)\n", tmagNumClasses);
    for (int i = 0; i < tmagNumClasses; i++) {
        tmagClassMean[i][0] = tmagClassSum[i][0] / tmagClassCount[i];
        tmagClassMean[i][1] = tmagClassSum[i][1] / tmagClassCount[i];
        tmagClassMean[i][2] = tmagClassSum[i][2] / tmagClassCount[i];
        Serial.printf("  '%s': n=%d mean=(%.2f, %.2f, %.2f)\n",
                      tmagClassLabels[i].c_str(), tmagClassCount[i],
                      tmagClassMean[i][0], tmagClassMean[i][1],
                      tmagClassMean[i][2]);
    }

    // Measure each class's own spread: the largest distance from its mean
    // to any of ITS OWN calibration samples. The largest such spread
    // across all classes, times TMAG_REJECT_MARGIN, becomes the
    // "too far to be any known class" threshold - grounded in this rig's
    // actual observed variance, not a guess.
    float maxIntraClassDistSq = 0;
    for (int s = 0; s < tmagCalSampleCount; s++) {
        int idx = -1;
        for (int c = 0; c < tmagNumClasses; c++) {
            if (tmagClassLabels[c] == tmagCalSampleLabel[s]) {
                idx = c;
                break;
            }
        }
        if (idx < 0) continue;
        float dx = tmagCalSampleVec[s][0] - tmagClassMean[idx][0];
        float dy = tmagCalSampleVec[s][1] - tmagClassMean[idx][1];
        float dz = tmagCalSampleVec[s][2] - tmagClassMean[idx][2];
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq > maxIntraClassDistSq) maxIntraClassDistSq = distSq;
    }
    // Floor of (0.1mT * margin)^2 so a degenerate zero-spread calibration
    // (e.g. only 1 sample in every class) doesn't make the threshold zero
    // and flag every single future event as no-correlation.
    const float MIN_SPREAD_SQ = 0.01f;  // (0.1mT)^2
    if (maxIntraClassDistSq < MIN_SPREAD_SQ) maxIntraClassDistSq = MIN_SPREAD_SQ;
    tmagRejectThresholdSq =
        maxIntraClassDistSq * (TMAG_REJECT_MARGIN * TMAG_REJECT_MARGIN);
    Serial.printf("  no-correlation threshold: %.2fmT (%.1fx largest "
                  "observed intra-class spread of %.2fmT)\n",
                  sqrtf(tmagRejectThresholdSq), TMAG_REJECT_MARGIN,
                  sqrtf(maxIntraClassDistSq));

    tmagCalibrated = true;
}

// Classifies one event's peak vector via nearest centroid - whichever
// calibrated class mean is closest (Euclidean distance), or
// TMAG_NO_CORRELATION if even the nearest class is farther away than
// tmagRejectThresholdSq (see feedTmagCalibration) - i.e. this event's
// magnetic signature doesn't resemble any known click type, the way a
// finger-snap or other spurious acoustic trigger wouldn't. Only
// meaningful if tmagCalibrated; callers should check that first.
String classifyTmagVector(float vecX, float vecY, float vecZ) {
    int bestIdx = -1;
    float bestDist = 1e18f;
    for (int i = 0; i < tmagNumClasses; i++) {
        float dx = vecX - tmagClassMean[i][0];
        float dy = vecY - tmagClassMean[i][1];
        float dz = vecZ - tmagClassMean[i][2];
        float dist = dx * dx + dy * dy + dz * dz;
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    if (bestIdx < 0) return "?";
    if (bestDist > tmagRejectThresholdSq) return TMAG_NO_CORRELATION;
    return tmagClassLabels[bestIdx];
}

bool tmagWriteReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(TMAG5273_I2C_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool tmagReadRegs(uint8_t startReg, uint8_t *buf, uint8_t len) {
    Wire.beginTransmission(TMAG5273_I2C_ADDR);
    Wire.write(startReg);
    if (Wire.endTransmission(false) != 0) return false;  // repeated start,
                                                           // keep bus held
    if (Wire.requestFrom((int)TMAG5273_I2C_ADDR, (int)len) != len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

// Converts a raw signed 16-bit register pair (MSB, LSB) to a field strength
// in mT, per the TMAG5273 datasheet's conversion (full-scale RANGE maps to
// the signed 16-bit range: value/32768 * RANGE). Uses the RANGE detected at
// init time (tmagRangeMt), not a hardcoded constant - see note above.
float tmagRawToMt(int16_t raw) {
    return ((float)raw * tmagRangeMt) / 32768.0f;
}

// Configures the sensor for continuous X/Y/Z (+temperature) conversion and
// verifies its manufacturer ID AND detects its range variant (A1 vs A2)
// over I2C first. Returns false (leaving tmagOk false) if anything doesn't
// check out - callers must treat all tmagReadXYZ() calls as
// unavailable/untrustworthy in that case rather than silently logging
// garbage (or, worse, correctly-shaped but wrongly-SCALED garbage - the
// original version of this code assumed the A1's 40mT range unconditionally,
// which would have silently under-reported this A2 board's actual field
// strength by ~3.3x).
bool tmagInit() {
    uint8_t idBuf[2];
    if (!tmagReadRegs(TMAG_REG_MANUFACTURER_ID_LSB, idBuf, 2)) return false;
    uint16_t mfgId = ((uint16_t)idBuf[1] << 8) | idBuf[0];
    if (mfgId != TMAG_MANUFACTURER_ID_EXPECTED) {
        Serial.printf("[WARN] TMAG5273 manufacturer ID mismatch: got 0x%04X, "
                      "expected 0x%04X - not initializing.\n", mfgId,
                      TMAG_MANUFACTURER_ID_EXPECTED);
        return false;
    }

    uint8_t deviceIdReg;
    if (!tmagReadRegs(TMAG_REG_DEVICE_ID, &deviceIdReg, 1)) return false;
    uint8_t variant = deviceIdReg & 0x03;
    if (variant == 0x1) {
        tmagRangeMt = 40.0f;
        Serial.println("TMAG5273 variant: X1 (+/-40mT range, narrow).");
    } else if (variant == 0x2) {
        tmagRangeMt = 133.0f;
        Serial.println("TMAG5273 variant: X2 (+/-133mT range, narrow).");
    } else {
        Serial.printf("[WARN] TMAG5273 DEVICE_ID variant byte 0x%02X not "
                       "recognized (expected 1 or 2) - not initializing.\n",
                       variant);
        return false;
    }

    // SENSOR_CONFIG_1: enable X, Y, Z magnetic channels (bits 7-4 = 0x7),
    // sleep-time bits (3-0) left at 0 - unused outside wake-up/sleep mode.
    if (!tmagWriteReg(TMAG_REG_SENSOR_CONFIG_1, 0x70)) return false;
    // T_CONFIG: enable the temperature channel (bit 0) - logged for
    // reference/context only, not currently used for anything.
    if (!tmagWriteReg(TMAG_REG_T_CONFIG, 0x01)) return false;
    // DEVICE_CONFIG_2: continuous measure mode (bits 1-0 = 0x2) - keeps the
    // sensor converting in the background so reads are always fresh,
    // rather than needing an explicit trigger per read.
    if (!tmagWriteReg(TMAG_REG_DEVICE_CONFIG_2, 0x02)) return false;

    return true;
}

// Reads the current X/Y/Z field (mT). Returns false on any I2C error -
// callers must not trust x/y/z if this returns false.
bool tmagReadXYZ(float &x, float &y, float &z) {
    uint8_t buf[6];
    if (!tmagReadRegs(TMAG_REG_X_MSB_RESULT, buf, 6)) return false;
    int16_t rawX = ((int16_t)buf[0] << 8) | buf[1];
    int16_t rawY = ((int16_t)buf[2] << 8) | buf[3];
    int16_t rawZ = ((int16_t)buf[4] << 8) | buf[5];
    x = tmagRawToMt(rawX);
    y = tmagRawToMt(rawY);
    z = tmagRawToMt(rawZ);
    return true;
}

// Suppresses click detection for the given duration - used any time we know
// a spurious/non-solenoid acoustic event is about to happen (or just did)
// so it doesn't get logged as a real click.
void suppressDetectionFor(uint32_t ms) {
    refractoryUntil = millis() + ms;
    state = IDLE;
    quietChunkCount = 0;
}

// WiFi connect/disconnect transitions have been observed to produce
// spurious "click" readings on the mic - long (150-400ms), very low
// frequency (<150Hz) glitches that don't match real click acoustics
// (17-24ms, 5-8kHz). Call this around any WiFi state change.
const uint32_t WIFI_TRANSITION_SUPPRESS_MS = 1500;
void suppressDetectionForWifiTransition() {
    suppressDetectionFor(WIFI_TRANSITION_SUPPRESS_MS);
}

// The front button itself is a much louder/longer acoustic event now that
// the board is mechanically coupled to the solenoid (button contact thump
// transmits directly into the solenoid body) - e.g. 99-116ms/loud, vs a
// real click's 17-24ms. Call this the instant a button press is detected,
// before any WiFi logic even runs.
const uint32_t BUTTON_PRESS_SUPPRESS_MS = 600;
void suppressDetectionForButtonPress() {
    suppressDetectionFor(BUTTON_PRESS_SUPPRESS_MS);
}

uint32_t clickCount = 0;

// Most recent 4 events. Each entry keeps its own color, assigned once at
// creation time from a rotating color wheel (golden-angle hue step, so
// consecutive events get visibly distinct-but-related hues instead of a
// static tiered palette). Colors travel WITH their event as it shifts
// position on screen - they don't get reassigned by row.
//
// entries[0]                = oldest of the visible window (rendered at TOP)
// entries[NUM_HISTORY_LINES-1] = newest                     (rendered at BOTTOM)
struct EventEntry {
    String timePart;  // printed at a smaller font (see TIME_TEXT_SIZE) to
                       // save horizontal space for the rest of the line
    String restPart;  // printed at the normal font size
    uint16_t color;
};
const int NUM_HISTORY_LINES = 6;
EventEntry eventLines[NUM_HISTORY_LINES] = {
    {"", "", BLACK}, {"", "", BLACK}, {"", "", BLACK},
    {"", "", BLACK}, {"", "", BLACK}, {"", "", BLACK}};

RTC_TimeTypeDef eventStartTime;
RTC_DateTypeDef eventStartDate;

// Live clock, updated periodically so the seconds display ticks in real
// time (no more flashing colon needed now that seconds are visible).
uint32_t lastClockUpdateMs = 0;

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

// Standalone RGB565 packer (doesn't depend on M5.Lcd being initialized), so
// it's safe to use in file-scope constant initializers.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
}

const uint16_t STATUS_COLOR_ON  = rgb565(80, 255, 120);  // green - wifi on
const uint16_t STATUS_COLOR_OFF = rgb565(255, 90, 90);   // red   - wifi off

// Simple HSV (0-359, 0-255, 0-255) -> RGB565 conversion, used to give each
// click event its own point on a rotating color wheel.
uint16_t hsvColor565(int hue, uint8_t sat, uint8_t val) {
    hue = ((hue % 360) + 360) % 360;
    uint8_t region = hue / 60;
    uint8_t remainder = (hue % 60) * 255 / 60;

    uint8_t p = (val * (255 - sat)) / 255;
    uint8_t q = (val * (255 - (sat * remainder) / 255)) / 255;
    uint8_t t = (val * (255 - (sat * (255 - remainder)) / 255)) / 255;

    uint8_t r, g, b;
    switch (region) {
        case 0: r = val; g = t; b = p; break;
        case 1: r = q; g = val; b = p; break;
        case 2: r = p; g = val; b = t; break;
        case 3: r = p; g = q; b = val; break;
        case 4: r = t; g = p; b = val; break;
        default: r = val; g = p; b = q; break;
    }
    return M5.Lcd.color565(r, g, b);
}

// Golden-angle hue step (~137.5 degrees) gives a good spread of distinct
// colors across consecutive event indices without obvious repetition.
uint16_t colorForEventIndex(uint32_t eventIndex) {
    int hue = (int)((eventIndex * 137) % 360);
    return hsvColor565(hue, 200, 255);
}

// Small vector WiFi glyph (dot + two upward arcs, ~15px wide) drawn using
// drawCircleHelper's quadrant mask (no icon font needed). (x, y) is the
// bottom-most point (the dot). Color reflects current WiFi state: bright
// when serving, dim gray when off.
void drawWifiGlyph(int x, int y) {
    // Off state uses a dim RED tint (not plain gray) so it stays clearly
    // legible/distinct at a glance against the black background - a flat
    // dark gray was hard to make out. Keeps the same red/green convention
    // used elsewhere (STATUS_COLOR_ON/OFF).
    uint16_t color = serveModeActive ? STATUS_COLOR_ON : rgb565(120, 40, 40);
    M5.Lcd.fillCircle(x, y, 1, color);
    M5.Lcd.drawCircleHelper(x, y, 4, 3, color);  // inner arc (top-left|top-right)
    M5.Lcd.drawCircleHelper(x, y, 7, 3, color);  // outer arc
}

// Reads the AXP192 power-input status register (0x00). Bit 5 (0x20)
// indicates VBUS present - i.e. the USB cable is actually delivering power,
// which is the closest thing to a real "is USB plugged in" signal this
// board exposes (there's no separate USB-data-only detection).
bool isUsbConnected() {
    return (M5.Axp.Read8bit(0x00) & 0x20) != 0;
}

// Small plug-shaped glyph (connector body + cable stub) indicating whether
// USB is currently supplying power. Same color convention as the WiFi
// glyph: green when connected, dim red when not.
void drawUsbGlyph(int x, int y) {
    uint16_t color = isUsbConnected() ? STATUS_COLOR_ON : rgb565(120, 40, 40);
    M5.Lcd.fillRect(x - 3, y - 4, 7, 5, color);   // connector body
    M5.Lcd.drawLine(x - 2, y - 7, x - 2, y - 4, color);  // cable stub
    M5.Lcd.drawLine(x + 2, y - 7, x + 2, y - 4, color);
}

// Draws the HH:MM:SS clock (12-hour, no AM/PM text - conveyed instead via a
// subtle color: yellow for AM, blue for PM) right-justified on the
// "thresh=" line. Called periodically from loop() so seconds tick in real
// time, without touching/redrawing the rest of the screen.
// Computes the USB glyph's position (in the gap between "thresh=" and the
// clock) and draws it. Shared by redrawScreen() and the periodic refresh in
// loop(), since USB connection state can change anytime, independent of any
// full-screen redraw trigger.
// Small left margin to compensate for the physical bezel on this dev board
// partially obscuring the leftmost column of the display.
const int LEFT_MARGIN = 1;

// Small top margin, same idea as LEFT_MARGIN - the bezel obscures the very
// top pixel row. Applied to TEXT only (title, counter, thresh, history
// lines, clock) - NOT to the WiFi/USB glyphs, which stay at their existing
// position.
const int TOP_MARGIN = 1;

// Font size used just for the timestamp portion of each history line
// (normal text elsewhere is size 2) - buys back horizontal space for the
// indicator/frequency/Hall fields alongside it.
const int TIME_TEXT_SIZE = 1;

void updateUsbGlyph() {
    // Positioned on the title row, immediately left of the WiFi glyph
    // (mirrors the WiFi glyph's own positioning formula so the pair stays
    // centered as a unit in the gap between the title and the counter).
    String countStr = String(clickCount);
    int countWidth = M5.Lcd.textWidth(countStr);
    int pairCenterX =
        (M5.Lcd.width() - countWidth + M5.Lcd.textWidth("ClickLogger")) / 2;
    int usbGlyphX = pairCenterX - 9;
    int usbGlyphY = M5.Lcd.fontHeight() - 4;
    drawUsbGlyph(usbGlyphX, usbGlyphY);
}

void drawClock() {
    RTC_TimeTypeDef t;
    M5.Rtc.GetTime(&t);

    bool isPM = t.Hours >= 12;
    int hour12 = t.Hours % 12;
    if (hour12 == 0) hour12 = 12;

    char buf[10];
    snprintf(buf, sizeof(buf), "%2d:%02d:%02d", hour12, t.Minutes, t.Seconds);
    String clockStr(buf);

    uint16_t clockColor = isPM ? M5.Lcd.color565(90, 140, 255)   // PM - blue
                                : M5.Lcd.color565(255, 220, 40); // AM - yellow

    int y = M5.Lcd.fontHeight() + TOP_MARGIN;

    // No separate fillRect() clear here on purpose: setTextColor(fg, BLACK)
    // makes print() draw opaque glyph backgrounds itself in a single pass.
    // Since clockStr is always the same fixed width ("H:MM:SS"/"HH:MM:SS"),
    // this fully overwrites the previous text with no gap - a prior version
    // did fillRect(BLACK) then print() as two separate draws, which caused
    // a visible black flash every update (flicker).
    M5.Lcd.setTextColor(clockColor, BLACK);
    M5.Lcd.setCursor(M5.Lcd.width() - M5.Lcd.textWidth(clockStr), y);
    M5.Lcd.print(clockStr);
}

// Redraws the entire screen from scratch every time. Doing a full
// fillScreen() instead of clearing small rectangles avoids leftover/ghosted
// pixels when new text doesn't exactly overlap old text (different string
// lengths, font row-height mismatches, etc).
void redrawScreen() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(LEFT_MARGIN, TOP_MARGIN);
    M5.Lcd.print("ClickLogger");

    // Click counter, top-right corner (no parentheses - just the number),
    // keeps it off to the side instead of consuming its own line.
    // (Right-justified, so it's unaffected by the left margin.)
    String countStr = String(clickCount);
    int countWidth = M5.Lcd.textWidth(countStr);
    M5.Lcd.setCursor(M5.Lcd.width() - countWidth, TOP_MARGIN);
    M5.Lcd.print(countStr);

    // WiFi + USB glyphs, paired together and centered in the gap between
    // the title and the counter. WiFi sits slightly right of center, USB
    // slightly left (see updateUsbGlyph(), which mirrors this position).
    // NOTE: glyphY intentionally does NOT get TOP_MARGIN - graphics stay at
    // their existing position, only text shifts down.
    int glyphX = (M5.Lcd.width() - countWidth + M5.Lcd.textWidth("ClickLogger")) / 2 + 9;
    int glyphY = M5.Lcd.fontHeight() - 4;
    drawWifiGlyph(glyphX, glyphY);
    updateUsbGlyph();

    M5.Lcd.setCursor(LEFT_MARGIN, M5.Lcd.fontHeight() + TOP_MARGIN);
    String threshStr = "thresh=" + String(THRESH_ON);
    M5.Lcd.print(threshStr);

    // No blank spacer line here on purpose - relying on colorization (each
    // history entry has its own distinct hue) to visually separate the
    // header from the scrolling event history, instead of wasting a line
    // of vertical space.
    //
    // Using explicit setCursor() per line (instead of println()'s
    // auto-newline, which resets x to 0) so the left margin is preserved on
    // every line, not just the first.
    for (int i = 0; i < NUM_HISTORY_LINES; i++) {
        M5.Lcd.setTextColor(eventLines[i].color, BLACK);
        int rowY = (2 + i) * M5.Lcd.fontHeight() + TOP_MARGIN;

        if (clickCount == 0 && i == NUM_HISTORY_LINES - 1) {
            M5.Lcd.setCursor(LEFT_MARGIN, rowY);
            M5.Lcd.print("listening...");
            continue;
        }

        int restX = LEFT_MARGIN;
        if (eventLines[i].timePart.length() > 0) {
            // Timestamp printed at a smaller size to save horizontal room
            // for the rest of the line (indicator/freq/hall). Vertically
            // nudge it down a bit since a smaller font is shorter than the
            // row height (which is fixed at the normal font's height).
            M5.Lcd.setTextSize(TIME_TEXT_SIZE);
            int smallFontVOffset = (M5.Lcd.fontHeight() * (2 - TIME_TEXT_SIZE)) / 4;
            M5.Lcd.setCursor(LEFT_MARGIN, rowY + smallFontVOffset);
            M5.Lcd.print(eventLines[i].timePart);
            restX = LEFT_MARGIN + M5.Lcd.textWidth(eventLines[i].timePart) + 3;
            M5.Lcd.setTextSize(2);  // restore before anything else uses
                                     // fontHeight()/textWidth() at normal size
        }

        M5.Lcd.setCursor(restX, rowY);
        M5.Lcd.print(eventLines[i].restPart);
    }

    drawClock();
}

// ---------------------------------------------------------------------------
// TMAG5273 live positioning display (TMAGSTREAM/TMAGSTREAMOFF) - a
// dedicated full-screen view showing live X/Y/Z field readings as
// numbers + bipolar bar graphs, so bench positioning experiments (moving
// the sensor around the solenoid, comparing resting open/closed states)
// can be done by watching the Stick's own screen, untethered from a
// laptop serial monitor - the numeric equivalent of watching the A3144E's
// onboard LED, but with 3-axis magnitude+direction instead of a single
// on/off trigger.
//
// NOTE: if a real click event happens while this view is active,
// logClickEvent()'s redrawScreen() call will briefly replace it with the
// normal click-history view; the next periodic TMAG update then redraws
// this live view again. Not worth avoiding for a bench-positioning tool.
// ---------------------------------------------------------------------------

// Static parts (title, range) - drawn once when TMAGSTREAM turns on, not
// repeated on every update (unlike the per-axis rows, which redraw often).
void drawTmagLiveScreenStatic() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(LEFT_MARGIN, TOP_MARGIN);
    M5.Lcd.print("TMAG LIVE");
    M5.Lcd.setCursor(LEFT_MARGIN, TOP_MARGIN + M5.Lcd.fontHeight());
    M5.Lcd.printf("range +/-%.0fmT", tmagRangeMt);
}

// Draws one axis's row: a numeric readout plus a bipolar horizontal bar
// (centered = 0, extends right for positive, left for negative, scaled to
// the sensor's active +/-tmagRangeMt range). rowIndex 0/1/2 = X/Y/Z, drawn
// top-to-bottom below the static title/range lines.
void drawTmagAxisBar(int rowIndex, char axisLabel, float value) {
    int rowTop = TOP_MARGIN + (2 + rowIndex * 2) * M5.Lcd.fontHeight();

    // Numeric readout - setTextColor(fg, BLACK) makes print() opaque-erase
    // the previous value in place (same trick used elsewhere in this file
    // for the clock), so no separate fillRect() needed for the text part.
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(LEFT_MARGIN, rowTop);
    M5.Lcd.printf("%c %+6.2f", axisLabel, value);

    // Bar graph - centered horizontally, positive right/green, negative
    // left/orange. Needs an explicit clear since the bar's length (not
    // just its text content) changes between updates.
    int barY = rowTop + M5.Lcd.fontHeight() + 2;
    int barHeight = 6;
    int centerX = M5.Lcd.width() / 2;
    int halfWidth = (M5.Lcd.width() / 2) - 10;

    M5.Lcd.fillRect(0, barY, M5.Lcd.width(), barHeight, BLACK);
    M5.Lcd.drawFastVLine(centerX, barY, barHeight, DARKGREY);  // zero tick

    float clamped = value;
    if (clamped > tmagRangeMt) clamped = tmagRangeMt;
    if (clamped < -tmagRangeMt) clamped = -tmagRangeMt;
    int barLen = (int)((clamped / tmagRangeMt) * halfWidth);

    const uint16_t barColorPos = rgb565(80, 255, 120);   // green (matches
                                                           // STATUS_COLOR_ON)
    const uint16_t barColorNeg = rgb565(255, 160, 60);    // orange
    if (barLen >= 0) {
        M5.Lcd.fillRect(centerX, barY, barLen, barHeight, barColorPos);
    } else {
        M5.Lcd.fillRect(centerX + barLen, barY, -barLen, barHeight, barColorNeg);
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool initI2SMic() {
    esp_err_t err = ESP_OK;
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ALL_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 2,
        .dma_buf_len = 128,
    };

    i2s_pin_config_t pin_config = {};  // zero-init to avoid garbage in
                                        // fields we don't set below (e.g.
                                        // mck_io_num), which previously caused
                                        // "mclk config failed" on some builds.
#if (ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 3, 0))
    pin_config.mck_io_num = I2S_PIN_NO_CHANGE;
#endif
    pin_config.bck_io_num   = I2S_PIN_NO_CHANGE;
    pin_config.ws_io_num    = PIN_CLK;
    pin_config.data_out_num = I2S_PIN_NO_CHANGE;
    pin_config.data_in_num  = PIN_DATA;

    err += i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    err += i2s_set_pin(I2S_NUM_0, &pin_config);
    err += i2s_set_clk(I2S_NUM_0, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT,
                        I2S_CHANNEL_MONO);

    return err == ESP_OK;
}

// Persists across calls so zero-crossing detection works correctly at chunk
// boundaries (otherwise we'd miss/double-count the crossing that happens
// right at the edge between two consecutive reads).
// Reads one chunk and returns the peak absolute sample value - purely used
// as a threshold trigger (see 2026-09-09 decision to deprioritize acoustic
// precision, top-of-file doc comment). Used to compute a zero-crossing-rate
// pitch estimate too, but that's been removed - it never reliably
// distinguished open vs. close, and full classification now belongs to the
// TMAG5273 nearest-centroid classifier instead.
int16_t readMicChunk() {
    size_t bytesRead = 0;
    i2s_read(I2S_NUM_0, (char *)audioBuf, sizeof(audioBuf), &bytesRead,
              portMAX_DELAY);
    size_t n = bytesRead / sizeof(int16_t);
    int16_t peak = 0;
    for (size_t i = 0; i < n; i++) {
        int16_t v = audioBuf[i];
        int16_t a = (v < 0) ? -v : v;
        if (a > peak) peak = a;
    }
    return peak;
}

// Builds "YYYY-MM-DDTHH:MM:SS" from the RTC's currently cached Date/Time
// fields. Call M5.Rtc.GetDate()/GetTime() right before to refresh them.
// Used for the on-flash CSV log, where readability/parseability matters.
String isoTimestamp(RTC_DateTypeDef &d, RTC_TimeTypeDef &t) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d", d.Year, d.Month,
             d.Date, t.Hours, t.Minutes, t.Seconds);
    return String(buf);
}

// Builds "YYYYMMDDHHMMSS" (numbers only, no punctuation) - compact enough to
// fit on one line of the small on-device screen alongside the duration.
String compactTimestamp(RTC_DateTypeDef &d, RTC_TimeTypeDef &t) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d%02d", d.Year, d.Month,
             d.Date, t.Hours, t.Minutes, t.Seconds);
    return String(buf);
}

// Builds "HHMMSS" (numbers only, time-of-day only - no year, no month/day,
// no punctuation) for the on-device screen. Dropping the date entirely
// (not just the year) frees up more characters for the duration + ON/OFF
// classification label alongside it.
// Builds "MMSS.DDD" for the on-device screen: minute+second (no hours - not
// needed for this use case, and saves 2 chars), with the event's duration
// folded in as a fake decimal fraction (e.g. an 18ms event at 07:24 shows as
// "0724.018"). This is a deliberate fudge - the event didn't actually occur
// at "7.24018 minutes" - but it's a compact way to show both the time and
// duration in one field, freeing up room on the line for Hall-sensor data.
// Builds "MMSS" (minute+second only, no hours) for the small-font portion
// of the on-device screen line.
String compactMinSec(RTC_TimeTypeDef &t) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d%02d", t.Minutes, t.Seconds);
    return String(buf);
}

// Builds ".DDD" - the event's duration as a fake decimal fraction (e.g. an
// 18ms event becomes ".018"). Deliberately a fudge (this isn't really a
// fractional second), but compact - and now rendered at NORMAL font size
// since duration is an important heuristic, unlike the MMSS timestamp
// prefix (small font, see compactMinSec()).
String durationAsDecimal(uint32_t durationMs) {
    char buf[8];
    snprintf(buf, sizeof(buf), ".%03lu", (unsigned long)durationMs);
    return String(buf);
}

void logClickEvent(RTC_DateTypeDef &d, RTC_TimeTypeDef &t, uint32_t durationMs,
                    int16_t peak) {
    clickCount++;

    // TMAG5273 calibration/classification - feed this event into an
    // ongoing calibration run (no-op if none is active), then classify it
    // if a calibration has already completed. Uses the burst-derived
    // label (see burstLabel()) as ground truth during calibration - NOT
    // the plain timing prediction - so multi-solenoid rigs (master +
    // station valve staggered firing) get correctly split into their own
    // classes rather than being lumped together. Done unconditionally
    // whenever the sensor is present (not gated by TMAGON, same as the
    // existing per-axis min/max tracking) since it's cheap and useful to
    // see live even without full CSV logging enabled.
    if (tmagOk) {
        float vecX = tmagPeakSigned(tmagXMin, tmagXMax);
        float vecY = tmagPeakSigned(tmagYMin, tmagYMax);
        float vecZ = tmagPeakSigned(tmagZMin, tmagZMax);
        feedTmagCalibration(burstLabel(), vecX, vecY, vecZ);
        if (tmagCalibrated) {
            tmagClassification = classifyTmagVector(vecX, vecY, vecZ);
        }
    }

    // Diagnostic indicator: "<timing prediction>" plus an optional
    // TMAG classification field appended once a TMAG5273 calibration has
    // completed (see TMAGCAL). Timing digit: '1' = start of cycle (valve
    // opening), '0' = end of cycle (valve returning closed - normally-
    // closed valve). The TMAG field (e.g. "O0_0"/"O0_1" for a staggered-
    // open burst, "C1_0"/"C1_1" for staggered-close - see burstLabel()) is
    // the one that's actually trustworthy for classification (see
    // 2026-09-09 decision, top-of-file doc comment) - acoustic duration/
    // frequency-based guessing has been removed entirely.
    String indicator = String(currentTimingPrediction);
    if (tmagCalibrated) {
        indicator += tmagClassification;
    }

    String iso = isoTimestamp(d, t);
    String gapMsStr = (currentEventGapMs == 0xFFFFFFFF)
                           ? "NA"
                           : String(currentEventGapMs);
    String line = iso + "," + String(durationMs) + "," +
                  String(peak) + "," +
                  indicator + "," + gapMsStr + "," + burstLabel();

    // Hall-sensor correlation data (experimental, HALLON only): whether the
    // external Grove Hall Effect Unit's switch output went LOW (triggered)
    // at any point during this event - checking whether it fires
    // consistently on one click type but not the other (open vs close).
    if (hallModeOn) {
        line += "," + String(hallTriggeredEvent ? 1 : 0);
    }

    // TMAG5273 correlation data (experimental, TMAGON only): min/max field
    // reading (mT) per axis seen during this event - looking for a
    // direction/magnitude signature that distinguishes open vs close.
    if (tmagModeOn && tmagOk) {
        line += "," + String(tmagXMin, 2) + "," + String(tmagXMax, 2) + "," +
                String(tmagYMin, 2) + "," + String(tmagYMax, 2) + "," +
                String(tmagZMin, 2) + "," + String(tmagZMax, 2);
    }

    Serial.println(line);

    if (snippetModeOn) {
        saveSnippet(iso);
    }

    File f = SPIFFS.open(LOG_PATH, FILE_APPEND);
    if (f) {
        f.println(line);
        f.close();
    } else {
        Serial.println("  [WARN] could not open log file for append");
    }

    // Shift history up (oldest at index 0 drops off) and insert the new
    // event at the end (newest, rendered at the bottom). Each entry keeps
    // its own color as it moves - this is the "colors retained, only the
    // window of 3 visible lines shifts" behavior.
    for (int i = 0; i < NUM_HISTORY_LINES - 1; i++) {
        eventLines[i] = eventLines[i + 1];
    }

    // External Hall Effect Unit trigger indicator for the on-screen history
    // line: "H" if it fired (went LOW) at any point during this event,
    // "-" otherwise.
    const char *hallBuf = hallTriggeredEvent ? "H" : "-";

    eventLines[NUM_HISTORY_LINES - 1] = {
        compactMinSec(t),
        durationAsDecimal(durationMs) + " " + indicator + " " +
            burstLabel() + " " + hallBuf,
        colorForEventIndex(clickCount)};

    redrawScreen();
}

// ---------------------------------------------------------------------------
// NTP sync
// ---------------------------------------------------------------------------

// Tracks when the RTC was last successfully synced (millis()-based, so this
// resets on reboot - that's fine, we always resync at boot anyway).
uint32_t lastNtpSyncMs = 0;

// The BM8563 RTC used here is reasonably accurate (typically low
// seconds/month drift), so there's no need for a dedicated periodic WiFi
// wake-up just for NTP. Instead, this piggybacks on WiFi windows we're
// already opening for other reasons (the 5-minute auto-push, manual SERVE,
// or a button-triggered session) - gated by this interval so we're not
// doing a redundant NTP round-trip on every single wake-up.
const uint32_t NTP_RESYNC_INTERVAL_MS = 60UL * 60UL * 1000UL;  // 1 hour

// Fetches the current time via NTP and writes it to the RTC. Assumes WiFi
// is already connected. Returns true on success. No screen/serial chatter
// here (unlike the boot-time sync) - callers decide what to log/display.
bool fetchNtpAndSetRtc() {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org",
               "time.nist.gov");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10000)) {
        return false;
    }

    RTC_TimeTypeDef tt;
    tt.Hours   = timeinfo.tm_hour;
    tt.Minutes = timeinfo.tm_min;
    tt.Seconds = timeinfo.tm_sec;

    RTC_DateTypeDef dt;
    dt.WeekDay = timeinfo.tm_wday;
    dt.Month   = timeinfo.tm_mon + 1;
    dt.Date    = timeinfo.tm_mday;
    dt.Year    = timeinfo.tm_year + 1900;

    M5.Rtc.SetTime(&tt);
    M5.Rtc.SetDate(&dt);
    lastNtpSyncMs = millis();

    Serial.printf("RTC synced: %04d-%02d-%02d %02d:%02d:%02d\n", dt.Year,
                  dt.Month, dt.Date, tt.Hours, tt.Minutes, tt.Seconds);
    return true;
}

// Call whenever WiFi happens to already be connected (e.g. during a push or
// serve session) - resyncs the RTC if it's been more than
// NTP_RESYNC_INTERVAL_MS since the last successful sync, otherwise no-ops
// immediately (cheap to call opportunistically on every such wake-up).
void maybeResyncRtc() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (millis() - lastNtpSyncMs < NTP_RESYNC_INTERVAL_MS) return;

    Serial.println("Opportunistic NTP resync...");
    if (!fetchNtpAndSetRtc()) {
        Serial.println("Opportunistic NTP resync failed; will retry next "
                        "WiFi window.");
    }
}

void syncRtcFromNtp() {
    if (strlen(WIFI_SSID) == 0) {
        Serial.println("No WiFi credentials configured; skipping NTP sync.");
        return;
    }

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Connecting WiFi...");
    M5.Lcd.print("SSID: ");
    M5.Lcd.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("WiFi connect failed (status=%d); keeping existing RTC time.\n",
                      (int)WiFi.status());
        Serial.println("Status codes: 0=IDLE 1=NO_SSID_AVAIL 3=CONNECTED "
                        "4=CONNECT_FAILED 5=CONNECTION_LOST 6=DISCONNECTED");
        M5.Lcd.printf("status=%d\n", (int)WiFi.status());

        Serial.println("Nearby networks:");
        WiFi.disconnect();
        delay(100);
        int n = WiFi.scanNetworks();
        if (n <= 0) {
            Serial.println("  (none found)");
        } else {
            for (int i = 0; i < n; i++) {
                Serial.printf("  \"%s\" (RSSI %d)\n", WiFi.SSID(i).c_str(),
                              WiFi.RSSI(i));
            }
        }
        M5.Lcd.println("WiFi FAILED");
        M5.Lcd.println("using RTC time");
        delay(1500);
        WiFi.mode(WIFI_OFF);
        return;
    }

    Serial.println("WiFi connected, syncing time via NTP...");
    if (fetchNtpAndSetRtc()) {
        M5.Lcd.println("RTC synced OK");
    } else {
        Serial.println("NTP sync failed; keeping existing RTC time.");
        M5.Lcd.println("NTP FAILED");
    }

    // Done with WiFi for the rest of the session - turn it off so it doesn't
    // interfere with I2S timing or draw extra power while logging.
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(1000);
}

// ---------------------------------------------------------------------------
// On-demand HTTP server (SERVE / STOP serial commands)
// ---------------------------------------------------------------------------

void handleClicksCsvRequest() {
    File f = SPIFFS.open(LOG_PATH, FILE_READ);
    if (!f) {
        httpServer.send(404, "text/plain", "no log file yet\n");
        return;
    }
    httpServer.streamFile(f, "text/csv");
    f.close();
}

void handleSnippetsRequest() {
    File f = SPIFFS.open(SNIPPETS_PATH, FILE_READ);
    if (!f) {
        httpServer.send(404, "text/plain", "no snippets file yet\n");
        return;
    }
    httpServer.streamFile(f, "application/octet-stream");
    f.close();
}

void handleRootRequest() {
    httpServer.send(200, "text/plain",
                     "ClickLogger\nGET /clicks.csv to download the log\n");
}

// Pushes a non-click status message (e.g. an IP address or "wifi off") into
// the same scrolling history used for click events, so it shares the
// shift/color-wheel behavior instead of needing its own UI area. Does NOT
// count towards clickCount.
void pushStatusLine(const String &text, uint16_t color) {
    for (int i = 0; i < NUM_HISTORY_LINES - 1; i++) {
        eventLines[i] = eventLines[i + 1];
    }
    // No timePart for status messages (IP address, "off", etc.) - just
    // render the whole thing at normal size.
    eventLines[NUM_HISTORY_LINES - 1] = {"", text, color};
    redrawScreen();
}

// Connects to WiFi and starts the HTTP server, without touching the screen.
// Returns true on success. Shared by both the serial SERVE command and the
// front-button toggle.
bool connectWifiForServe() {
    if (strlen(WIFI_SSID) == 0) {
        Serial.println("No WiFi credentials configured; can't serve.");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("WiFi connect failed (status=%d).\n", (int)WiFi.status());
        WiFi.mode(WIFI_OFF);
        return false;
    }

    httpServer.on("/", handleRootRequest);
    httpServer.on("/clicks.csv", handleClicksCsvRequest);
    httpServer.on("/snippets.bin", handleSnippetsRequest);
    httpServer.begin();

    serveModeActive = true;
    serveModeUntilMs = millis() + SERVE_TIMEOUT_MS;
    suppressDetectionForWifiTransition();
    maybeResyncRtc();
    return true;
}

void enterServeMode() {
    Serial.println("Reconnecting WiFi for SERVE mode...");
    if (!connectWifiForServe()) {
        Serial.println("SERVE aborted.");
        return;
    }

    String ip = WiFi.localIP().toString();
    Serial.printf("SERVE mode active. curl http://%s/clicks.csv\n",
                  ip.c_str());
    Serial.println("Click detection still running (experimental). Send "
                    "STOP to end early, or it auto-stops after 5 minutes.");

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.println("SERVING");
    M5.Lcd.println(ip);
    M5.Lcd.println("/clicks.csv");
}

void exitServeMode() {
    // Final push before disconnecting, so anything logged during this SERVE
    // session (after the initial on-connect push, e.g. from button toggle-on)
    // doesn't get stranded on-device until the next trigger. WiFi is still
    // up at this point, so this reuses the existing connection.
    pushDataOverExistingWifi();

    httpServer.stop();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    serveModeActive = false;
    suppressDetectionForWifiTransition();
    Serial.println("WiFi off, click detection resumed.");
    pushStatusLine("off", STATUS_COLOR_OFF);
}

// ---------------------------------------------------------------------------
// Periodic auto-push (every PUSH_INTERVAL_MS): wakes WiFi briefly, POSTs the
// whole current /clicks.csv to PUSH_HOST/PUSH_PORT/PUSH_PATH, clears the log
// on a successful (HTTP 200) response so it doesn't get re-sent next time,
// then turns WiFi back off. Runs independently of the manual SERVE/button
// flow - if a manual serve session is already active, this cycle is simply
// skipped (retried on the next interval) rather than fighting over WiFi.
// ---------------------------------------------------------------------------
uint32_t lastPushAttemptMs = 0;

// Reads /clicks.csv (and /snippets.bin if SNIPON) and POSTs them, assuming
// WiFi is ALREADY connected - does not touch WiFi connect/disconnect state
// itself. Shared by doPeriodicPush() (which wraps this with its own
// connect/disconnect cycle) and the front-button handler (which reuses the
// WiFi connection already brought up for SERVE, without tearing it down
// afterward - the button's whole point is to leave WiFi up for querying).
void pushDataOverExistingWifi() {
    if (strlen(PUSH_HOST) == 0) {
        return;  // no push target configured
    }

    File f = SPIFFS.open(LOG_PATH, FILE_READ);
    if (!f || f.size() == 0) {
        if (f) f.close();
        return;  // nothing to send
    }
    String body;
    body.reserve(f.size());
    while (f.available()) {
        body += (char)f.read();
    }
    f.close();

    HTTPClient http;
    String url = String("http://") + PUSH_HOST + ":" + String(PUSH_PORT) +
                 PUSH_PATH;
    http.begin(url);
    http.addHeader("Content-Type", "text/csv");
    int code = http.POST(body);
    Serial.printf("Push: POST %s (%u bytes) -> %d\n", url.c_str(),
                  (unsigned)body.length(), code);
    http.end();

    if (code == 200) {
        SPIFFS.remove(LOG_PATH);
        Serial.println("Push: succeeded, log cleared.");
    } else {
        Serial.println("Push: server didn't return 200; keeping log for "
                        "retry next cycle.");
    }

    // Snippets (calibration mode only) - pushed as a raw binary blob to a
    // separate path, reusing this same WiFi connection.
    if (snippetModeOn) {
        File sf = SPIFFS.open(SNIPPETS_PATH, FILE_READ);
        if (sf && sf.size() > 0) {
            size_t sz = sf.size();
            uint8_t *buf = (uint8_t *)malloc(sz);
            if (buf) {
                sf.read(buf, sz);
                sf.close();

                HTTPClient shttp;
                String surl = String("http://") + PUSH_HOST + ":" +
                              String(PUSH_PORT) + PUSH_PATH_SNIPPETS;
                shttp.begin(surl);
                shttp.addHeader("Content-Type", "application/octet-stream");
                int scode = shttp.POST(buf, sz);
                Serial.printf("Push: POST %s (%u bytes) -> %d\n",
                              surl.c_str(), (unsigned)sz, scode);
                shttp.end();
                free(buf);

                if (scode == 200) {
                    SPIFFS.remove(SNIPPETS_PATH);
                    Serial.println("Push: snippets sent, file cleared.");
                } else {
                    Serial.println("Push: snippets server error; keeping "
                                    "file for retry.");
                }
            } else {
                sf.close();
                Serial.println("  [WARN] malloc failed for snippet push");
            }
        } else if (sf) {
            sf.close();
        }
    }
}

void doPeriodicPush() {
    if (strlen(PUSH_HOST) == 0) {
        return;  // no push target configured
    }

    // Cheap check before waking WiFi at all: anything to send?
    File f = SPIFFS.open(LOG_PATH, FILE_READ);
    bool hasData = f && f.size() > 0;
    if (f) f.close();
    if (!hasData) return;

    Serial.println("Push: connecting WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(250);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("Push: WiFi connect failed (status=%d); will retry "
                      "next cycle, log kept.\n",
                      (int)WiFi.status());
        WiFi.mode(WIFI_OFF);
        return;
    }
    suppressDetectionForWifiTransition();
    maybeResyncRtc();

    pushDataOverExistingWifi();

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    suppressDetectionForWifiTransition();
}

// Front button (also a handy ~17ms click-test event picked up by the mic
// itself): toggles WiFi/HTTP serving on/off.
//   - Turning ON: shows the IP address inline in the scrolling history (so
//     you know what to `curl`) AND immediately hurries a data push (doesn't
//     wait for PUSH_INTERVAL_MS) since WiFi is already up anyway - this
//     push does NOT disconnect WiFi afterward, unlike the regular periodic
//     push, so SERVE stays available for querying.
//   - Turning OFF: does a final push first (catches anything logged during
//     the SERVE session after the initial on-connect push, so nothing gets
//     stranded on-device), THEN disconnects and shows "off". This final
//     push also happens on serial STOP and on SERVE_TIMEOUT_MS auto-expiry
//     (see exitServeMode()) - not just the button.
void handleFrontButtonPress() {
    if (serveModeActive) {
        exitServeMode();  // toggle off
    } else {
        Serial.println("BtnA: connecting WiFi for on-demand access...");
        if (connectWifiForServe()) {
            String ip = WiFi.localIP().toString();
            Serial.printf("WiFi on via button. curl http://%s/clicks.csv\n",
                          ip.c_str());
            // Also hurries the periodic push along - no need to wait for
            // PUSH_INTERVAL_MS since WiFi is already up anyway. Doesn't
            // disconnect WiFi afterward (unlike doPeriodicPush()) - the
            // button's whole point is to leave WiFi/SERVE up for querying.
            pushDataOverExistingWifi();
            lastPushAttemptMs = millis();  // don't also fire the regular
                                            // periodic push right away
            pushStatusLine(ip, STATUS_COLOR_ON);
        } else {
            pushStatusLine("wifi fail", STATUS_COLOR_OFF);
        }
    }
}

// ---------------------------------------------------------------------------
// Serial command handling
// ---------------------------------------------------------------------------
void handleSerialCommands() {
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd.equalsIgnoreCase("DUMP")) {
        File f = SPIFFS.open(LOG_PATH, FILE_READ);
        if (!f) {
            Serial.println("(no log file yet)");
            return;
        }
        Serial.println("--- clicks.csv ---");
        while (f.available()) {
            Serial.write(f.read());
        }
        Serial.println("--- end ---");
        f.close();
    } else if (cmd.equalsIgnoreCase("CLEAR")) {
        SPIFFS.remove(LOG_PATH);
        SPIFFS.remove(SNIPPETS_PATH);
        clickCount = 0;
        resetBurstCycle();
        Serial.println("Log cleared (clicks.csv + snippets.bin); burst "
                        "cycle numbering reset too.");
    } else if (cmd.equalsIgnoreCase("CYCLESTART")) {
        resetBurstCycle();
        Serial.println("Burst cycle numbering reset - the next detected "
                        "burst will be labeled as burst 0, opening. Run "
                        "this once right before each repeated full test "
                        "cycle (e.g. right before triggering a manual "
                        "Program Run) so labels line up cycle to cycle.");
    } else if (cmd.equalsIgnoreCase("STATUS")) {
        Serial.printf("clicks logged (session): %lu\n",
                      (unsigned long)clickCount);
        Serial.printf("THRESH_ON=%d THRESH_OFF=%d\n", THRESH_ON, THRESH_OFF);
        Serial.printf("SPIFFS: used=%u total=%u\n",
                      (unsigned)SPIFFS.usedBytes(), (unsigned)SPIFFS.totalBytes());
        if (tmagOk) {
            if (tmagCalibrated) {
                Serial.printf("TMAG: calibrated, %d class(es), "
                              "no-correlation threshold=%.2fmT:\n",
                              tmagNumClasses, sqrtf(tmagRejectThresholdSq));
                for (int i = 0; i < tmagNumClasses; i++) {
                    Serial.printf("  '%s': n=%d mean=(%.2f, %.2f, %.2f)\n",
                                  tmagClassLabels[i].c_str(),
                                  tmagClassCount[i], tmagClassMean[i][0],
                                  tmagClassMean[i][1], tmagClassMean[i][2]);
                }
            } else if (tmagCalibrating) {
                Serial.printf("TMAG: calibrating, %d event(s) remaining\n",
                              tmagCalRemaining);
            } else {
                Serial.println("TMAG: not calibrated (run TMAGCAL)");
            }
        }
    } else if (cmd.startsWith("THRESH ")) {
        int v = cmd.substring(7).toInt();
        if (v > 0 && v < 32767) {
            THRESH_ON = v;
            THRESH_OFF = v / 2;
            Serial.printf("THRESH_ON=%d THRESH_OFF=%d\n", THRESH_ON, THRESH_OFF);
        }
    } else if (cmd.equalsIgnoreCase("SERVE")) {
        if (serveModeActive) {
            Serial.println("Already serving.");
        } else {
            enterServeMode();
        }
    } else if (cmd.equalsIgnoreCase("STOP")) {
        if (serveModeActive) {
            exitServeMode();
        } else {
            Serial.println("Not currently serving.");
        }
    } else if (cmd.equalsIgnoreCase("PUSHNOW")) {
        if (serveModeActive) {
            Serial.println("Can't push while a manual SERVE session is active.");
        } else {
            lastPushAttemptMs = millis();
            doPeriodicPush();
        }
    } else if (cmd.equalsIgnoreCase("SNIPON")) {
        snippetModeOn = true;
        capturingSnippet = false;
        snippetLen = 0;
        Serial.println("Snippet capture ON - raw PCM will be saved to "
                        "/snippets.bin alongside each click.");
    } else if (cmd.equalsIgnoreCase("SNIPOFF")) {
        snippetModeOn = false;
        capturingSnippet = false;
        Serial.println("Snippet capture OFF.");
    } else if (cmd.equalsIgnoreCase("HALLON")) {
        hallModeOn = true;
        Serial.printf("Hall-sensor logging ON (external unit, current=%s).\n",
                      digitalRead(PIN_HALL_DATA) == LOW ? "TRIGGERED" : "idle");
    } else if (cmd.equalsIgnoreCase("HALLOFF")) {
        hallModeOn = false;
        Serial.println("Hall-sensor logging OFF.");
    } else if (cmd.equalsIgnoreCase("TMAGON")) {
        if (!tmagOk) {
            Serial.println("TMAG5273 not detected/configured - can't enable.");
        } else {
            tmagModeOn = true;
            float x, y, z;
            if (tmagReadXYZ(x, y, z)) {
                Serial.printf("TMAG5273 logging ON (x=%.2f y=%.2f z=%.2f mT).\n",
                              x, y, z);
            } else {
                Serial.println("TMAG5273 logging ON (read failed just now).");
            }
        }
    } else if (cmd.equalsIgnoreCase("TMAGOFF")) {
        tmagModeOn = false;
        Serial.println("TMAG5273 logging OFF.");
    } else if (cmd.equalsIgnoreCase("TMAGSTREAM")) {
        if (!tmagOk) {
            Serial.println("TMAG5273 not detected/configured - can't stream.");
        } else {
            tmagStreamOn = true;
            drawTmagLiveScreenStatic();
            Serial.println("TMAG5273 live stream ON (prints X/Y/Z every "
                            "~150ms to Serial AND shows a live bar-graph "
                            "view on the Stick's own screen - for "
                            "untethered bench positioning).");
        }
    } else if (cmd.equalsIgnoreCase("TMAGSTREAMOFF")) {
        tmagStreamOn = false;
        redrawScreen();
        Serial.println("TMAG5273 live stream OFF.");
    } else if (cmd.startsWith("TMAGCAL")) {
        if (!tmagOk) {
            Serial.println("TMAG5273 not detected/configured - can't calibrate.");
        } else {
            int n = 8;  // default: ~2 full cycles' worth of events on a
                         // 2-solenoid (master+station) rig; degenerates to
                         // 8 single events (still 4 cycles) on a 1-valve rig
            String arg = cmd.substring(7);
            arg.trim();
            if (arg.length() > 0) {
                int v = arg.toInt();
                if (v > 0) n = v;
            }
            startTmagCalibration(n);
            Serial.printf("TMAG calibration started: capturing the next %d "
                          "click event(s) (using burst-derived labels as "
                          "ground truth - see burst_label in the log "
                          "format) - make sure the valve(s) are cycling "
                          "normally. Needs at least 2 distinct classes to "
                          "succeed.\n", n);
        }
    } else {
        Serial.println("Commands: DUMP, CLEAR, CYCLESTART, STATUS, THRESH <n>, SERVE, STOP, "
                        "PUSHNOW, SNIPON, SNIPOFF, HALLON, HALLOFF, TMAGON, "
                        "TMAGOFF, TMAGSTREAM, TMAGSTREAMOFF, TMAGCAL <n>");
    }
}

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------
void setup() {
    M5.begin();
    M5.Lcd.setRotation(3);
    M5.Lcd.setTextSize(2);

    // Board boots with the backlight at its AXP192 hardware default, which
    // works out to roughly 70 on the ScreenBreath(0-100) scale. Halve it.
    M5.Axp.ScreenBreath(35);

    Serial.begin(115200);
    delay(200);

    pinMode(PIN_HALL_DATA, INPUT_PULLUP);

    // Wire.begin() reconfigures GPIO32/33 for I2C regardless of the
    // pinMode() call just above (see the TMAG5273 section's NOTE) - so this
    // must run AFTER that call, not before, for the A3144E/TMAG5273 to
    // peacefully coexist in firmware even though only one is ever
    // physically plugged in at a time.
    Wire.begin(32, 33);
    tmagOk = tmagInit();
    Serial.println(tmagOk
                        ? "TMAG5273 detected and configured."
                        : "[WARN] TMAG5273 not detected/configured - "
                          "TMAGON/TMAGSTREAM will no-op.");

    if (!SPIFFS.begin(true)) {
        Serial.println("[WARN] SPIFFS mount failed");
    }

    syncRtcFromNtp();

    if (!initI2SMic()) {
        M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("MIC INIT FAILED");
        Serial.println("[FAIL] I2S mic init failed");
        while (true) delay(1000);
    }
    redrawScreen();

    Serial.println("ClickLogger ready. Commands: DUMP, CLEAR, CYCLESTART, STATUS, THRESH <n>");
}

void loop() {
    handleSerialCommands();

    uint32_t now = millis();

    M5.update();
    if (M5.BtnA.wasPressed()) {
        suppressDetectionForButtonPress();
        handleFrontButtonPress();
    }

    if (serveModeActive) {
        httpServer.handleClient();
        maybeResyncRtc();  // opportunistic, only actually syncs if due
        if (now >= serveModeUntilMs) {
            exitServeMode();
        }
        // Click detection intentionally NOT paused here anymore - testing
        // whether WiFi active + HTTP serving raises the mic's noise floor
        // or disturbs I2S timing enough to matter in practice. If it does,
        // reinstate the early `return` that used to be here.
    } else if (now - lastPushAttemptMs >= PUSH_INTERVAL_MS) {
        lastPushAttemptMs = now;
        doPeriodicPush();
    }

    if (now - lastClockUpdateMs >= 250) {
        lastClockUpdateMs = now;
        drawClock();
        updateUsbGlyph();
    }

    if (tmagStreamOn && tmagOk &&
        now - lastTmagStreamMs >= TMAG_STREAM_INTERVAL_MS) {
        lastTmagStreamMs = now;
        float x, y, z;
        if (tmagReadXYZ(x, y, z)) {
            Serial.printf("TMAG x=%.2f y=%.2f z=%.2f mT\n", x, y, z);
            drawTmagAxisBar(0, 'X', x);
            drawTmagAxisBar(1, 'Y', y);
            drawTmagAxisBar(2, 'Z', z);
        }
    }

    int16_t peak = readMicChunk();

    if (snippetModeOn) {
        if (!capturingSnippet) {
            // Keep a rolling pre-roll buffer so that once triggered, we can
            // include the moment just BEFORE threshold-crossing (the true
            // onset of the click is often a few ms before it gets loud
            // enough to cross THRESH_ON).
            memcpy(prerollBuf[prerollWriteIdx], audioBuf, sizeof(audioBuf));
            prerollWriteIdx = (prerollWriteIdx + 1) % PREROLL_CHUNKS;
        } else {
            appendToSnippet(audioBuf, CHUNK_SAMPLES);
        }
    }

    switch (state) {
        case IDLE:
            if (now >= refractoryUntil && peak >= THRESH_ON) {
                state = ACTIVE;
                eventStartMs = now;
                peakThisEvent = peak;
                quietChunkCount = 0;
                M5.Rtc.GetTime(&eventStartTime);
                M5.Rtc.GetDate(&eventStartDate);

                currentEventGapMs = (prevEventStartMs == 0)
                                        ? 0xFFFFFFFF
                                        : (now - prevEventStartMs);
                // Valve is normally-closed and opens briefly: the first
                // click after a long gap is the START of a cycle (valve
                // opening) = '1'; the second click ~20s later is the END
                // (valve returning closed) = '0'.
                currentTimingPrediction =
                    (currentEventGapMs > TIMING_GAP_THRESHOLD_MS) ? '1' : '0';

                // Burst grouping (see comment above burstLabel()): a new
                // burst starts whenever the gap since the previous event
                // exceeds BURST_GAP_MS. Its type (open vs close) comes
                // from strict alternation, not a time-threshold guess -
                // see burstLabel()/resetBurstCycle() comments for why.
                if (currentEventGapMs > BURST_GAP_MS) {
                    if (burstSeqIndex >= 0) {
                        // Not the very first burst since the last
                        // CYCLESTART/CLEAR - flip from the previous
                        // burst's type.
                        currentBurstIsOpen = !currentBurstIsOpen;
                    } else {
                        // First burst of a fresh cycle always starts open
                        // (a valve was closed; the cycle begins with
                        // something opening).
                        currentBurstIsOpen = true;
                    }
                    burstSeqIndex++;
                    currentBurstPosition = 0;
                } else {
                    currentBurstPosition++;
                }

                prevEventStartMs = now;

                hallTriggeredEvent = (digitalRead(PIN_HALL_DATA) == LOW);

                if (tmagOk) {
                    float x, y, z;
                    if (tmagReadXYZ(x, y, z)) {
                        tmagXMin = tmagXMax = x;
                        tmagYMin = tmagYMax = y;
                        tmagZMin = tmagZMax = z;
                    }
                }

                if (snippetModeOn) {
                    // Start the snippet with the pre-roll chunks (oldest
                    // first), then the current (triggering) chunk.
                    snippetLen = 0;
                    for (int i = 0; i < PREROLL_CHUNKS; i++) {
                        int idx = (prerollWriteIdx + i) % PREROLL_CHUNKS;
                        appendToSnippet(prerollBuf[idx], CHUNK_SAMPLES);
                    }
                    appendToSnippet(audioBuf, CHUNK_SAMPLES);
                    capturingSnippet = true;
                }
            }
            break;

        case ACTIVE: {
            if (peak > peakThisEvent) peakThisEvent = peak;

            if (digitalRead(PIN_HALL_DATA) == LOW) hallTriggeredEvent = true;

            if (tmagOk) {
                float x, y, z;
                if (tmagReadXYZ(x, y, z)) {
                    if (x < tmagXMin) tmagXMin = x;
                    if (x > tmagXMax) tmagXMax = x;
                    if (y < tmagYMin) tmagYMin = y;
                    if (y > tmagYMax) tmagYMax = y;
                    if (z < tmagZMin) tmagZMin = z;
                    if (z > tmagZMax) tmagZMax = z;
                }
            }

            if (peak < THRESH_OFF) {
                quietChunkCount++;
            } else {
                quietChunkCount = 0;
            }

            uint32_t elapsed = now - eventStartMs;

            if (quietChunkCount >= RELEASE_CHUNKS || elapsed > MAX_CLICK_MS) {
                if (elapsed >= MIN_CLICK_MS && elapsed <= MAX_CLICK_MS) {
                    logClickEvent(eventStartDate, eventStartTime, elapsed,
                                  peakThisEvent);
                }
                capturingSnippet = false;
                refractoryUntil = now + REFRACTORY_MS;
                state = IDLE;
            }
            break;
        }

        case REFRACTORY:
            // unused; kept for clarity/future use
            state = IDLE;
            break;
    }
}
