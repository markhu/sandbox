# This file is executed on every boot (including wake-boot from deepsleep)
import esp
esp.osdebug(None)

import network
import webrepl
import time

print("=" * 50)
print("ESP32 Boot Sequence")
print("=" * 50)

# Load WiFi configuration
try:
    from wifi_config import WIFI_SSID, WIFI_PASSWORD, WEBREPL_PASSWORD
    print("[WIFI] Configuration loaded")
except ImportError:
    print("[ERROR] wifi_config.py not found!")
    print("[INFO] WebREPL will not be available")
    WIFI_SSID = None
    WIFI_PASSWORD = None
    WEBREPL_PASSWORD = "micropython"

# Connect to WiFi if configured
if WIFI_SSID and WIFI_PASSWORD and WIFI_SSID != "YOUR_WIFI_SSID":
    print(f"[WIFI] Connecting to {WIFI_SSID}...")
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)

    if not wlan.isconnected():
        wlan.connect(WIFI_SSID, WIFI_PASSWORD)

        # Wait for connection (max 10 seconds)
        timeout = 10
        while not wlan.isconnected() and timeout > 0:
            time.sleep(1)
            timeout -= 1
            print(".", end="")
        print()

    if wlan.isconnected():
        ip_info = wlan.ifconfig()
        print(f"[WIFI] ✓ Connected!")
        print(f"[WIFI] IP Address: {ip_info[0]}")
        print(f"[WIFI] Subnet: {ip_info[1]}")
        print(f"[WIFI] Gateway: {ip_info[2]}")
        print(f"[WIFI] DNS: {ip_info[3]}")

        # Start WebREPL
        print("[WEBREPL] Starting WebREPL...")
        webrepl.start(password=WEBREPL_PASSWORD)
        print(f"[WEBREPL] ✓ WebREPL started on ws://{ip_info[0]}:8266")
        print(f"[WEBREPL] Connect via: http://micropython.org/webrepl/")
        print(f"[WEBREPL] Password: {WEBREPL_PASSWORD}")
    else:
        print("[ERROR] WiFi connection failed!")
        print("[INFO] WebREPL will not be available")
else:
    print("[INFO] WiFi not configured - edit wifi_config.py")
    print("[INFO] WebREPL will not be available")

print("=" * 50)
print()

# Auto-start main.py - starts BLE provisioning and OLED display
import main
main.main()
