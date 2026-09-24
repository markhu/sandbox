"""
Quick BLE Status Check Script

This script checks if BLE is running and what name it's advertising
Run this while your ESP32 is running normally (not during test_ble.py)
"""

import bluetooth
import time

print("=" * 60)
print("BLE Status Check")
print("=" * 60)

# Check if BLE is active
ble = bluetooth.BLE()
print(f"\n[1] BLE Active: {ble.active()}")

# Try to read the current BLE name from file
try:
    with open('ble_name.txt', 'r') as f:
        name = f.read().strip()
    print(f"[2] BLE name in file: '{name}'")
except Exception as e:
    print(f"[2] Could not read ble_name.txt: {e}")

# Check if bluetooth module is available
try:
    print(f"[3] Bluetooth module available: Yes")
    print(f"[4] BLE config: {ble.config('mac')}")
except Exception as e:
    print(f"[3] Bluetooth check failed: {e}")

print("\n" + "=" * 60)
print("If BLE Active = True, then hardware is working")
print("The issue may be with boot.py or main.py preventing advertising")
print("=" * 60)
