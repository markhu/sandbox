// Hello-World Wi-Fi scanner for M5Stack M5StickS3
//
// - Scans nearby Wi-Fi networks
// - Displays on the built-in LCD:
//     * Top 5 SSIDs (sorted by RSSI, strongest first)
//     * Last 6 hex characters of the device's Wi-Fi station MAC address
//     * Board/firmware revision info reported by M5Unified
// - Re-scans every few seconds in a loop.
//
// NOTE on a dark-screen bring-up issue (see BRINGUP_NOTES.md for the full
// investigation): M5StickS3's LCD panel is only created by M5Unified's
// board autodetect *after* it successfully probes the M5PM1 PMIC over I2C
// (SDA=47/SCL=48, addr 0x6E) and uses it to power on the panel rail. If
// that I2C probe runs before the PMIC's bus has stabilized after a fresh
// power-up/reset, the probe silently fails, the panel is never created,
// and every subsequent M5.Display.* call is a silent no-op - no exception,
// just a permanently dark screen. We defend against this by giving the
// bus a moment to settle before M5.begin(), and retrying the whole
// begin()/autodetect sequence a few times if the board isn't recognized
// as a StickS3 on the first attempt.

#include <M5Unified.h>
#include <WiFi.h>
#include <algorithm>
#include <vector>

namespace {

constexpr uint32_t kScanIntervalMs = 8000;
constexpr int kTopN = 5;
constexpr int kMaxBeginAttempts = 5;
constexpr uint32_t kBeginRetryDelayMs = 250;
constexpr uint8_t kI2C_SDA = 47;
constexpr uint8_t kI2C_SCL = 48;
constexpr uint8_t kM5PM1_ADDR = 0x6E;

struct Network {
  String ssid;
  int32_t rssi;
};

String macLast6() {
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);  // station MAC
  char buf[7];
  snprintf(buf, sizeof(buf), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(buf);
}

std::vector<Network> scanTopNetworks(int topN) {
  std::vector<Network> nets;

  int count = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
  if (count <= 0) {
    return nets;
  }

  nets.reserve(count);
  for (int i = 0; i < count; ++i) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) {
      ssid = "<hidden>";
    }
    nets.push_back({ssid, WiFi.RSSI(i)});
  }

  std::sort(nets.begin(), nets.end(), [](const Network &a, const Network &b) {
    return a.rssi > b.rssi;
  });

  if (static_cast<int>(nets.size()) > topN) {
    nets.resize(topN);
  }

  WiFi.scanDelete();
  return nets;
}

void drawScreen(const std::vector<Network> &nets, const String &macTail,
                const String &revInfo) {
  auto &lcd = M5.Display;
  lcd.startWrite();
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextSize(1);
  lcd.setCursor(0, 0);

  lcd.println("== WiFi Scan (Top 5) ==");
  lcd.println();

  if (nets.empty()) {
    lcd.println("No networks found");
  } else {
    for (size_t i = 0; i < nets.size(); ++i) {
      lcd.printf("%d. %s\n", static_cast<int>(i + 1), nets[i].ssid.c_str());
      lcd.printf("   %d dBm\n", nets[i].rssi);
    }
  }

  lcd.println();
  lcd.printf("MAC: ..%s\n", macTail.c_str());
  lcd.println(revInfo);

  lcd.endWrite();
}

String buildRevInfo() {
  auto board = M5.getBoard();
  const char *boardName = "Unknown";
  switch (board) {
    case m5::board_t::board_M5StickS3: boardName = "StickS3"; break;
    case m5::board_t::board_M5StickCPlus2: boardName = "StickC Plus2"; break;
    case m5::board_t::board_M5StickCPlus: boardName = "StickC Plus"; break;
    case m5::board_t::board_M5StickC: boardName = "StickC"; break;
    case m5::board_t::board_M5Stack: boardName = "Stack"; break;
    case m5::board_t::board_M5StackCore2: boardName = "Core2"; break;
    case m5::board_t::board_M5StackCoreS3: boardName = "CoreS3"; break;
    case m5::board_t::board_M5AtomS3: boardName = "AtomS3"; break;
    default: boardName = "M5 (generic)"; break;
  }

  String info = "Board: ";
  info += boardName;
  info += "\nChip rev: ";
  info += String(ESP.getChipRevision());
  info += "  FW: hello-wifi-scan v1.1";
  return info;
}

