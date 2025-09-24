// ESP32 firmware

/* Features implemented:
- LED blink, Wi-Fi (NVS + serial provision), Bluetooth LE advertising, BLE scan, I2C scan
- Async web server with /api/status /api/toggle /api/info, BLE advertising (dynamic name),
- SSD1306 OLED rotating diagnostic panels.
*/

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Configuration defaults
// -----------------------------------------------------------------------------
#define BLE_PREFIX "RAINpark"
#define DEBUG_FLAG false

#ifndef LED_PIN
#define LED_PIN 2
#endif

// -----------------------------------------------------------------------------
// I2C detection (optional)
// -----------------------------------------------------------------------------
static int activeLedPin = LED_PIN;
static bool ledState = false;
static unsigned long lastToggle = 0;
static const unsigned long intervalMs = 1000; // blink
static uint32_t counter1 = 0; // toggles counter

#ifdef USE_I2C_LED
#include <Wire.h>
#include <Adafruit_LEDBackpack.h>
#ifndef I2C_LED_ADDR
#define I2C_LED_ADDR 0x70
#endif
static Adafruit_7segment matrix = Adafruit_7segment();
static bool i2cLedInitialized = false;
static unsigned long lastI2cLedUpdate = 0;
static const unsigned long i2cLedIntervalMs = 1000; // Update every second

static void initI2cLed() {
  if (!i2cLedInitialized) {
    matrix.begin(I2C_LED_ADDR);
    matrix.setBrightness(8); // Medium brightness (0-15)
    i2cLedInitialized = true;
    Serial.printf("[i2c_led] Initialized seven-segment display at 0x%02X\n", I2C_LED_ADDR);
  }
}

static void updateI2cLedUptime() {
  if (!i2cLedInitialized) return;

  unsigned long now = millis();
  if (now - lastI2cLedUpdate < i2cLedIntervalMs) return;
  lastI2cLedUpdate = now;

  // Calculate uptime in minutes and seconds
  unsigned long totalSeconds = now / 1000;
  unsigned long minutes = (totalSeconds / 60) % 100; // Limit to 99 minutes max for display
  unsigned long seconds = totalSeconds % 60;

  // Always use individual digit control for consistent formatting
  matrix.writeDigitNum(0, minutes / 10);     // First digit of minutes (0-9)
  matrix.writeDigitNum(1, minutes % 10);     // Second digit of minutes (0-9)
  matrix.writeDigitNum(3, seconds / 10);     // First digit of seconds (0-5)
  matrix.writeDigitNum(4, seconds % 10);     // Second digit of seconds (0-9)
  matrix.drawColon(true); // Enable colon separator
  matrix.writeDisplay();

  if (DEBUG_FLAG) {
    Serial.printf("[i2c_led] Uptime: %02lu:%02lu\n", minutes, seconds);
  }
}
#endif

#ifdef FIND_I2C
#include <Wire.h>
static void scanI2C() {
  byte r, address;
  int nDevices = 0;

  Serial.println("[i2c] Scanning I2C bus...");
  Wire.begin(/* SDA */ 21, /* SCL */ 22, 400000); // 400 kHz OK for short QT cables

  for(address = 1; address < 127; address++ ) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    r = Wire.endTransmission();
    if (r == 0) {
      Serial.print("[i2c] I2C device found: address 0x");
      if (address<16)
        Serial.print("0");
      Serial.println(address,HEX);
      nDevices++;
    }
    else if (r==4) {
      Serial.print("[i2c] Unknown error: address 0x");
      if (address<16)
        Serial.print("0");
      Serial.println(address,HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("[i2c] No I2C devices found");
  else
    Serial.printf("[i2c] Scan complete. Found %d device(s)\n", nDevices);
}
#endif

// -----------------------------------------------------------------------------
// OLED support
// -----------------------------------------------------------------------------
#ifdef USE_OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// #include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/Arial8pt7b.h>
#include <Fonts/Arial_Unicode7pt7b.h>
#ifndef OLED_ADDR
#define OLED_ADDR 0x3C
#endif
#ifndef OLED_WIDTH
#define OLED_WIDTH 128
#endif
#ifndef OLED_HEIGHT
#define OLED_HEIGHT 64
#endif
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static void oledPrintCentered(const String &l1, const String &l2 = "", uint8_t size = 1, const String &l3 = "") {
  display.clearDisplay(); display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
//  display.setFont(&FreeSansBold9pt7b);  // too big for 2-line 128x64 display
  display.setFont(&Arial_Unicode7pt7b);  // ".pio/libdeps/esp32dev/Adafruit GFX Library/fontconvert/fontconvert" /Library/Fonts/Arial\ Unicode.ttf 7 > .pio/libdeps/esp32dev/Adafruit\ GFX\ Library/Fonts/Arial_Unicode7pt7b.h
  int16_t x,y; uint16_t w,h;

  // Adjust Y positions based on number of lines
  int y1, y2, y3;
  if (l3.length()) {
    // Three lines: distribute evenly across 64 pixel height
    y1 = 10 - (size-1)*4;   // Top line
    y2 = 28 - (size-1)*2;  // Middle line
    y3 = 48 + (size-1)*2;  // Bottom line
  } else {
    // Two lines: use original positioning
    y1 = 12 - (size-1)*8;
    y2 = 32 + (size-1)*8;
  }

  if (l1.length()) { display.getTextBounds(l1,0,0,&x,&y,&w,&h); display.setCursor((OLED_WIDTH-w)/2, y1); display.println(l1);}
  if (l2.length()) { display.getTextBounds(l2,0,0,&x,&y,&w,&h); display.setCursor((OLED_WIDTH-w)/2, y2); display.println(l2);}
  if (l3.length()) { display.getTextBounds(l3,0,0,&x,&y,&w,&h); display.setCursor((OLED_WIDTH-w)/2, y3); display.println(l3);}
  display.display();
}
#endif

// -----------------------------------------------------------------------------
// BLE Advertising (variables declared early for provisioning)
// -----------------------------------------------------------------------------
#ifdef USE_BLE_ADV
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>
#include <NimBLEUtils.h>
#ifndef BLE_ADV_NAME
#define BLE_ADV_NAME "" // dynamic by default
#endif
static NimBLEAdvertising* pAdvertising = nullptr;
char gBleName[20];
static unsigned long lastBleUpdate = 0;
bool bleNeedsRestart = false;

struct BleCharacteristic {
  String uuid;
  String hexValue;
};

String bleName = BLE_PREFIX; // will append MAC suffix in setup
String bleServiceUuid = "12345678-1234-1234-1234-123456789ABC";
BleCharacteristic bleChars[2] = {
  {"AC9005F6-80BE-42A2-925E-A8C93049E8DA", "31342e322e3132"},  // "14.2.12"   # model number?
  {"4D41385F-3629-7E51-B387-27116C3391A3", "342e3132332e30"}  // "4.123.0"    # firmware version
};

NimBLEService *pService = nullptr;
NimBLECharacteristic *pChar1 = nullptr;
NimBLECharacteristic *pChar2 = nullptr;

// BLE Server Callbacks for detailed logging
class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        Serial.println("[BLE] Client connected");
        Serial.printf("[BLE] Connected devices: %d\n", pServer->getConnectedCount());
    }

    void onDisconnect(NimBLEServer* pServer) {
        Serial.println("[BLE] Client disconnected");
        Serial.printf("[BLE] Connected devices: %d\n", pServer->getConnectedCount());
        if (pServer->getConnectedCount() == 0) {
            Serial.println("[BLE] No clients connected, restarting advertising");
            pServer->startAdvertising();
        }
    }

    void onMTUChange(uint16_t MTU, ble_gap_conn_desc* desc) {
        Serial.printf("[BLE] MTU update event: conn_handle=%d, new_mtu=%d\n", desc->conn_handle, MTU);
        Serial.printf("[BLE] Connection details: peer_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
                     desc->peer_id_addr.val[5], desc->peer_id_addr.val[4], desc->peer_id_addr.val[3],
                     desc->peer_id_addr.val[2], desc->peer_id_addr.val[1], desc->peer_id_addr.val[0]);
    }
};

