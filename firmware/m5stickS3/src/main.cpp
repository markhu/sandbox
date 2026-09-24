// Hello-World Wi-Fi scanner for M5Stack M5StickS3
//
// - Scans nearby Wi-Fi networks
// - Displays on the built-in LCD:
//     * Top 4 visible SSIDs (sorted by RSSI, strongest first; hidden/blank
//       SSIDs are excluded)
//     * Last 6 hex characters of the device's Wi-Fi station MAC address
//     * Board/firmware revision info reported by M5Unified
// - Re-scans every 30 seconds in a loop.
//
// Bring-up note (see BRINGUP_NOTES.md for the full investigation): the LCD
// panel on the StickS3 is only created by M5Unified's board autodetect
// *after* it successfully probes the M5PM1 PMIC over I2C (SDA=47/SCL=48,
// addr 0x6E), which is also what powers the panel rail. On this board that
// probe only succeeds reliably if the I2C bus has already been initialized
// once (via Wire.begin() on those pins) before M5.begin() runs - otherwise
// the autodetect fails silently and every M5.Display.* call becomes a
// harmless no-op (dark screen, no errors). initDisplayBus() below is that
// fix; beginWithRetry() adds a small bounded retry as extra resilience.

#include <M5Unified.h>
#include <WiFi.h>
#include <Wire.h>
#include <algorithm>
#include <vector>

namespace {

constexpr uint32_t kScanIntervalMs = 30000;
constexpr int kTopN = 4;
constexpr int kMaxBeginAttempts = 3;
constexpr uint32_t kBeginRetryDelayMs = 250;
constexpr uint8_t kI2C_SDA = 47;
constexpr uint8_t kI2C_SCL = 48;

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

  int count = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false);
  if (count <= 0) {
    return nets;
  }

  nets.reserve(count);
  for (int i = 0; i < count; ++i) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) {
      continue;  // skip hidden/blank SSIDs
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
  lcd.setTextSize(1.5);
  lcd.setCursor(0, 0);

  lcd.println("== WiFi Scan (Top 4) ==");
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

// Pre-initializes the I2C bus the M5PM1 PMIC lives on (SDA=47/SCL=48) so
// that M5Unified's board autodetect can reliably find it during
// M5.begin(). Without this, the panel-power probe fails silently and the
// LCD stays dark with no error. See file header / BRINGUP_NOTES.md.
void initDisplayBus() {
  Wire.begin(kI2C_SDA, kI2C_SCL, 100000);
  delay(50);
}

// Calls M5.begin() and retries a couple of times if the board doesn't come
// back identified as a StickS3, as extra insurance against a slow PMIC
// power-up on cold boot.
void beginWithRetry() {
  for (int attempt = 1; attempt <= kMaxBeginAttempts; ++attempt) {
    auto cfg = M5.config();
    M5.begin(cfg);

    if (M5.getBoard() == m5::board_t::board_M5StickS3) {
      return;
    }

    delay(kBeginRetryDelayMs);
  }
}

}  // namespace

void setup() {
  initDisplayBus();
  beginWithRetry();

  M5.Display.setBrightness(255);
  M5.Display.setRotation(2);  // native portrait (135x240), right-side up
  M5.Display.setTextSize(1.5);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.println("Booting...");

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
