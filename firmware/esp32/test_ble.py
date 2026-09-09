"""
ESP32 BLE Diagnostic Test Script

This script tests BLE advertising functionality independently
to help diagnose connectivity issues.

Usage:
1. Upload this file to your ESP32
2. Run: import test_ble
3. Or run directly: python3 -c "import test_ble"
"""

import bluetooth
import time
from micropython import const

# BLE event constants
_IRQ_CENTRAL_CONNECT = const(1)
_IRQ_CENTRAL_DISCONNECT = const(2)

print("=" * 60)
print("ESP32 BLE Diagnostic Test")
print("=" * 60)

# Test 1: Check if bluetooth module is available
print("\n[TEST 1] Checking bluetooth module...")
try:
    ble = bluetooth.BLE()
    print("✓ Bluetooth module imported successfully")
except Exception as e:
    print(f"✗ FAILED: {e}")
    import sys
    sys.exit(1)

# Test 2: Activate BLE
print("\n[TEST 2] Activating BLE...")
try:
    ble.active(True)
    print(f"✓ BLE activated: {ble.active()}")
except Exception as e:
    print(f"✗ FAILED: {e}")
    import sys
    sys.exit(1)

# Test 3: Get BLE configuration
print("\n[TEST 3] BLE Configuration...")
try:
    config = ble.config('mac')
    mac_str = ':'.join(['{:02x}'.format(b) for b in config[1]])
    print(f"✓ MAC Address: {mac_str}")
except Exception as e:
    print(f"✗ Could not read MAC: {e}")

# Test 4: Create advertising payload
print("\n[TEST 4] Creating advertising payload...")
try:
    test_name = "ESP32-TEST"

    # Create payload
    payload = bytearray([
        0x02, 0x01, 0x06,  # Flags: General discoverable mode
    ])

    # Add complete local name
    name_bytes = test_name.encode('utf-8')
    name_len = len(name_bytes) + 1
    payload.extend(bytearray([name_len, 0x09]))  # Complete local name
    payload.extend(name_bytes)

    print(f"✓ Payload created: {len(payload)} bytes")
    print(f"  Name: {test_name}")
    print(f"  Payload: {' '.join(['{:02x}'.format(b) for b in payload])}")

except Exception as e:
    print(f"✗ FAILED: {e}")
    import sys
    sys.exit(1)

# Test 5: Start advertising
print("\n[TEST 5] Starting BLE advertising...")
try:
    # Stop any existing advertising first
    ble.gap_advertise(None)
    time.sleep_ms(100)

    # Start advertising (100ms interval)
    ble.gap_advertise(100000, adv_data=payload)
    print(f"✓ Advertising started with name: {test_name}")
    print("  Interval: 100ms")

except Exception as e:
    print(f"✗ FAILED: {e}")
    import sys
    sys.exit(1)

# Test 6: Keep advertising and show status
print("\n[TEST 6] Monitoring advertising status...")
print("=" * 60)
print(f"✓ BLE is now advertising as: {test_name}")
print()
print("INSTRUCTIONS:")
print("1. Open a BLE scanner app on your phone")
print("   - iOS: LightBlue, nRF Connect")
print("   - Android: nRF Connect, BLE Scanner")
print("2. Look for device named: ESP32-TEST")
print("3. You should see it appear in the scan results")
print()
print("This script will keep advertising for 30 seconds...")
print("Press Ctrl+C to stop early")
print("=" * 60)

try:
    for i in range(30):
        print(f"[{i+1}/30] Advertising... (BLE Active: {ble.active()})")
        time.sleep(1)

    print("\n[SUCCESS] Test completed!")
    print("If you could see 'ESP32-TEST' in your BLE scanner,")
    print("then BLE is working correctly on your ESP32.")

except KeyboardInterrupt:
    print("\n[INFO] Test interrupted by user")

finally:
    # Stop advertising
    print("\n[CLEANUP] Stopping BLE advertising...")
    try:
        ble.gap_advertise(None)
        print("✓ Advertising stopped")
    except:
        pass

    print("\n[DONE] Diagnostic test finished")
    print("=" * 60)
