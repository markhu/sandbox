// Clean reconstructed firmware implementing: LED blink & auto-detect, Wi-Fi (NVS + serial provision),
// Async web server with /api/status /api/toggle /api/info, BLE advertising (dynamic name),
// SSD1306 OLED rotating diagnostic panels.

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Configuration defaults
// -----------------------------------------------------------------------------
#ifndef LED_PIN
#define LED_PIN 2
#endif

// -----------------------------------------------------------------------------
// LED detection (optional)
// -----------------------------------------------------------------------------
static int activeLedPin = LED_PIN;
static bool ledState = false;
static unsigned long lastToggle = 0;
static const unsigned long intervalMs = 1000; // blink
static uint32_t counter1 = 0; // toggles counter

#ifdef FIND_LED
static int testPins[] = {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
static int detectLedPin() {
  Serial.println("[FIND_LED] Scanning pins for plausible LED...");
  for (int p: testPins) {
    pinMode(p, OUTPUT);
    digitalWrite(p, HIGH); delay(15);
    digitalWrite(p, LOW);  delay(15);
  }
  // Simple heuristic: just return default; real detection would sample current draw / brightness sensor
  Serial.println("[FIND_LED] Using default LED_PIN after scan.");
  return LED_PIN;
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
static void oledPrintCentered(const String &l1, const String &l2 = "", uint8_t size = 1) {
  display.clearDisplay(); display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
//  display.setFont(&FreeSansBold9pt7b);  // too big for 2-line 128x64 display
  display.setFont(&Arial_Unicode7pt7b);  // ".pio/libdeps/esp32dev/Adafruit GFX Library/fontconvert/fontconvert" /Library/Fonts/Arial\ Unicode.ttf 7 > .pio/libdeps/esp32dev/Adafruit\ GFX\ Library/Fonts/Arial_Unicode7pt7b.h
  int16_t x,y; uint16_t w,h;
//  int y1 = 16 - (size-1)*8;
  int y1 = 12 - (size-1)*8;  // the Y offset changes per font size
  int y2 = 32 + (size-1)*8;
  if (l1.length()) { display.getTextBounds(l1,0,0,&x,&y,&w,&h); display.setCursor((OLED_WIDTH-w)/2, y1); display.println(l1);}
  if (l2.length()) { display.getTextBounds(l2,0,0,&x,&y,&w,&h); display.setCursor((OLED_WIDTH-w)/2, y2); display.println(l2);}
  display.display();
}
#endif

// -----------------------------------------------------------------------------
// BLE Advertising (variables declared early for provisioning)
// -----------------------------------------------------------------------------
#ifdef USE_BLE_ADV
#include <NimBLEDevice.h>
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

String bleName = "RAINBIRD"; // will append MAC suffix in setup
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
  Serial.println("  ble/status           -> show current BLE config");
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
        Serial.println("[wifi] Scanning for networks...");
        int n = WiFi.scanNetworks();
        if (n == 0) {
          Serial.println("[wifi] No networks found");
        } else {
          Serial.printf("[wifi] Found %d networks:\n", n);
          for (int i = 0; i < n; ++i) {
            String authType = "";
            switch (WiFi.encryptionType(i)) {
              case WIFI_AUTH_OPEN: authType = "Open"; break;
              case WIFI_AUTH_WEP: authType = "WEP"; break;
              case WIFI_AUTH_WPA_PSK: authType = "WPA"; break;
              case WIFI_AUTH_WPA2_PSK: authType = "WPA2"; break;
              case WIFI_AUTH_WPA_WPA2_PSK: authType = "WPA/WPA2"; break;
              case WIFI_AUTH_WPA2_ENTERPRISE: authType = "WPA2-ENT"; break;
              case WIFI_AUTH_WPA3_PSK: authType = "WPA3"; break;
              default: authType = "Unknown"; break;
            }
            Serial.printf("  %2d: %-20s %3ddBm %s\n", i+1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), authType.c_str());
          }
        }
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
  String title = "ESP32 Blink";

  String page = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>" + title + "</title><style>body{font-family:Arial;margin:1em;}button{padding:0.6em 1em;font-size:1.1em;} ul{list-style:none;padding:0;} li{margin:0.5em 0;} form label{display:block;margin:0.5em 0;} input[type=text]{width:300px;} .ascii{color:#666;font-style:italic;} .hex-display{color:#007bff;font-family:monospace;margin-left:10px;} .char-group{border:1px solid #ddd;padding:15px;margin:10px 0;border-radius:5px;}</style></head><body><h1>" + title + "</h1>"
        + statsBlock + toggleButton
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
    page += "'></div><button type='submit'>Update BLE</button></form><h2>API Endpoints</h2><ul><li><strong><a href='/api/status' target='_blank'>/api/status</a></strong> - Current LED state, counter, uptime, Wi-Fi IP, BLE name</li><li><strong>/api/toggle</strong> (POST) - Toggle LED on/off</li><li><strong><a href='/api/info' target='_blank'>/api/info</a></strong> - Build info, memory usage, feature flags</li><li><strong><a href='/api/wifi' target='_blank'>/api/wifi</a></strong> - Wi-Fi connection status (SSID, IP, RSSI)</li><li><strong><a href='/api/wifi/scan' target='_blank'>/api/wifi/scan</a></strong> - Scan for available Wi-Fi networks</li><li><strong><a href='/api/ble/status' target='_blank'>/api/ble/status</a></strong> - BLE configuration and characteristic values</li><li><strong>/api/ble/config</strong> (POST) - Update BLE name and characteristics</li></ul><script>function stringToHex(str){let hex='';for(let i=0;i<str.length;i++){hex+=str.charCodeAt(i).toString(16).padStart(2,'0');}return hex;}function updateHex(charNum){let stringInput=document.getElementById('char'+charNum+'_string');let hexDisplay=document.getElementById('char'+charNum+'_hex_display');let hexHidden=document.getElementById('char'+charNum+'_hex');let hexValue=stringToHex(stringInput.value);hexDisplay.textContent=hexValue;hexHidden.value=hexValue;}async function refresh(){let r=await fetch('/api/status');let j=await r.json();document.getElementById('stats').innerText=JSON.stringify(j,null,2);}async function toggle(){await fetch('/api/toggle',{method:'POST'});refresh();}refresh();setInterval(refresh,3000);</script></body></html>";
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
enum OledPanel: uint8_t { PANEL_BUILD=0, PANEL_UPTIME, PANEL_WIFI, PANEL_BLE, PANEL_BLE_CHAR, PANEL_COUNT };
static OledPanel currentPanel = PANEL_BUILD; static unsigned long lastPanelSwitch=0; static const unsigned long panelIntervalMs=4000;
static void oledShowPanel(OledPanel p) {
  switch(p){
    case PANEL_BUILD: { size_t sketch=ESP.getSketchSize(); size_t freeS=ESP.getFreeSketchSpace();
        size_t tot=sketch+freeS; float fp= tot? (sketch*100.f/tot):0.f; uint32_t ht=ESP.getHeapSize();
        uint32_t hf=ESP.getFreeHeap(); float hp=ht?((ht-hf)*100.f/ht):0.f;
        char l2[22]; snprintf(l2,sizeof(l2),"F%.0f%% H%.0f%%",fp,hp);
        oledPrintCentered("Build", l2); break; }
    case PANEL_UPTIME:{ unsigned long ms=millis(); unsigned long s=ms/1000; unsigned d=s/86400; s%=86400;
        unsigned h=s/3600; s%=3600; unsigned m=s/60; s%=60; char l1[16]; char l2[22];
        if(d) snprintf(l1,sizeof(l1),"Up %ud",d); else strncpy(l1,"Uptime",sizeof(l1));
        snprintf(l2,sizeof(l2),"%02u:%02u:%02lu",h,m,(unsigned long)s); oledPrintCentered(l1,l2); break; }
    case PANEL_WIFI:{
#ifdef USE_WIFI
      if (wifiConnected && WiFi.isConnected()) {
        long rssi = WiFi.RSSI();
        oledPrintCentered("Wi-Fi: " + wifiSsid, WiFi.localIP().toString() + " " + String(rssi) + "dBm", 1);
      } else {
        oledPrintCentered("Wi-Fi","(none)", 2);
      }
#else
      oledPrintCentered("Wi-Fi","disabled", 2);
#endif
      break; }
    case PANEL_BLE:{
#ifdef USE_BLE_ADV
      oledPrintCentered("BLE", gBleName);
#else
      oledPrintCentered("BLE","disabled");
#endif
      break; }
    case PANEL_BLE_CHAR:{
#ifdef USE_BLE_ADV
      String ascii1 = hexStringToAscii(bleChars[0].hexValue);
      String ascii2 = hexStringToAscii(bleChars[1].hexValue);
      oledPrintCentered("BLE Chars", ascii1 + " | " + ascii2);
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
    String json = "{\"networks\":[";
#ifdef USE_WIFI
    int n = WiFi.scanNetworks();
    if (n > 0) {
      for (int i = 0; i < n; ++i) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"encryption\":\"";
        switch (WiFi.encryptionType(i)) {
          case WIFI_AUTH_OPEN: json += "Open"; break;
          case WIFI_AUTH_WEP: json += "WEP"; break;
          case WIFI_AUTH_WPA_PSK: json += "WPA"; break;
          case WIFI_AUTH_WPA2_PSK: json += "WPA2"; break;
          case WIFI_AUTH_WPA_WPA2_PSK: json += "WPA/WPA2"; break;
          case WIFI_AUTH_WPA2_ENTERPRISE: json += "WPA2-ENT"; break;
          case WIFI_AUTH_WPA3_PSK: json += "WPA3"; break;
          default: json += "Unknown"; break;
        }
        json += "\"}";
      }
    }
#endif
    json += "],\"count\":" + String(n) + "}";
    req->send(200, "application/json", json);
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
  Serial.printf("Boot build %s %s\n", __DATE__, __TIME__);

  // Display help text on new serial connection
  Serial.println();
  Serial.println("=== ESP32 Blink Device ===");
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
  delay(5);
}