std::string hexStringToBytes(String hex) {
  std::string bytes;
  for(size_t i = 0; i < hex.length(); i += 2) {
    String byteStr = hex.substring(i, i + 2);
    char byte = strtol(byteStr.c_str(), NULL, 16);
    bytes += byte;
  }
  return bytes;
}

String hexStringToAscii(String hex) {
  String ascii = "";
  for(size_t i = 0; i < hex.length(); i += 2) {
    String byteStr = hex.substring(i, i + 2);
    char byte = strtol(byteStr.c_str(), NULL, 16);
    ascii += byte;
  }
  return ascii;
}
#endif

// -----------------------------------------------------------------------------
// Wi-Fi + NVS credentials + provisioning
// -----------------------------------------------------------------------------
#ifdef USE_WIFI
#include <WiFi.h>
#include "generated_secrets.h"
static bool wifiConnected = false; // moved up so helper functions can reference
static String wifiSsid, wifiPass;
static unsigned long lastWifiRetry = 0;
#ifdef USE_WIFI_NVS
#include <Preferences.h>
static Preferences wifiPrefs;
static bool nvsLoaded = false;
static bool loadCredsFromNvs();
static bool saveCredsToNvs(const String &s, const String &p);
static void clearCredsNvs();
#endif
#ifdef USE_WIFI_NVS
static bool loadCredsFromNvs() {
  if (!wifiPrefs.begin("wifi", true)) { Serial.println("NVS open (RO) failed"); return false; }
  String s = wifiPrefs.getString("ssid", "");
  String p = wifiPrefs.getString("pass", "");
  wifiPrefs.end();
  if (s.length() && p.length()) { Serial.printf("Loaded NVS WiFi ssid='%s' len(pass)=%d\n", s.c_str(), p.length()); wifiSsid=s; wifiPass=p; nvsLoaded=true; return true; }
  return false;
}
static bool saveCredsToNvs(const String &s, const String &p) {
  if (!wifiPrefs.begin("wifi", false)) { Serial.println("NVS open (RW) failed"); return false; }
  bool ok1 = wifiPrefs.putString("ssid", s); bool ok2 = wifiPrefs.putString("pass", p); wifiPrefs.end();
  Serial.println(ok1 && ok2 ? "Saved WiFi creds to NVS" : "Failed saving WiFi creds");
  return ok1 && ok2;
}
static void clearCredsNvs() {
  if (wifiPrefs.begin("wifi", false)) { wifiPrefs.remove("ssid"); wifiPrefs.remove("pass"); wifiPrefs.end(); }
  nvsLoaded = false; Serial.println("Cleared WiFi creds from NVS");
}
#endif // USE_WIFI_NVS
static const unsigned long wifiRetryIntervalMs = 15000;
#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS 15000UL
#endif
static void resolveWifiCredentials() {
#ifdef USE_WIFI_NVS
#ifdef CLEAR_WIFI_NVS
  clearCredsNvs();
#endif
  if (loadCredsFromNvs()) return;
#endif
  if (WIFI_HAVE_CREDS) { wifiSsid = WIFI_SSID; wifiPass = WIFI_PASS; Serial.printf("Using build-time WiFi ssid='%s'\n", wifiSsid.c_str()); }
}
static void tryConnectWifi() {
  if (!wifiSsid.length() || !wifiPass.length()) { Serial.println("No WiFi creds yet"); return; }
  Serial.printf("Connecting to '%s'...\n", wifiSsid.c_str());
  WiFi.mode(WIFI_STA); WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  unsigned long start=millis();
  while (WiFi.status()!=WL_CONNECTED && millis()-start < WIFI_CONNECT_TIMEOUT_MS) { delay(250); Serial.print('.'); }
  if (WiFi.status()==WL_CONNECTED) {
    wifiConnected = true; Serial.printf("\nWiFi OK %s\n", WiFi.localIP().toString().c_str());
#ifdef USE_OLED
    oledPrintCentered("WiFi OK", WiFi.localIP().toString());
#endif
#ifdef USE_WIFI_NVS
    if (!nvsLoaded) { saveCredsToNvs(wifiSsid, wifiPass); nvsLoaded=true; }
#endif
  } else {
    Serial.println("\nWiFi failed");
#ifdef USE_OLED
    oledPrintCentered("WiFi","Failed");
#endif
  }
}

// Compact build timestamp helper function
static String getCompactBuildTime() {
  // Parse __DATE__ (format: "Jan 19 2025") and __TIME__ (format: "12:34:56")
  const char* date = __DATE__;
  const char* time = __TIME__;

  // Month name to number mapping
  const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  int month = (strstr(months, date) - months) / 3 + 1;

  // Extract day, year from __DATE__
  int day = atoi(date + 4);
  int year = atoi(date + 7);

  // Extract hour, minute from __TIME__
  int hour = atoi(time);
  int minute = atoi(time + 3);

  // Format as YYMM.hhmm
  char compact[12];
  snprintf(compact, sizeof(compact), "%02d%02d.%02d%02d",
           year % 100, month, hour, minute);

  return String(compact);
}

static String getHumanBuildTime() {
  // Parse __DATE__ (format: "Jan 19 2025") and __TIME__ (format: "12:34:56")
  const char* date = __DATE__;
  const char* time = __TIME__;

  // Month name mapping for human-readable format
  const char* monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

  // Safe month index calculation with bounds check
  const char* monthPtr = strstr(months, date);
  int monthIndex = 0;
  if (monthPtr != nullptr) {
    monthIndex = (monthPtr - months) / 3;
    if (monthIndex < 0 || monthIndex >= 12) {
      monthIndex = 0; // Default to January if out of bounds
    }
  }

  // Extract day, year from __DATE__
  int day = atoi(date + 4);
  int year = atoi(date + 7);

  // Extract time components from __TIME__
  int hour = atoi(time);
  int minute = atoi(time + 3);
  int second = atoi(time + 6);

  // old Format: "Jan 19, 2025 at 12:34:56"
  // new Format: "20250119 12:34:56"
  char human[64];
  snprintf(human, sizeof(human), "%04d%02d%02d %02d:%02d:%02d",
           year, monthIndex + 1, day,
           hour, minute, second);

  return String(human);
}

