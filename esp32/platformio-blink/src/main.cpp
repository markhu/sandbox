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
#if defined(ALLOW_WIFI_PROVISION)
static String provBuf;
static void handleProvisioning() {
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
      if (provBuf == "help" || provBuf == "?" ) {
        Serial.println();
        Serial.println("Provisioning commands:");
        Serial.println("  wifi:SSID:PASSWORD  -> set / connect (password masked)");
        Serial.println("  wifi:clear           -> erase stored credentials");
        Serial.println("  wifi:status          -> show connection status");
        Serial.println("  help / ?             -> this help");
      } else if (provBuf.startsWith("wifi:status")) {
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
      } else if (provBuf.startsWith("wifi:clear")) {
#ifdef USE_WIFI_NVS
        clearCredsNvs();
#endif
        wifiSsid=""; wifiPass=""; wifiConnected=false; Serial.println("[prov] Cleared creds");
      } else if (provBuf.startsWith("wifi:")) {
        int sep=provBuf.indexOf(':',5);
        if(sep>5){ String ssid=provBuf.substring(5,sep); String pass=provBuf.substring(sep+1); wifiSsid=ssid; wifiPass=pass; Serial.printf("[prov] Got ssid='%s' len(pass)=%d\n",ssid.c_str(),pass.length());
#ifdef USE_WIFI_NVS
          saveCredsToNvs(ssid,pass); nvsLoaded=true;
#endif
          tryConnectWifi(); }
        else Serial.println("[prov] Bad format. Use wifi:SSID:PASSWORD");
      }
      provBuf=""; Serial.print("\nwifi> "); }
    else {
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
  String page = F("<!DOCTYPE html><html><head><meta charset='utf-8'><title>ESP32 Blink</title><style>body{font-family:Arial;margin:1em;}button{padding:0.6em 1em;font-size:1.1em;} ul{list-style:none;padding:0;} li{margin:0.5em 0;}</style></head><body><h1>ESP32 Blink</h1><div id=stats>Loading...</div><button onclick=toggle()>Toggle LED</button><h2>API Endpoints</h2><ul><li><strong>/api/status</strong> - Current LED state, counter, uptime, Wi-Fi IP, BLE name</li><li><strong>/api/toggle</strong> (POST) - Toggle LED on/off</li><li><strong>/api/info</strong> - Build info, memory usage, feature flags</li><li><strong>/api/wifi</strong> - Wi-Fi connection status (SSID, IP, RSSI)</li></ul><script>async function refresh(){let r=await fetch('/api/status');let j=await r.json();document.getElementById('stats').innerText=JSON.stringify(j,null,2);}async function toggle(){await fetch('/api/toggle',{method:'POST'});refresh();}refresh();setInterval(refresh,3000);</script></body></html>");
  return page;
}
static void setupWeb(); // fwd
#endif


// -----------------------------------------------------------------------------
// BLE Advertising
// -----------------------------------------------------------------------------
#ifdef USE_BLE_ADV
#include <NimBLEDevice.h>
#ifndef BLE_ADV_NAME
#define BLE_ADV_NAME "" // dynamic by default
#endif
static NimBLEAdvertising* pAdvertising = nullptr;
char gBleName[20];
static unsigned long lastBleUpdate = 0;
static void setupBle() {
  if (strlen(BLE_ADV_NAME)) { strncpy(gBleName,BLE_ADV_NAME,sizeof(gBleName)-1); gBleName[sizeof(gBleName)-1]='\0'; }
  else { uint64_t mac=ESP.getEfuseMac(); snprintf(gBleName,sizeof(gBleName),"ESP32WROOM%04X", (uint16_t)(mac & 0xFFFF)); }
  NimBLEDevice::init(gBleName);
  pAdvertising = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData adv; adv.setName(gBleName); adv.setManufacturerData(std::string("LED=") + (ledState?"1":"0"));
  pAdvertising->setAdvertisementData(adv); pAdvertising->start();
  Serial.printf("BLE advertising name=%s\n", gBleName);
}
static void updateBleAdv() {
  if(!pAdvertising) return; unsigned long now=millis(); if(now-lastBleUpdate<2000) return; lastBleUpdate=now;
  NimBLEAdvertisementData adv; adv.setName(gBleName); adv.setManufacturerData(std::string("LED=") + (ledState?"1":"0")); pAdvertising->setAdvertisementData(adv);
}
#endif

// -----------------------------------------------------------------------------
// OLED Panels (must follow BLE so gBleName exists)
// -----------------------------------------------------------------------------
#ifdef USE_OLED
enum OledPanel: uint8_t { PANEL_BUILD=0, PANEL_UPTIME, PANEL_WIFI, PANEL_BLE, PANEL_COUNT };
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
  Serial.println("Provisioning commands:");
  Serial.println("  wifi:SSID:PASSWORD  -> set / connect (password masked)");
  Serial.println("  wifi:clear           -> erase stored credentials");
  Serial.println();
  Serial.print("wifi> ");
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
  handleProvisioning();
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
#endif
#ifdef USE_OLED
  oledRotatePanels();
#endif
  delay(5);
}
