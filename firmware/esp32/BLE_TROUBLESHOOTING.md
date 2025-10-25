# ESP32 BLE Troubleshooting Guide

## Problem: BLE Not Advertising (Device Not Visible)

This guide helps diagnose and fix BLE advertising issues on your ESP32 running MicroPython.

---

## Issues Fixed

### ✅ Issue #1: Incompatible `select` module usage

**Problem:** The original [`ble_provisioning.py`](ble_provisioning.py:222) tried to use the `select` module which is not available in MicroPython, causing the BLE provisioning to fail silently.

**Solution:** Simplified the main loop to use file-based polling instead of serial input monitoring. The script now:
- Checks `ble_name.txt` every 5 seconds for updates
- Doesn't rely on the unavailable `select` module
- Runs more reliably on MicroPython

---

## Quick Diagnostic Steps

### Step 1: Run the BLE Test Script

The [`test_ble.py`](test_ble.py) script tests BLE functionality independently:

```bash
# Connect to ESP32 via serial
screen /dev/tty.usbserial-XXXXXXXX 115200

# In the MicroPython REPL, press Ctrl+C to stop boot.py
# Then run:
>>> import test_ble
```

**Expected Output:**
```
============================================================
ESP32 BLE Diagnostic Test
============================================================

[TEST 1] Checking bluetooth module...
✓ Bluetooth module imported successfully

[TEST 2] Activating BLE...
✓ BLE activated: True

[TEST 3] BLE Configuration...
✓ MAC Address: xx:xx:xx:xx:xx:xx

[TEST 4] Creating advertising payload...
✓ Payload created: 14 bytes
  Name: ESP32-TEST
  Payload: 02 01 06 0a 09 45 53 50 33 32 2d 54 45 53 54

[TEST 5] Starting BLE advertising...
✓ Advertising started with name: ESP32-TEST
  Interval: 100ms

[TEST 6] Monitoring advertising status...
============================================================
✓ BLE is now advertising as: ESP32-TEST

INSTRUCTIONS:
1. Open a BLE scanner app on your phone
   - iOS: LightBlue, nRF Connect
   - Android: nRF Connect, BLE Scanner
2. Look for device named: ESP32-TEST
3. You should see it appear in the scan results
```

### Step 2: Scan for BLE Devices

Use a BLE scanner app:

**iOS Apps:**
- **LightBlue** (Free, recommended)
- **nRF Connect** (Free)

**Android Apps:**
- **nRF Connect** (Free, recommended)
- **BLE Scanner** (Free)

**What to look for:**
- Device name: `ESP32-TEST` (from test script) or your configured BLE name
- Signal strength (RSSI): Should be visible if within ~10 meters
- Advertising data should include the device name

---

## Common Issues and Solutions

### Issue: BLE test fails at "Activating BLE"

**Symptoms:**
```
[TEST 2] Activating BLE...
✗ FAILED: Bluetooth not available
```

**Possible Causes:**
1. ESP32 variant doesn't support BLE (some ESP32-S2 models lack BLE)
2. MicroPython firmware doesn't include BLE support
3. Hardware issue