// BLE scan helper function
#ifdef USE_BLE_ADV
static NimBLEScan* pBLEScan = nullptr;

static void performBleScan(int scanDuration = 5) { // seconds
  Serial.printf("\n[ble] Starting BLE scan for %d seconds...\n", scanDuration);

  if (!pBLEScan) {
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setActiveScan(true); // Active scan uses more power but gets more info
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);  // Less or equal to interval
  }

  Serial.printf("[ble] Scanning for %d seconds...\n", scanDuration);
  pBLEScan->start(scanDuration * 1000, false);
  delay(scanDuration * 1000);  // check scan status AFTER scanDuration seconds
  NimBLEScanResults foundDevices = pBLEScan->getResults();
  Serial.printf("[ble] Scan complete. Found %d devices:\n", foundDevices.getCount());

  // Print summary of found devices
  for (int i = 0; i < foundDevices.getCount(); i++) {
    const NimBLEAdvertisedDevice* device = foundDevices.getDevice(i);
    Serial.printf("  %d: %s (RSSI: %d)\n",
                  i + 1,
                  device->haveName() ? device->getName().c_str() : "Unknown",
                  device->getRSSI());

    // Print detailed info
    Serial.printf("[BLE] Device %d: Address=%s", i + 1, device->getAddress().toString().c_str());
    if (device->haveName()) {
      Serial.printf(" Name=%s", device->getName().c_str());
    }
    if (device->haveServiceUUID()) {
      Serial.printf(" ServiceUUID=%s", device->getServiceUUID().toString().c_str());
    }
    if (device->haveManufacturerData()) {
      std::string manufacturerData = device->getManufacturerData();
      Serial.printf(" MfrData=");
      for (int j = 0; j < manufacturerData.length(); j++) {
        Serial.printf("%02X ", (uint8_t)manufacturerData[j]);
      }
    }
    Serial.printf(" RSSI=%d\n", device->getRSSI());
  }

  pBLEScan->clearResults(); // Free memory
}
#endif

// Consolidated WiFi scan helper functions
static String getEncryptionTypeString(wifi_auth_mode_t encType) {
  switch (encType) {
    case WIFI_AUTH_OPEN: return "Open";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    default: return "Unknown";
  }
}

static void performWifiScanSerial() {
  Serial.println("[wifi] Scanning for networks...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("[wifi] No networks found");
  } else {
    Serial.printf("[wifi] Found %d networks:\n", n);
    for (int i = 0; i < n; ++i) {
      String authType = getEncryptionTypeString(WiFi.encryptionType(i));
      Serial.printf("  %2d: %-20s %3ddBm %s\n", i+1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), authType.c_str());
    }
  }
}

static String performWifiScanJson() {
  String json = "{\"networks\":[";
  int n = WiFi.scanNetworks();
  if (n > 0) {
    for (int i = 0; i < n; ++i) {
      if (i > 0) json += ",";
      json += "{";
      json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
      json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
      json += "\"encryption\":\"" + getEncryptionTypeString(WiFi.encryptionType(i)) + "\"";
      json += "}";
    }
  }
  json += "],\"count\":" + String(n) + "}";
  return json;
}

static void printHelp() {
  Serial.println();
  Serial.println("Provisioning commands:");
  Serial.println("  wifi/SSID/PASSWORD  -> set / connect (password masked)");
  Serial.println("  wifi/clear           -> erase stored credentials");
  Serial.println("  wifi/status          -> show connection status");
  Serial.println("  wifi/scan            -> scan for nearby Wi-Fi networks");
  Serial.println("  ble/name NAME        -> set BLE device name");
  Serial.println("  ble/service UUID     -> set service UUID");
  Serial.println("  ble/char1 UUID HEX   -> set char1 UUID and hex value");
  Serial.println("  ble/char2 UUID HEX   -> set char2 UUID and hex value");
  Serial.println("  ble/scan             -> scan for nearby BLE devices");
  Serial.println("  ble/status           -> show current BLE config");
#ifdef FIND_I2C
  Serial.println("  i2c/scan             -> scan for I2C devices");
#endif
  Serial.println("  exit                 -> exit provisioning mode");
  Serial.println("  quit / disconnect    -> show serial monitor exit instructions");
  Serial.println("  help / ?             -> this help");
}


