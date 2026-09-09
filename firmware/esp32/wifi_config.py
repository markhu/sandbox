"""
WiFi Configuration

Real credentials live in wifi_secrets.py (gitignored, not committed).
Copy wifi_secrets.py.example to wifi_secrets.py on the device and fill in
your own SSID/password before deploying.
"""

# WiFi credentials + WebREPL password
try:
    from wifi_secrets import WIFI_SSID, WIFI_PASSWORD, WEBREPL_PASSWORD
except ImportError:
    # wifi_secrets.py not present on this device yet.
    WIFI_SSID = ""
    WIFI_PASSWORD = ""
    WEBREPL_PASSWORD = ""

# Optional: Static IP configuration (comment out to use DHCP)
# STATIC_IP = "192.168.1.100"
# SUBNET_MASK = "255.255.255.0"
# GATEWAY = "192.168.1.1"
# DNS = "192.168.1.1"
