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
//   <ISO8601 timestamp>,<duration_ms>,<peak_amplitude>,<est_freq_hz>,<classification>
//
// est_freq_hz is a zero-crossing-rate estimate of the dominant pitch of the
// click, useful alongside duration for telling apart two acoustically
// distinct events (e.g. solenoid "open" vs "close").
//
// classification is a 2-character diagnostic indicator "<timing><acoustic>":
//   - timing digit: '1' if this click started >TIMING_GAP_THRESHOLD_MS after
//     the previous one (predicted START of cycle/valve opening - valve is
//     normally-closed), '0' otherwise (predicted END of cycle/valve
//     returning closed). This has been 100% consistent across every
//     physical setup tested so far.
//   - acoustic digit: classifyClick()'s duration-based guess (see
//     CLICK_CLASSIFY_MS) - NOT reliably correlated with open/close per
//     testing so far, logged mainly so we can see, event by event, how
//     often it agrees/disagrees with the timing prediction.
//   e.g. "00" = agree (both say open); "01" = disagree (timing says open,
//   duration-based guess says close).
//
// Serial commands (type into the serial monitor, newline-terminated):
//   DUMP    - print the full contents of /clicks.csv
//   CLEAR   - erase the log file
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
//             (experimental, off by default). When on, each CSV row gains 3
//             extra columns: hall_baseline, hall_min, hall_max - theory
//             being a DC latching solenoid reverses coil current direction
//             between open/close, so the magnetic pulse polarity might
//             reliably distinguish them where acoustic features haven't.
//
// Finger-snap/noise rejection (always on, independent of HALLON): the Hall
// sensor is always sampled around each detected event; if it doesn't show
// enough deviation from the ambient baseline (HALL_ACTIVITY_THRESHOLD) to
// look like a real solenoid coil pulse, the event is discarded (logged to
// Serial as "Rejected", but not written to /clicks.csv or counted) rather
// than polluting the log - handy for e.g. a habitual finger-snap smoke-test
// on power-up, which sounds loud enough to trigger acoustically but has no
// magnetic signature.
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

static int16_t audioBuf[CHUNK_SAMPLES];

// ---------------------------------------------------------------------------
// Raw snippet capture (calibration/analysis mode) - OFF by default.
//
// Toggled via serial commands SNIPON/SNIPOFF. When on, captures a short raw
// PCM waveform around each detected click event (a couple chunks of
// pre-roll, so we catch the true onset before threshold-crossing, plus the
// event itself) and appends it to /snippets.bin for offline analysis (real
// FFT, spectrograms, etc. in Python - far richer than anything cheap enough
// to compute on-device). This is meant to be temporary: once analysis
// identifies a reliable feature, we port a lightweight version of *that*
// computation into classifyClick() and can rip this back out.
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
int zcCountEvent        = 0;  // zero-crossings accumulated during ACTIVE

// Timing-based prediction (diagnostic, to compare against the acoustic
// duration-based guess): a click starting more than TIMING_GAP_THRESHOLD_MS
// after the previous one is assumed to be the START of a new cycle - the
// valve is normally-closed and opens briefly, so this is the "opening"
// click ('1'); one arriving sooner is assumed to be the END of the current
// cycle, valve returning closed ('0'). This has been 100% consistent
// across every physical test setup so far, unlike any acoustic feature
// we've tried - logged alongside the acoustic guess so we can see, event by
// event, whether/when they agree.
const uint32_t TIMING_GAP_THRESHOLD_MS = 25000;
uint32_t prevEventStartMs = 0;      // 0 = no previous event yet this boot
char currentTimingPrediction = '0';

// ---------------------------------------------------------------------------
// Hall-effect sensor correlation (experimental, HALLON/HALLOFF) - the ESP32
// has a built-in Hall sensor (hallRead(), no extra hardware). Theory: this
// is a DC LATCHING solenoid, which reverses coil current direction to
// switch open vs close - so the magnetic pulse accompanying each actuation
// should have OPPOSITE polarity for open vs close, unlike anything we've
// found acoustically. Tracks a slow-moving baseline while idle, then
// min/max deviation from that baseline during each click event.
// ---------------------------------------------------------------------------
bool hallModeOn = false;
float hallBaseline = 0;
int hallMinEvent = 0;
int hallMaxEvent = 0;