#if defined(ALLOW_WIFI_PROVISION)
static String provBuf;
static bool provisioningMode = true; // start in provisioning mode
static void handleProvisioning() {
  if (!Serial.available()) return;

  while (Serial.available()) {
    char c=(char)Serial.read();
    if (c=='\r') {
      // ignore carriage return (common in CRLF) without echo
      continue;
    }
    if (c=='\b' || c==0x7F) { // handle backspace/delete
      if (provBuf.length()>0) {
        provBuf.remove(provBuf.length()-1);
        // Erase last char visually: move cursor back, print space, move back again
        Serial.print("\b \b");
      }
      continue;
    }
    if(c=='\n') {
      // Check for help command to re-enter provisioning mode
      if (!provisioningMode && (provBuf == "help" || provBuf == "?")) {
        provisioningMode = true;
        printHelp();
        provBuf=""; Serial.print("\nesp> ");
        return;
      }

      // If not in provisioning mode, ignore other commands
      if (!provisioningMode) {
        provBuf="";
        return;
      }

      if (provBuf == "help" || provBuf == "?" ) {
        printHelp();
      } else if (provBuf == "exit" || provBuf == "quit" || provBuf.indexOf("disco") >= 0) {
        provisioningMode = false;
        Serial.println();
        Serial.println("=== SERIAL MONITOR EXIT INSTRUCTIONS ===");
        Serial.println("The ESP32 cannot force-close your serial monitor.");
        Serial.println("To exit, use your serial monitor's exit method:");
        Serial.println();
        Serial.println("• PlatformIO: Press Ctrl+C");
        Serial.println("• Arduino IDE: Close the Serial Monitor window");
        Serial.println("• Terminal/Screen: Press Ctrl+A then K, or Ctrl+C");
        Serial.println("• Minicom: Press Ctrl+A then X");
        Serial.println("• PuTTY: Close the window or press Ctrl+C");
        Serial.println();
        Serial.println("Device will continue running normally.");
        Serial.println("=== END EXIT INSTRUCTIONS ===");
        Serial.println();
        Serial.println("[prov] Exiting provisioning mode. Device will continue normal operation.");
        Serial.println("       Enter 'help' or '?' to re-enter provisioning mode.");
        provBuf="";
        return;
      } else if (provBuf.startsWith("wifi/status")) {
#ifdef USE_WIFI
        if (wifiConnected && WiFi.isConnected()) {
          long rssi = WiFi.RSSI();
          Serial.printf("[wifi] Connected SSID='%s' IP=%s RSSI=%lddBm\n", wifiSsid.c_str(), WiFi.localIP().toString().c_str(), rssi);
        } else if (wifiSsid.length()) {
          Serial.printf("[wifi] Not connected (attempting) SSID='%s'\n", wifiSsid.c_str());
        } else {
          Serial.println("[wifi] No credentials set");
        }
#else
        Serial.println("[wifi] WiFi disabled at build time");
#endif
      } else if (provBuf.startsWith("wifi/scan")) {
#ifdef USE_WIFI
        performWifiScanSerial();
#else
        Serial.println("[wifi] WiFi disabled at build time");
#endif
      } else if (provBuf.startsWith("ble/status")) {
#ifdef USE_BLE_ADV
        Serial.printf("[ble] Name: %s\n", bleName.c_str());
        Serial.printf("[ble] Service: %s\n", bleServiceUuid.c_str());
        String ascii1 = hexStringToAscii(bleChars[0].hexValue);
        Serial.printf("[ble] Char1: %s \"%s\" (hex: %s)\n", bleChars[0].uuid.c_str(), ascii1.c_str(), bleChars[0].hexValue.c_str());
        String ascii2 = hexStringToAscii(bleChars[1].hexValue);
        Serial.printf("[ble] Char2: %s \"%s\" (hex: %s)\n", bleChars[1].uuid.c_str(), ascii2.c_str(), bleChars[1].hexValue.c_str());
#else
        Serial.println("[ble] BLE disabled at build time");
#endif
      } else if (provBuf.startsWith("ble/name ")) {
#ifdef USE_BLE_ADV
        bleName = provBuf.substring(9);
        bleNeedsRestart = true;
        Serial.printf("[ble] Name set to: %s\n", bleName.c_str());
#else
        Serial.println("[ble] BLE disabled at build time");
#endif
      } else if (provBuf.startsWith("ble/service ")) {
#ifdef USE_BLE_ADV
        bleServiceUuid = provBuf.substring(12);
        bleNeedsRestart = true;
        Serial.printf("[ble] Service UUID set to: %s\n", bleServiceUuid.c_str());
#else
        Serial.println("[ble] BLE disabled at build time");
#endif
      } else if (provBuf.startsWith("ble/char1 ")) {
#ifdef USE_BLE_ADV
        int space1 = provBuf.indexOf(' ', 10);
        if (space1 > 10) {
          String uuid = provBuf.substring(10, space1);
          String hex = provBuf.substring(space1 + 1);
          bleChars[0].uuid = uuid;
          bleChars[0].hexValue = hex;
          bleNeedsRestart = true;
          Serial.printf("[ble] Char1 set to: %s %s\n", uuid.c_str(), hex.c_str());
        } else {
          Serial.println("[ble] Bad format. Use ble/char1 UUID HEX");
        }
#else
        Serial.println("[ble] BLE disabled at build time");
#endif
      } else if (provBuf.startsWith("ble/char2 ")) {
#ifdef USE_BLE_ADV
        int space1 = provBuf.indexOf(' ', 10);
        if (space1 > 10) {
          String uuid = provBuf.substring(10, space1);
          String hex = provBuf.substring(space1 + 1);
          bleChars[1].uuid = uuid;
          bleChars[1].hexValue = hex;
          bleNeedsRestart = true;
          Serial.printf("[ble] Char2 set to: %s %s\n", uuid.c_str(), hex.c_str());
        } else {
          Serial.println("[ble] Bad format. Use ble/char2 UUID HEX");
        }
#else
        Serial.println("[ble] BLE disabled at build time");
#endif
      } else if (provBuf.startsWith("ble/scan")) {
#ifdef USE_BLE_ADV
        performBleScan();
#else
        Serial.println("[ble] BLE disabled at build time");
#endif
      } else if (provBuf.startsWith("i2c/scan")) {
#ifdef FIND_I2C
        scanI2C();
#else
        Serial.println("[i2c] I2C scanning disabled at build time");
#endif
      } else if (provBuf.startsWith("wifi/clear")) {
#ifdef USE_WIFI_NVS
        clearCredsNvs();
#endif
        wifiSsid=""; wifiPass=""; wifiConnected=false; Serial.println("[prov] Cleared creds");
      } else if (provBuf.startsWith("wifi/")) {
        int sep=provBuf.indexOf('/',5);
        if(sep>5){ String ssid=provBuf.substring(5,sep); String pass=provBuf.substring(sep+1); wifiSsid=ssid; wifiPass=pass; Serial.printf("[prov] Got ssid='%s' len(pass)=%d\n",ssid.c_str(),pass.length());
#ifdef USE_WIFI_NVS
          saveCredsToNvs(ssid,pass); nvsLoaded=true;
#endif
          tryConnectWifi(); }
        else Serial.println("[prov] Bad format. Use wifi/SSID/PASSWORD");
      }
      provBuf="";
      if (provisioningMode) {
        Serial.print("\nesp> ");
      }
    } else {
      if (c >= 0x20 && c < 0x7F) { // printable ASCII
        if(provBuf.length()<160) {
          int firstColon = provBuf.indexOf(':');  // Mask password chars after 2nd colon
          int secondColon = firstColon >=0 ? provBuf.indexOf(':', firstColon+1) : -1;
          bool mask = (secondColon >=0);  // after entering second colon we are in password region
          provBuf += c;
          if (mask && c != ':') Serial.print('*'); else Serial.print(c);
        }
      }
    }
  }
}
#endif // ALLOW_WIFI_PROVISION
#endif // USE_WIFI