**Solutions:**
1. Verify your ESP32 model supports BLE (ESP32, ESP32-C3, ESP32-S3 do; ESP32-S2 doesn't)
2. Re-flash with official ESP32 MicroPython firmware that includes BLE
3. Try a different ESP32 board

### Issue: BLE activates but advertising fails

**Symptoms:**
```
[TEST 5] Starting BLE advertising...
✗ FAILED: Operation not permitted
```

**Possible Causes:**
1. WiFi is interfering with BLE (both use 2.4GHz radio)
2. BLE already in use by another process
3. Insufficient memory

**Solutions:**

1. **Disable WiFi temporarily:**
```python
>>> import network
>>> wlan = network.WLAN(network.STA_IF)
>>> wlan.active(False)
>>> # Now try BLE test again
>>> import test_ble
```

2. **Reset the BLE stack:**
```python
>>> import bluetooth
>>> ble = bluetooth.BLE()
>>> ble.active(False)
>>> import time
>>> time.sleep(1)
>>> ble.active(True)
```

3. **Check free memory:**
```python
>>> import gc
>>> gc.collect()
>>> gc.mem_free()
```
You should have at least 50KB free for BLE operations.

### Issue: BLE advertises but scanner can't find it

**Symptoms:**
- Test script shows "✓ Advertising started"
- Scanner app doesn't show the device

**Solutions:**

1. **Check scanner settings:**
   - Ensure Bluetooth is enabled on your phone
   - Make sure location services are enabled (required on Android for BLE scanning)
   - Try refreshing the scanner

2. **Verify advertising is active:**
```python
>>> import bluetooth
>>> ble = bluetooth.BLE()
>>> ble.active()  # Should return True
```

3. **Check signal strength:**
   - Move closer to the ESP32 (within 1-2 meters)
   - Remove obstacles between phone and ESP32
   - ESP32's built-in antenna has limited range

4. **Try different advertising interval:**
```python
# In ble_provisioning.py, line 77:
# Change from 100000 (100ms) to 50000 (50ms) for faster discovery
self.ble.gap_advertise(50000, adv_data=payload)
```

### Issue: WiFi connection prevents BLE from working

**Symptoms:**
- WiFi connects successfully
- BLE advertising fails or device isn't visible

**Explanation:**
ESP32 shares radio hardware between WiFi and BLE. In some cases, WiFi can interfere with BLE.

**Solutions:**

1. **Temporary: Disable WiFi for testing:**
   Edit [`boot.py`](boot.py:25-60) to skip WiFi connection:
   ```python
   # Comment out WiFi connection block
   # if WIFI_SSID and WIFI_PASSWORD:
   #     ... (WiFi connection code)
   ```

2. **Use BLE-only mode:**
   Create a minimal boot script that only starts BLE:
   ```python
   # boot_ble_only.py
   import esp
   esp.osdebug(None)

   # Skip WiFi, only start BLE
   import ble_provisioning
   ble_provisioning.main()
   ```

3. **Optimize coexistence:**
   - Use lower WiFi transmit power
   - Reduce BLE advertising frequency
   - Avoid simultaneous WiFi and BLE intensive operations

---

## Deployment Instructions

### Method 1: Using the Makefile

```bash
cd esp32
make deploy
```

This will:
1. Upload all Python files to the ESP32
2. Reset the device
3. Start monitoring serial output

### Method 2: Manual deployment

```bash
# Find your ESP32 port
ls /dev/tty.usbserial-*

# Upload files using ampy
export AMPY_PORT=/dev/tty.usbserial-XXXXXXXX
ampy put boot.py
ampy put main.py
ampy put ble_provisioning.py
ampy put wifi_config.py
ampy put ble_name.txt
ampy put test_ble.py

# Reset the device
python3 -c "import serial; s=serial.Serial('/dev/tty.usbserial-XXXXXXXX', 115200); s.setDTR(False); s.setDTR(True); s.close()"
```

### Method 3: Using WebREPL (if WiFi works)

```bash
# Install webrepl_cli
pip3 install webrepl

# Upload files
webrepl_cli -p python3 boot.py 192.168.1.XXX:/boot.py
webrepl_cli -p python3 ble_provisioning.py 192.168.1.XXX:/ble_provisioning.py
```

---

## Testing the Fixed BLE Provisioning

After deploying the fixed code:

### 1. Check boot sequence

Connect via serial and watch the boot messages:

```bash
screen /dev/tty.usbserial-XXXXXXXX 115200
# Press reset button on ESP32
```

**Expected output:**
```
==================================================
ESP32 Boot Sequence
==================================================
[WIFI] Configuration loaded
[WIFI] Connecting to orcYard...
..
[WIFI] ✓ Connected!
[WIFI] IP Address: 192.168.1.XXX
...
==================================================

==================================================
ESP32 Auto-Start System
==================================================
[MAIN] ✓ OLED display thread started
[MAIN] ✓ Starting BLE provisioning in main thread

==================================================
ESP32 BLE Provisioning Script
==================================================
[INFO] Starting BLE provisioning system...
[FILE] Loaded name from ble_name.txt: ESP32-Device
[BLE] Started advertising as: ESP32-Device

[READY] Waiting for provisioning commands...
[INFO] Running in file-polling mode (checking ble_name.txt every 5 seconds)

[POLL] Checking ble_name.txt... Current: ESP32-Device, File: ESP32-Device
[POLL] No change detected
```

### 2. Test BLE visibility

1. Open BLE scanner app on phone
2. Look for device with name from `ble_name.txt` (default: "ESP32-Device")
3. You should see it advertising

### 3. Test name changes

To change the BLE name, update the file:

**Option A: Via WebREPL**
```python
# In WebREPL:
>>> with open('ble_name.txt', 'w') as f:
...     f.write('MY-ESP32-123')
```

**Option B: Via ampy**
```bash
echo "MY-ESP32-123" > ble_name.txt
ampy put ble_name.txt
```

The system will detect the change within 5 seconds and restart advertising with the new name.

---

## Understanding the File-Based Provisioning

The updated system works as follows:

1. **On boot:**
   - Reads `ble_name.txt` to get the device name
   - Starts BLE advertising with that name

2. **During operation:**
   - Every 5 seconds, checks if `ble_name.txt` has changed
   - If changed, stops advertising and restarts with new name

3. **To provision a new name:**
   - Update `ble_name.txt` via WebREPL, ampy, or serial REPL
   - Wait up to 5 seconds for automatic detection
   - Or restart the device to apply immediately

---

## Advanced Debugging

### Enable verbose BLE logging

Add this to [`ble_provisioning.py`](ble_provisioning.py:56) `start_advertising()`:

```python
def start_advertising(self):
    """Start BLE advertising with the current name"""
    try:
        print(f"[DEBUG] BLE active: {self.ble.active()}")
        print(f"[DEBUG] Is advertising: {self.is_advertising}")
        print(f"[DEBUG] Name to advertise: {self.ble_name}")

        # ... rest of function
```

### Monitor BLE events

Add event logging to [`ble_provisioning.py`](ble_provisioning.py:42) `_irq_handler()`:

```python
def _irq_handler(self, event, data):
    """Handle BLE events"""
    print(f"[DEBUG] BLE Event: {event}, Data: {data}")
    # ... rest of handler
```

### Check BLE configuration

```python
>>> import bluetooth
>>> ble = bluetooth.BLE()
>>> ble.active(True)
>>> ble.config('gap_name')  # Get current GAP name
>>> ble.config('mac')       # Get MAC address
>>> ble.config('rxbuf')     # Get RX buffer size
```

---

## Summary of Changes

1. **Fixed [`ble_provisioning.py`](ble_provisioning.py:195-224)**
   - Removed dependency on unavailable `select` module
   - Simplified main loop to use file-based polling
   - Removed unused `main_polling()` function
   - Added clearer status messages

2. **Created [`test_ble.py`](test_ble.py)**
   - Independent BLE diagnostic script
   - Tests all BLE functionality step-by-step
   - Provides clear pass/fail indicators

3. **Created this troubleshooting guide**
   - Common issues and solutions
   - Deployment instructions
   - Testing procedures

---

## Still Having Issues?

If BLE still doesn't work after following this guide:

1. **Verify ESP32 model:** Confirm your board supports BLE
2. **Check MicroPython version:** Use latest stable release
3. **Test with minimal code:** Use [`test_ble.py`](test_ble.py) in isolation
4. **Check hardware:** Try a different ESP32 board
5. **Review serial output:** Look for error messages during boot

For additional help, provide:
- ESP32 model/board name
- MicroPython version (`import sys; sys.version`)
- Complete serial output from boot
- Output from [`test_ble.py`](test_ble.py)
