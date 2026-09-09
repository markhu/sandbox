"""
WiFi Configuration
Edit this file with your WiFi credentials before deploying
"""

import os

# WiFi credentials
WIFI_SSID = "orcYard"
WIFI_PASSWORD = os.environ.get("WIFI_PASSWORD")  # print(f"WIFI... {WIFI_PASSWORD}")

# WebREPL password (change this!)
WEBREPL_PASSWORD = "python3"

# Optional: Static IP configuration (comment out to use DHCP)
# STATIC_IP = "192.168.1.100"
# SUBNET_MASK = "255.255.255.0"
# GATEWAY = "192.168.1.1"
# DNS = "192.168.1.1"