// -----------------------------------------------------------------------------
// Async Web Server
// -----------------------------------------------------------------------------
#ifdef USE_ASYNC_WEB
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
static AsyncWebServer server(80);
static bool webStarted = false;
static String htmlPage() {
  String ip = "";
#ifdef USE_WIFI
  if (wifiConnected && WiFi.isConnected()) ip = WiFi.localIP().toString();
#endif
  String ascii1 = hexStringToAscii(bleChars[0].hexValue);
  String ascii2 = hexStringToAscii(bleChars[1].hexValue);
  // modularized HTML so we can decide not to display the JSON stats block
  String statsBlock = "";  // String statsBlock = "<div id=stats>Loading...</div>";
  // modularized HTML so we can comment out the "Toggle LED" button as-desired
  String toggleButton = ""; // String toggleButton = "<button onclick=toggle()>Toggle LED</button>";
  String title = "ESP32 WROOM";

  String page = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>" + title + "</title><style>body{font-family:Arial;margin:1em;}button{padding:0.6em 1em;font-size:1.1em;} ul{list-style:none;padding:0;} li{margin:0.5em 0;} form label{display:block;margin:0.5em 0;} input[type=text]{width:300px;} .ascii{color:#666;font-style:italic;} .hex-display{color:#007bff;font-family:monospace;margin-left:10px;} .char-group{border:1px solid #ddd;padding:15px;margin:10px 0;border-radius:5px;} .scan-section{border:1px solid #007bff;padding:20px;margin:20px 0;border-radius:8px;background:#f8f9fa;} .device-list{background:white;border:1px solid #ddd;border-radius:4px;padding:10px;margin-top:10px;max-height:300px;overflow-y:auto;} .device-item{padding:8px;border-bottom:1px solid #eee;} .device-item:last-child{border-bottom:none;} .device-name{font-weight:bold;color:#007bff;} .device-address{font-family:monospace;color:#666;font-size:0.9em;} .device-rssi{color:#28a745;font-size:0.9em;} .scan-status{margin:10px 0;font-style:italic;color:#666;} .loading{color:#007bff;} .build-info{color:#666;font-size:0.85em;margin-bottom:1em;font-style:italic;}</style></head><body><h1>" + title + "</h1>"
        + "<div class='build-info'>Build: " + getHumanBuildTime() + "</div>"
        + statsBlock + toggleButton
        + "<h2>BLE Scanner</h2><div class='scan-section'><button onclick='scanBleDevices()' id='scanBtn'>Scan for BLE Devices</button><div id='scanStatus' class='scan-status'></div><div id='bleDevices' class='device-list' style='display:none;'></div></div>"
        + "<h2>BLE Configuration</h2><form action='/api/ble/config' method='post'><label>BLE Name: <input type='text' name='ble_name' value='";
  page += bleName;
  page += "'></label><label>Service UUID: <input type='text' name='service_uuid' value='";
  page += bleServiceUuid;
  page += "'></label><div class='char-group'><h3>Characteristic 1</h3><label>UUID: <input type='text' name='char1_uuid' value='";
  page += bleChars[0].uuid;
  page += "'></label><label>String Value: <input type='text' id='char1_string' onkeyup='updateHex(1)' value='";
  page += ascii1;
  page += "'> <span class='hex-display'>Hex: <span id='char1_hex_display'>";
  page += bleChars[0].hexValue;
  page += "</span></span></label><input type='hidden' name='char1_hex' id='char1_hex' value='";
  page += bleChars[0].hexValue;
  page += "'></div><div class='char-group'><h3>Characteristic 2</h3><label>UUID: <input type='text' name='char2_uuid' value='";
  page += bleChars[1].uuid;
  page += "'></label><label>String Value: <input type='text' id='char2_string' onkeyup='updateHex(2)' value='";
  page += ascii2;
  page += "'> <span class='hex-display'>Hex: <span id='char2_hex_display'>";
  page += bleChars[1].hexValue;
  page += "\"</span></label><input type='hidden' name='char2_hex' id='char2_hex' value='";
  page += bleChars[1].hexValue;
  page += "'></div><button type='submit'>Update BLE</button></form>";
  page += "<h2>API Endpoints</h2><ul><li><strong><a href='/api/status' target='_blank'>/api/status</a></strong> - Current LED state, counter, uptime, Wi-Fi IP, BLE name</li>";
  page += "<li><strong>/api/toggle</strong> (POST) - Toggle LED on/off</li><li><strong><a href='/api/info' target='_blank'>/api/info</a></strong> - Build info, memory usage, feature flags</li>";
  page += "<li><strong><a href='/api/wifi' target='_blank'>/api/wifi</a></strong> - Wi-Fi connection status (SSID, IP, RSSI)</li><li><strong><a href='/api/wifi/scan' target='_blank'>/api/wifi/scan</a></strong> - Scan for available Wi-Fi networks</li><li><strong><a href='/api/ble/status' target='_blank'>/api/ble/status</a></strong> - BLE configuration and characteristic values</li><li><strong><a href='/api/ble/scan' target='_blank'>/api/ble/scan</a></strong> - Scan for nearby BLE devices</li><li><strong>/api/ble/config</strong> (POST) - Update BLE name and characteristics</li></ul>";
  page += "<script>function stringToHex(str){let hex='';for(let i=0;i<str.length;i++){hex+=str.charCodeAt(i).toString(16).padStart(2,'0');}return hex;}function updateHex(charNum){let stringInput=document.getElementById('char'+charNum+'_string');let hexDisplay=document.getElementById('char'+charNum+'_hex_display');let hexHidden=document.getElementById('char'+charNum+'_hex');let hexValue=stringToHex(stringInput.value);hexDisplay.textContent=hexValue;hexHidden.value=hexValue;}";
  page += "async function scanBleDevices(){const scanBtn=document.getElementById('scanBtn');const scanStatus=document.getElementById('scanStatus');const devicesList=document.getElementById('bleDevices');scanBtn.disabled=true;scanBtn.textContent='Scanning...';scanStatus.textContent='Scanning for BLE devices...';scanStatus.className='scan-status loading';devicesList.style.display='none';try{const response=await fetch('/api/ble/scan');const data=await response.json();if(data.devices && data.devices.length>0){let html='<h4>Found '+data.count+' device(s):</h4>';data.devices.forEach(device=>{html+='<div class=\"device-item\">';html+='<div class=\"device-name\">'+device.name+'</div>';html+='<div class=\"device-address\">'+device.address+'</div>';html+='<div class=\"device-rssi\">RSSI: '+device.rssi+' dBm</div>';if(device.serviceUUID){html+='<div style=\"font-size:0.8em;color:#666;\">Service: '+device.serviceUUID+'</div>';}html+='</div>';});devicesList.innerHTML=html;devicesList.style.display='block';scanStatus.textContent='Scan completed successfully.';}else{scanStatus.textContent='No BLE devices found.';devicesList.style.display='none';}}catch(error){scanStatus.textContent='Error scanning for devices: '+error.message;devicesList.style.display='none';}finally{scanBtn.disabled=false;scanBtn.textContent='Scan for BLE Devices';scanStatus.className='scan-status';}}";
  page += "async function refresh(){let r=await fetch('/api/status');let j=await r.json();document.getElementById('stats').innerText=JSON.stringify(j,null,2);}async function toggle(){await fetch('/api/toggle',{method:'POST'});refresh();}refresh();setInterval(refresh,3000);</script>";
  page += "</body></html>";
  return page;
}
static void setupWeb(); // fwd
#endif


// -----------------------------------------------------------------------------
// BLE Advertising
// -----------------------------------------------------------------------------
#ifdef USE_BLE_ADV

static void setupBle() {
  NimBLEDevice::init(bleName.c_str());
  delay(1000); // wait for BLE host to be ready

  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  Serial.println("[BLE] Server callbacks registered for detailed logging");

  strncpy(gBleName, bleName.c_str(), sizeof(gBleName) - 1);
  gBleName[sizeof(gBleName) - 1] = '\0';

  pService = pServer->createService(NimBLEUUID(bleServiceUuid.c_str()));
  pChar1 = pService->createCharacteristic(NimBLEUUID(bleChars[0].uuid.c_str()), NIMBLE_PROPERTY::READ);
  pChar1->setValue(hexStringToBytes(bleChars[0].hexValue));
  pChar2 = pService->createCharacteristic(NimBLEUUID(bleChars[1].uuid.c_str()), NIMBLE_PROPERTY::READ);
  pChar2->setValue(hexStringToBytes(bleChars[1].hexValue));
  pService->start();

  pAdvertising = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData adv;
  adv.setName(gBleName);
  adv.setManufacturerData(std::string("LED=") + (ledState ? "1" : "0"));
  pAdvertising->setAdvertisementData(adv);
  pAdvertising->start();
  Serial.printf("BLE advertising name=%s\n", gBleName);
}

static void updateBleAdv() {
  if(!pAdvertising) return;
  unsigned long now = millis();
  if(now - lastBleUpdate < 2000) return;
  lastBleUpdate = now;
  NimBLEAdvertisementData adv;
  adv.setName(gBleName);
  adv.setManufacturerData(std::string("LED=") + (ledState ? "1" : "0"));
  pAdvertising->setAdvertisementData(adv);
}
#endif

// -----------------------------------------------------------------------------
// OLED Panels (must follow BLE so gBleName exists)
// -----------------------------------------------------------------------------
#ifdef USE_OLED
enum OledPanel: uint8_t { PANEL_BUILD=0, PANEL_WIFI, PANEL_BLE, PANEL_COUNT };
static OledPanel currentPanel = PANEL_BUILD;
static unsigned long lastPanelSwitch=0; static const unsigned long panelIntervalMs=4000;
static void oledShowPanel(OledPanel p) {
  switch(p){
    case PANEL_BUILD: { size_t sketch=ESP.getSketchSize(); size_t freeS=ESP.getFreeSketchSpace();
        size_t tot=sketch+freeS; float fp= tot? (sketch*100.f/tot):0.f; uint32_t ht=ESP.getHeapSize();
        uint32_t hf=ESP.getFreeHeap(); float hp=ht?((ht-hf)*100.f/ht):0.f;
        char l3[22]; snprintf(l3,sizeof(l3),"F%.0f%% H%.0f%%",fp,hp);

        unsigned long ms=millis(); unsigned long s=ms/1000; unsigned d=s/86400; s%=86400;
        unsigned h=s/3600; s%=3600; unsigned m=s/60; s%=60; char l1[16]; char l2[22];
        if(d) snprintf(l1,sizeof(l1),"Up: %ud",d);
        else strncpy(l1,"Uptime: ",sizeof(l1));
        snprintf(l2,sizeof(l2),"%02u:%02u:%02lu",h,m,(unsigned long)s);
        // oledPrintCentered(l1,l2);  // just print uptime
        oledPrintCentered(
            String(l1) + String(l2),  // uptime
            l3,  // memory/flash usage
            1,  // fontsize scale
            getHumanBuildTime()   // print build date/time
            // getCompactBuildTime()   // print build time
          );
        break;
      }
    case PANEL_WIFI:{
#ifdef USE_WIFI
      if (wifiConnected && WiFi.isConnected()) {
        long rssi = WiFi.RSSI();
        oledPrintCentered("Wi-Fi: " + wifiSsid,WiFi.localIP().toString(), 1, String(rssi) + "dBm");
      } else {
        oledPrintCentered("Wi-Fi","(none)", 1);
      }
#else
      oledPrintCentered("Wi-Fi","disabled", 1);
#endif
      break; }
    case PANEL_BLE:{
#ifdef USE_BLE_ADV
      String ascii1 = hexStringToAscii(bleChars[0].hexValue);
      String ascii2 = hexStringToAscii(bleChars[1].hexValue);
      oledPrintCentered("BLE", String(gBleName), 1, ascii1 + " | " + ascii2);
#else
      oledPrintCentered("BLE","disabled");
#endif
      break; }
    default: break; }
}
static void oledRotatePanels(){ unsigned long now=millis(); if(now-lastPanelSwitch>=panelIntervalMs){ lastPanelSwitch=now; currentPanel=(OledPanel)((currentPanel+1)%PANEL_COUNT); oledShowPanel(currentPanel);} }
#endif

// -----------------------------------------------------------------------------
// Web server implementation (after helper pieces)
// -----------------------------------------------------------------------------
#ifdef USE_ASYNC_WEB
static void setupWeb(){ if(webStarted) return; Serial.println("Starting Async Web server...");
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send(200,"text/html", htmlPage()); });
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req){
    String ip="";
#ifdef USE_WIFI
    if (wifiConnected && WiFi.isConnected()) ip=WiFi.localIP().toString();
#endif
    String json="{"; json += "\"led\":" + String(ledState?1:0) + ",";
    json += "\"counter\":" + String(counter1) + ",";
    json += "\"uptime_ms\":" + String(millis()) + ",";
    if(ip.length()) json += "\"wifi_ip\":\""+ip+"\",";
#ifdef USE_BLE_ADV
    json += "\"ble_name\":\"" + String(gBleName) + "\",";
#endif
    if(json.endsWith(",")) json.remove(json.length()-1); json += '}'; req->send(200,"application/json",json);
  });
  server.on("/api/toggle", HTTP_POST, [](AsyncWebServerRequest *req){ ledState=!ledState; digitalWrite(activeLedPin, ledState?HIGH:LOW); counter1++; req->send(204); });
  server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *req){
    size_t sketch=ESP.getSketchSize(); size_t freeS=ESP.getFreeSketchSpace(); size_t flashTotal=sketch+freeS; float flashPct=flashTotal?(sketch*100.f/flashTotal):0.f; uint32_t heapTotal=ESP.getHeapSize(); uint32_t heapFree=ESP.getFreeHeap(); float heapPct=heapTotal?((heapTotal-heapFree)*100.f/heapTotal):0.f;
    String ip="";
#ifdef USE_WIFI
    if (wifiConnected && WiFi.isConnected()) ip=WiFi.localIP().toString();
#endif
    String json="{";
    json += "\"build_date\":\"" __DATE__ "\",\"build_time\":\"" __TIME__ "\",";
    json += "\"uptime_ms\":" + String(millis()) + ",";
    json += "\"flash_used\":" + String(sketch) + ",\"flash_total\":" + String(flashTotal) + ",";
    json += "\"heap_used\":" + String(heapTotal-heapFree) + ",\"heap_total\":" + String(heapTotal) + ",";
    if(ip.length()) json += "\"wifi_ip\":\""+ip+"\",";
#ifdef USE_BLE_ADV
    json += "\"ble_name\":\"" + String(gBleName) + "\",";
#endif
    // feature flags array
    json += "\"features\":[";
#define FEAT_ADD(flag,name) do{ json += String(flag?"{\\\"name\\\":\\\"":""); json += String(flag?name:""); if(flag) json += "\\\"}"; if(flag) json += ","; }while(0)
    // Instead of above macro complexity, just append flags sequentially
    json.remove(json.length()-0); // no-op placeholder
    bool first=true; auto addFeat=[&](bool en,const char* n){ if(!en) return; if(!first) json+=","; json+='{'; json += "\"name\":\""; json += n; json += "\"}"; first=false;};
    addFeat(true,"CORE");
#ifdef FIND_LED
    addFeat(true,"FIND_LED");
#endif
#ifdef USE_OLED
    addFeat(true,"OLED");
#endif
#ifdef USE_WIFI
    addFeat(true,"WIFI");
#endif
#ifdef USE_WIFI_NVS
    addFeat(true,"WIFI_NVS");
#endif
#ifdef ALLOW_WIFI_PROVISION
    addFeat(true,"PROVISION");
#endif
#ifdef USE_ASYNC_WEB
    addFeat(true,"ASYNC_WEB");
#endif
#ifdef USE_BLE_ADV
    addFeat(true,"BLE_ADV");
#endif
    json += "],";
    json += "\"flash_pct\":" + String(flashPct,1) + ",";
    json += "\"heap_pct\":" + String(heapPct,1);
    json += '}';
    req->send(200,"application/json",json);
  });
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *req){
#ifdef USE_WIFI
    String json = performWifiScanJson();
    req->send(200, "application/json", json);
#else
    req->send(400, "application/json", "{\"error\":\"WiFi not enabled\"}");
#endif
  });
  server.on("/api/wifi", HTTP_GET, [](AsyncWebServerRequest *req){
    String json="{";
#ifdef USE_WIFI
    if (wifiConnected && WiFi.isConnected()) {
      long rssi = WiFi.RSSI();
      json += "\"connected\":true,\"ssid\":\"" + wifiSsid + "\",\"ip\":\"" + WiFi.localIP().toString() + "\",\"rssi\":" + String(rssi);
    } else if (wifiSsid.length()) {
      json += "\"connected\":false,\"ssid\":\"" + wifiSsid + "\",\"attempting\":true";
    } else {
      json += "\"connected\":false,\"credentials\":false";
    }
#else
    json += "\"wifi_disabled\":true";
#endif
    json += '}';
    req->send(200,"application/json",json);
  });
  server.on("/api/ble/status", HTTP_GET, [](AsyncWebServerRequest *req){
#ifdef USE_BLE_ADV
    String json = "{";
    json += "\"ble_name\":\"" + String(gBleName) + "\",";
    json += "\"service_uuid\":\"" + bleServiceUuid + "\",";
    json += "\"char1\":{";
    json += "\"uuid\":\"" + bleChars[0].uuid + "\",";
    json += "\"hex_value\":\"" + bleChars[0].hexValue + "\",";
    json += "\"ascii_value\":\"" + hexStringToAscii(bleChars[0].hexValue) + "\"";
    json += "},";
    json += "\"char2\":{";
    json += "\"uuid\":\"" + bleChars[1].uuid + "\",";
    json += "\"hex_value\":\"" + bleChars[1].hexValue + "\",";
    json += "\"ascii_value\":\"" + hexStringToAscii(bleChars[1].hexValue) + "\"";
    json += "}";
    json += "}";
    req->send(200, "application/json", json);
#else
    req->send(400, "application/json", "{\"error\":\"BLE not enabled\"}");
#endif
  });
  server.on("/api/ble/scan", HTTP_GET, [](AsyncWebServerRequest *req){
#ifdef USE_BLE_ADV
    Serial.println("[ble] Web API scan started");
    unsigned long scanStartTime = millis();

    // Start BLE scan (synchronous blocking scan)
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    // Perform synchronous 10-second scan (this version blocks but works)
    int scanDuration = 10;
    Serial.printf("[ble] Scanning for %d seconds...\n", scanDuration);
    pBLEScan->start(scanDuration, true);  // true = blocking scan

    // Get scan results after blocking scan completes
    unsigned long scanEndTime = millis();
    unsigned long actualResponseTime = scanEndTime - scanStartTime;

    NimBLEScanResults foundDevices = pBLEScan->getResults();
    Serial.printf("[ble] Web API scan completed, found %d devices in %lu ms\n", foundDevices.getCount(), actualResponseTime);

    // Build response as structured data object
    struct BleDevice {
      String name;
      String address;
      int rssi;
      String serviceUUID;
      bool hasServiceUUID;
    };

    struct BleResponse {
      BleDevice devices[50]; // Reasonable limit for BLE devices
      int deviceCount;
      int requestedScanDuration;
      unsigned long actualResponseTime;
      String status;
    } response;

    // Initialize response structure
    response.deviceCount = foundDevices.getCount();
    response.requestedScanDuration = scanDuration;
    response.actualResponseTime = actualResponseTime;
    response.status = "success";

    // Populate device data
    int maxDevices = min(foundDevices.getCount(), 50);
    for (int i = 0; i < maxDevices; i++) {
      const NimBLEAdvertisedDevice* device = foundDevices.getDevice(i);
      response.devices[i].name = device->haveName() ? device->getName().c_str() : "Unknown";
      response.devices[i].address = device->getAddress().toString().c_str();
      response.devices[i].rssi = device->getRSSI();
      response.devices[i].hasServiceUUID = device->haveServiceUUID();
      if (response.devices[i].hasServiceUUID) {
        response.devices[i].serviceUUID = device->getServiceUUID().toString().c_str();
      }
    }

    // Transform structured response to JSON
    String json = "{";
    json += "\"devices\":[";
    for (int i = 0; i < response.deviceCount && i < 50; i++) {
      if (i > 0) json += ",";
      json += "{";
      json += "\"name\":\"" + response.devices[i].name + "\",";
      json += "\"address\":\"" + response.devices[i].address + "\",";
      json += "\"rssi\":" + String(response.devices[i].rssi);
      if (response.devices[i].hasServiceUUID) {
        json += ",\"serviceUUID\":\"" + response.devices[i].serviceUUID + "\"";
      }
      json += "}";
    }
    json += "],";
    json += "\"count\":" + String(response.deviceCount) + ",";
    json += "\"metadata\":{";
    json += "\"requestedScanDuration\":" + String(response.requestedScanDuration) + ",";
    json += "\"actualResponseTime\":" + String(response.actualResponseTime) + ",";
    json += "\"status\":\"" + response.status + "\"";
    json += "}}";

    pBLEScan->clearResults(); // Clear after we've processed them
    req->send(200, "application/json", json);
#else
    req->send(400, "application/json", "{\"error\":\"BLE not enabled\"}");
#endif
  });
  server.on("/api/ble/config", HTTP_POST, [](AsyncWebServerRequest *req){
#ifdef USE_BLE_ADV
    if (req->hasParam("ble_name", true)) {
      bleName = req->getParam("ble_name", true)->value();
    }
    if (req->hasParam("service_uuid", true)) {
      bleServiceUuid = req->getParam("service_uuid", true)->value();
    }
    if (req->hasParam("char1_uuid", true)) {
      bleChars[0].uuid = req->getParam("char1_uuid", true)->value();
    }
    if (req->hasParam("char1_hex", true)) {
      bleChars[0].hexValue = req->getParam("char1_hex", true)->value();
    }
    if (req->hasParam("char2_uuid", true)) {
      bleChars[1].uuid = req->getParam("char2_uuid", true)->value();
    }
    if (req->hasParam("char2_hex", true)) {
      bleChars[1].hexValue = req->getParam("char2_hex", true)->value();
    }
    // Restart BLE with new config
    bleNeedsRestart = true;

    // Send HTML response with return button
    String response = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>BLE Config Updated</title>";
    response += "<style>body{font-family:Arial;margin:2em;text-align:center;}";
    response += ".success{color:#28a745;font-size:1.2em;margin:1em 0;}";
    response += "button{padding:0.8em 1.5em;font-size:1.1em;background:#007bff;color:white;border:none;border-radius:4px;cursor:pointer;}";
    response += "button:hover{background:#0056b3;}</style></head><body>";
    response += "<h1>✓ BLE Configuration Updated</h1>";
    response += "<div class='success'>BLE settings have been successfully updated!</div>";
    response += "<p>Updated parameters:</p><ul style='text-align:left;display:inline-block;'>";
    response += "<li><strong>BLE Name:</strong> " + bleName + "</li>";
    response += "<li><strong>Service UUID:</strong> " + bleServiceUuid + "</li>";
    response += "<li><strong>Char1 UUID:</strong> " + bleChars[0].uuid + "</li>";
    response += "<li><strong>Char1 Hex:</strong> " + bleChars[0].hexValue + "</li>";
    response += "<li><strong>Char2 UUID:</strong> " + bleChars[1].uuid + "</li>";
    response += "<li><strong>Char2 Hex:</strong> " + bleChars[1].hexValue + "</li></ul>";
    response += "<button onclick=\"window.location.href='/'\">Return to Main Page</button>";
    response += "</body></html>";
    req->send(200, "text/html", response);
#else
    req->send(400, "text/plain", "BLE not enabled");
#endif
  });
  server.onNotFound([](AsyncWebServerRequest *r){ r->send(404,"text/plain","Not found"); });
  server.begin(); webStarted=true; Serial.println("Async Web server started");
}
#endif