// Minimum |deviation from baseline| required to treat an event as a real
// solenoid actuation rather than incidental noise (finger snap, tap,
// voice, etc.) that happened to be loud enough to cross THRESH_ON
// acoustically but has no accompanying magnetic pulse. Starting guess based
// on initial testing (real clicks showed 47-171 deviation vs ~20-40
// baseline noise) - may need tuning once more finger-snap samples are
// collected for comparison.
const int HALL_ACTIVITY_THRESHOLD = 30;

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
static int16_t lastSampleForZC = 0;

// Reads one chunk, returns the peak absolute sample value, and reports the
// number of zero-crossings in that chunk via zeroCrossingsOut. Zero-crossing
// rate is a cheap proxy for the dominant frequency/pitch of the sound - much
// cheaper than a full FFT, and enough to compare click "timbre" alongside
// duration (e.g. a higher-pitched vs lower-pitched click).
int16_t readMicChunk(int *zeroCrossingsOut) {
    size_t bytesRead = 0;
    i2s_read(I2S_NUM_0, (char *)audioBuf, sizeof(audioBuf), &bytesRead,
              portMAX_DELAY);
    size_t n = bytesRead / sizeof(int16_t);
    int16_t peak = 0;
    int zc = 0;
    int16_t prev = lastSampleForZC;
    for (size_t i = 0; i < n; i++) {
        int16_t v = audioBuf[i];
        int16_t a = (v < 0) ? -v : v;
        if (a > peak) peak = a;
        if ((prev < 0 && v >= 0) || (prev >= 0 && v < 0)) zc++;
        prev = v;
    }
    lastSampleForZC = prev;
    if (zeroCrossingsOut) *zeroCrossingsOut = zc;
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

// Rough heuristic distinguishing the solenoid's two click sounds by
// duration: shorter clicks are assumed to be the "0" (energize/latch)
// event, longer ones "1" (release). Adjust CLICK_CLASSIFY_MS once real
// durations for both cases are known more precisely.
const uint32_t CLICK_CLASSIFY_MS = 20;
const char *classifyClick(uint32_t durationMs) {
    return durationMs < CLICK_CLASSIFY_MS ? "0" : "1";
}

void logClickEvent(RTC_DateTypeDef &d, RTC_TimeTypeDef &t, uint32_t durationMs,
                    int16_t peak, float freqHz) {
    clickCount++;

    // Combined diagnostic indicator: "<timing prediction><acoustic guess>".
    // Timing digit: '1' = start of cycle (valve opening), '0' = end of
    // cycle (valve returning closed - normally-closed valve). E.g. "10" =
    // timing predicted opening, but duration-based guess says closing
    // (disagreement) - lets us see, per event, how often the acoustic
    // heuristic actually agrees with the (so-far much more reliable)
    // timing-based prediction.
    String indicator = String(currentTimingPrediction) + classifyClick(durationMs);

    String iso = isoTimestamp(d, t);
    String line = iso + "," + String(durationMs) + "," +
                  String(peak) + "," + String(freqHz, 0) + "," +
                  indicator;

    // Hall-sensor correlation data (experimental, HALLON only): baseline
    // (ambient field level just before this event) plus the min/max reading
    // seen during the event - deviation direction/magnitude from baseline
    // is what we're checking for a possible open-vs-close polarity signal.
    if (hallModeOn) {
        line += "," + String(hallBaseline, 0) + "," + String(hallMinEvent) +
                "," + String(hallMaxEvent);
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

    // Signed Hall-sensor peak deviation from baseline (whichever of
    // min/max swung furthest, keeping its sign) - compact single field for
    // on-screen experimentation.
    int minDev = hallMinEvent - (int)hallBaseline;
    int maxDev = hallMaxEvent - (int)hallBaseline;
    int hallPeakSigned = (abs(minDev) > abs(maxDev)) ? minDev : maxDev;
    char hallBuf[8];
    snprintf(hallBuf, sizeof(hallBuf), "%+d", hallPeakSigned);

    // Compact "X.Xk" frequency (kHz, 1 decimal).
    char freqBuf[8];
    snprintf(freqBuf, sizeof(freqBuf), "%.1fk", freqHz / 1000.0f);

    eventLines[NUM_HISTORY_LINES - 1] = {
        compactMinSec(t),
        durationAsDecimal(durationMs) + " " + indicator + " " + freqBuf +
            " " + hallBuf,
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
        Serial.println("Log cleared (clicks.csv + snippets.bin).");
    } else if (cmd.equalsIgnoreCase("STATUS")) {
        Serial.printf("clicks logged (session): %lu\n",
                      (unsigned long)clickCount);
        Serial.printf("THRESH_ON=%d THRESH_OFF=%d\n", THRESH_ON, THRESH_OFF);
        Serial.printf("SPIFFS: used=%u total=%u\n",
                      (unsigned)SPIFFS.usedBytes(), (unsigned)SPIFFS.totalBytes());
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
        hallBaseline = hallRead();
        Serial.printf("Hall-sensor capture ON (baseline=%.0f).\n", hallBaseline);
    } else if (cmd.equalsIgnoreCase("HALLOFF")) {
        hallModeOn = false;
        Serial.println("Hall-sensor capture OFF.");
    } else {
        Serial.println("Commands: DUMP, CLEAR, STATUS, THRESH <n>, SERVE, STOP, "
                        "PUSHNOW, SNIPON, SNIPOFF, HALLON, HALLOFF");
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

    hallBaseline = hallRead();  // seed it instead of starting at 0

    redrawScreen();

    Serial.println("ClickLogger ready. Commands: DUMP, CLEAR, STATUS, THRESH <n>");
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

    int chunkZC = 0;
    int16_t peak = readMicChunk(&chunkZC);

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
            // Slow-moving low-pass, tracks ambient/baseline field level (not
            // the brief actuation pulse) so we can measure deviation from it
            // during the next event. Always sampled (not just when
            // hallModeOn) since it also powers the finger-snap/noise
            // rejection filter below, which is on by default.
            hallBaseline = hallBaseline * 0.875f + hallRead() * 0.125f;

            if (now >= refractoryUntil && peak >= THRESH_ON) {
                state = ACTIVE;
                eventStartMs = now;
                peakThisEvent = peak;
                quietChunkCount = 0;
                zcCountEvent = chunkZC;
                M5.Rtc.GetTime(&eventStartTime);
                M5.Rtc.GetDate(&eventStartDate);

                uint32_t gapMs = (prevEventStartMs == 0)
                                      ? 0xFFFFFFFF
                                      : (now - prevEventStartMs);
                // Valve is normally-closed and opens briefly: the first
                // click after a long gap is the START of a cycle (valve
                // opening) = '1'; the second click ~20s later is the END
                // (valve returning closed) = '0'.
                currentTimingPrediction =
                    (gapMs > TIMING_GAP_THRESHOLD_MS) ? '1' : '0';
                prevEventStartMs = now;

                {
                    int h = hallRead();
                    hallMinEvent = h;
                    hallMaxEvent = h;
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
            zcCountEvent += chunkZC;

            {
                int h = hallRead();
                if (h < hallMinEvent) hallMinEvent = h;
                if (h > hallMaxEvent) hallMaxEvent = h;
            }

            if (peak < THRESH_OFF) {
                quietChunkCount++;
            } else {
                quietChunkCount = 0;
            }

            uint32_t elapsed = now - eventStartMs;

            if (quietChunkCount >= RELEASE_CHUNKS || elapsed > MAX_CLICK_MS) {
                if (elapsed >= MIN_CLICK_MS && elapsed <= MAX_CLICK_MS) {
                    // Finger-snap/noise rejection: a real solenoid actuation
                    // pulses current through a coil, producing a magnetic
                    // field deviation the Hall sensor can pick up; a finger
                    // snap (or voice, tap, etc.) doesn't, so it should sit
                    // near the baseline. Reject events that don't show
                    // enough Hall deviation to be a real actuation -
                    // exactly the "smoke-test snap on power-up" case.
                    int minDev = hallMinEvent - (int)hallBaseline;
                    int maxDev = hallMaxEvent - (int)hallBaseline;
                    int hallDevMag = max(abs(minDev), abs(maxDev));

                    if (hallDevMag < HALL_ACTIVITY_THRESHOLD) {
                        Serial.printf("Rejected (no Hall activity, likely "
                                      "finger-snap/noise): dur=%lums "
                                      "hall_dev=%d\n",
                                      (unsigned long)elapsed, hallDevMag);
                    } else {
                    // zero-crossings/2 = number of full cycles; divide by
                    // event duration in seconds to get an estimated
                    // dominant frequency in Hz.
                    float freqHz =
                        (elapsed > 0)
                            ? (zcCountEvent / 2.0f) * (1000.0f / elapsed)
                            : 0.0f;
                    logClickEvent(eventStartDate, eventStartTime, elapsed,
                                  peakThisEvent, freqHz);
                    }
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