// Scan I2C bus (SDA=47, SCL=48) for any responding devices, specifically checking for M5PM1 at 0x6E.
// This runs BEFORE M5.begin() to diagnose whether the PMIC is even accessible.
void scanI2CBus() {
  Serial.println("\n=== PRE-M5.BEGIN() I2C SCAN ===");

  // Initialize I2C on the StickS3's pins (may not be auto-initialized yet).
  // We use the raw Wire interface to avoid M5Unified's initialization.
#ifdef ARDUINO_VARIANT
  Wire.begin(kI2C_SDA, kI2C_SCL, 100000);  // 100 kHz, safe for PMIC probing
#else
  Wire.begin(kI2C_SDA, kI2C_SCL);
#endif

  delay(50);  // Let the bus settle

  Serial.println("Scanning SDA=47, SCL=48 for I2C devices...");
  int devicesFound = 0;
  bool m5pm1Found = false;

  for (uint8_t addr = 0x01; addr < 0x7F; ++addr) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("  Found device at 0x%02X\n", addr);
      devicesFound++;

      if (addr == kM5PM1_ADDR) {
        m5pm1Found = true;
        Serial.printf("    *** M5PM1 PMIC confirmed at 0x%02X ***\n", kM5PM1_ADDR);
      }
    }
  }

  Serial.printf("I2C Scan result: %d device(s) found\n", devicesFound);
  if (!m5pm1Found) {
    Serial.printf("  WARNING: M5PM1 (0x%02X) NOT FOUND!\n", kM5PM1_ADDR);
    Serial.println("  Possible causes:");
    Serial.println("    1. PMIC hardware defect or not soldered correctly");
    Serial.println("    2. GPIO47/48 pull-ups missing or damaged");
    Serial.println("    3. I2C bus held low by another device (short/fault)");
  } else {
    Serial.println("  M5PM1 should respond to M5.begin() autodetect.");
  }
  Serial.println();
}

// Runs M5.begin() and, if the board doesn't come back identified as a
// StickS3, tears down and retries after a short settle delay. This works
// around a PMIC-not-ready-yet race on fresh power-up (see file header).
void beginWithRetry() {
  Serial.println("\n=== M5.BEGIN() WITH RETRY ===");
  for (int attempt = 1; attempt <= kMaxBeginAttempts; ++attempt) {
    delay(kBeginRetryDelayMs);  // let the M5PM1 PMIC's I2C bus settle

    auto cfg = M5.config();
    M5.begin(cfg);

    auto boardId = M5.getBoard();
    Serial.printf("Attempt %d/%d: M5.begin() called, board ID = %d\n", attempt,
                  kMaxBeginAttempts, static_cast<int>(boardId));
    Serial.printf("  Display: %dx%d\n", M5.Display.width(), M5.Display.height());

    if (boardId == m5::board_t::board_M5StickS3) {
      Serial.printf("✓ SUCCESS: Board recognized as StickS3 on attempt %d\n",
                    attempt);
      return;
    }

    Serial.printf("✗ Board not recognized as StickS3 - retrying...\n");
  }

  Serial.println("\n!!! FAILURE AFTER MAX RETRIES !!!");
  Serial.printf("Board never identified as StickS3 (final ID: %d).\n",
                static_cast<int>(M5.getBoard()));
  Serial.println("Display panel may not be powered.");
  Serial.println("Continuing anyway - display calls will be silent no-ops.");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== M5StickS3 Wi-Fi scanner booting ===");

  scanI2CBus();    // Diagnose I2C bus before M5.begin()
  beginWithRetry();

  M5.Display.setBrightness(255);
  M5.Display.setRotation(1);
  M5.Display.setTextSize(1);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.println("Booting...");

  Serial.printf("Board id: %d, Display: %dx%d\n",
                static_cast<int>(M5.getBoard()), M5.Display.width(),
                M5.Display.height());

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
}

void loop() {
  M5.update();

  String macTail = macLast6();
  String revInfo = buildRevInfo();

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.println("Scanning Wi-Fi...");

  std::vector<Network> top = scanTopNetworks(kTopN);

  drawScreen(top, macTail, revInfo);

  delay(kScanIntervalMs);
}