// -----------------------------------------------------------------------------
// Setup & Loop
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200); while(!Serial && millis()<2000) {}
  Serial.printf("Boot build: %s\n", getHumanBuildTime().c_str());

  // Display help text on new serial connection
  Serial.println();
  Serial.println("=== ESP32 WROOM ===");
  Serial.println("Serial monitor connected successfully!");
  Serial.println();
  Serial.println("Available commands:");
  Serial.println("  help / ?            -> show this help text");
  Serial.println("  quit / disconnect   -> show serial monitor exit instructions");
#ifdef ALLOW_WIFI_PROVISION
  Serial.println("  wifi/SSID/PASSWORD  -> set WiFi credentials");
  Serial.println("  wifi/clear          -> clear stored credentials");
  Serial.println("  wifi/status         -> show connection status");
  Serial.println("  wifi/scan           -> scan for networks");
#endif
#ifdef USE_BLE_ADV
  Serial.println("  ble/name NAME       -> set BLE device name");
  Serial.println("  ble/service UUID    -> set service UUID");
  Serial.println("  ble/status          -> show BLE configuration");
#endif
#ifdef FIND_I2C
  Serial.println("  i2c/scan            -> scan for I2C devices");
#endif
  Serial.println();
  Serial.println("Device features:");
#ifdef USE_WIFI
  Serial.println("  ✓ WiFi networking");
#endif
#ifdef USE_BLE_ADV
  Serial.println("  ✓ Bluetooth LE advertising");
#endif
#ifdef USE_OLED
  Serial.println("  ✓ OLED display");
#endif
#ifdef USE_I2C_LED
  Serial.println("  ✓ Seven-segment display");
#endif
#ifdef USE_ASYNC_WEB
  Serial.println("  ✓ Web interface");
#endif
  Serial.println("  ✓ LED control");
  Serial.println();

  // Set BLE name with MAC suffix
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String macStr = "";
  for(int i = 0; i < 6; i++) {
    if(mac[i] < 16) macStr += "0";
    macStr += String(mac[i], HEX);
  }
  macStr.toUpperCase();
  String suffix = macStr.substring(6); // last 6 characters
  bleName += suffix;

#ifdef FIND_LED
  activeLedPin = detectLedPin();
#endif
  pinMode(activeLedPin, OUTPUT); digitalWrite(activeLedPin, LOW);

#ifdef USE_OLED
  Wire.begin(); if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { Serial.println("OLED init failed"); } else { oledPrintCentered("Boot", String(__DATE__)); }
#endif

#ifdef USE_WIFI
  resolveWifiCredentials();
  tryConnectWifi();
#if defined(ALLOW_WIFI_PROVISION)
  Serial.println();
  Serial.println("Provisioning commands: (Enter 'help' or '?' for full list)");
  Serial.println("  wifi/SSID/PASSWORD  -> set / connect (password masked)");
  Serial.println("  wifi/clear           -> erase stored credentials");
  Serial.println();
  Serial.print("esp> ");
#endif
#endif

#ifdef USE_BLE_ADV
  setupBle();
#endif

#ifdef USE_ASYNC_WEB
  if (wifiConnected) setupWeb(); else Serial.println("(defer) web until WiFi");
#endif

#ifdef USE_OLED
  oledShowPanel(PANEL_BUILD);
#endif

#ifdef USE_I2C_LED
  initI2cLed();
#endif

#ifdef FIND_I2C
  scanI2C();
#endif
}

void loop() {
  unsigned long now=millis();
  if(now-lastToggle>=intervalMs){ lastToggle=now; ledState=!ledState; digitalWrite(activeLedPin, ledState?HIGH:LOW); counter1++; }
#if defined(USE_WIFI) && defined(ALLOW_WIFI_PROVISION)
  if (Serial.available()) handleProvisioning();
  if(!wifiConnected && wifiSsid.length() && wifiPass.length() && (now-lastWifiRetry>2000)) { lastWifiRetry=now; tryConnectWifi(); }
#endif
#ifdef USE_WIFI
  if(!wifiConnected && wifiSsid.length() && wifiPass.length() && (now-lastWifiRetry>wifiRetryIntervalMs)) { lastWifiRetry=now; Serial.println("Retry WiFi..."); tryConnectWifi(); }
#ifdef USE_ASYNC_WEB
  if(wifiConnected && !webStarted) setupWeb();
#endif
#endif
#ifdef USE_BLE_ADV
  updateBleAdv();
  if (bleNeedsRestart) {
    NimBLEDevice::deinit();
    delay(100);
    setupBle();
    bleNeedsRestart = false;
  }
#endif
#ifdef USE_OLED
  oledRotatePanels();
#endif
#ifdef USE_I2C_LED
  updateI2cLedUptime();
#endif
  delay(5);
}
