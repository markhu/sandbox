# BLE Provisioning and REPL Conflict - Solution Guide

## The Problem

**The BLE provisioning script CANNOT work while the Python REPL is running.** Here's why:

### Root Causes

1. **Serial Port Conflict**: Both the REPL and the BLE provisioning script try to read from the same serial port (`sys.stdin`)
2. **Blocking Input**: The provisioning script uses `input()` or `select()` which blocks and prevents REPL interaction
3. **Main Thread Occupation**: The script runs in an infinite loop in the main thread, preventing the REPL prompt from appearing
4. **Auto-start Conflict**: The `boot.py` automatically starts `main.py`, which launches the BLE provisioning system before you can access the REPL

### Current Setup Analysis

```
boot.py (lines 7-8)
  ↓ Auto-starts on boot
main.py
  ↓ Starts BLE in main thread (line 73)
ble_provisioning.py
  ↓ Runs infinite loop reading stdin (lines 188-204)
BLOCKS REPL ACCESS ❌
```

## Solutions

### Solution 1: Disable Auto-Start (ALREADY APPLIED ✓)

**Best for**: Development, testing, REPL access

The `boot.py` file has been modified to NOT auto-start `main.py`. This allows you to:
- Access the REPL normally
- Manually start the BLE provisioning when needed
- Test code interactively

**To use BLE provisioning now:**

```python
# Connect to REPL
# Then manually import and start:
>>> import ble_provisioning
>>> # This will start the BLE provisioning system
```

**To re-enable auto-start for production:**

Edit `boot.py` and uncomment the last two lines:
```python
import main
main.main()
```

### Solution 2: Use WebREPL Instead of Serial REPL

**Best for**: Remote access, keeping auto-start enabled

Keep the auto-start in `boot.py` but access the ESP32 via WebREPL over WiFi instead of serial.

**Setup:**

1. Edit `boot.py` to enable WebREPL:
```python
import webrepl
webrepl.start()
```

2. Configure WiFi in `boot.py`:
```python
import network
wlan = network.WLAN(network.STA_IF)
wlan.active(True)
wlan.connect('YOUR_SSID', 'YOUR_PASSWORD')
```

3. Access via browser: `http://micropython.org/webrepl/`

**Pros**:
- BLE provisioning can run on serial port
- Still have REPL access via WiFi
- No physical connection needed

**Cons**:
- Requires WiFi setup
- Additional complexity

### Solution 3: Use Alternative Communication Method

**Best for**: Production deployments

Instead of using serial input for provisioning, use a BLE characteristic that can be written to:

**Changes needed:**

1. Add a GATT service with a writable characteristic
2. Read provisioning commands from the BLE characteristic
3. Remove serial input handling

**Example:**
```python
# Add a GATT service for provisioning
_PROV_UUID = bluetooth.UUID('12345678-1234-5678-1234-56789abcdef0')
_PROV_CHAR = bluetooth.UUID('12345678-1234-5678-1234-56789abcdef1')

# Handle writes to characteristic
def _irq_handler(self, event, data):
    if event == _IRQ_GATTS_WRITE:
        # Read the provisioning command from BLE write
        conn_handle, attr_handle = data
        value = self.ble.gatts_read(attr_handle)
        self.process_command(value.decode('utf-8'))
```

**Pros**:
- No serial port conflicts
- Truly wireless provisioning
- More professional solution

**Cons**:
- More complex implementation
- Requires BLE app on phone/computer

### Solution 4: Conditional Auto-Start with Boot Pin

**Best for**: Flexibility between development and production

Add a boot pin check to decide whether to auto-start:

```python
# boot.py
from machine import Pin
import time

# Check GPIO pin (e.g., GPIO 0)
boot_pin = Pin(0, Pin.IN, Pin.PULL_UP)
time.sleep_ms(100)  # Debounce

# Only auto-start if pin is HIGH (not pressed)
if boot_pin.value() == 1:
    import main
    main.main()
else:
    print("Boot pin held LOW - REPL mode")
    print("Release pin and run: import main; main.main()")
```

**Usage**:
- Normal boot: Auto-starts BLE provisioning
- Hold GPIO 0 to GND during boot: Get REPL access

## Testing BLE Advertising

Once BLE is running (via any method above), verify it's working:

### macOS/Linux:
```bash
# Scan for BLE devices
sudo hcitool lescan | grep "BAT-PRO-3 BEE9"
```

### Python Script:
```python
from bleak import BleakScanner
import asyncio

async def scan():
    devices = await BleakScanner.discover()
    for d in devices:
        if "BAT-PRO-3" in d.name:
            print(f"Found: {d.name} - {d.address}")

asyncio.run(scan())
```

### Phone Apps:
- iOS: Settings → Bluetooth
- Android: nRF Connect app
- Windows: Bluetooth settings

## Current Configuration

- **BLE Name File**: `ble_name.txt` contains "BAT-PRO-3 BEE9"
- **Auto-start**: DISABLED (REPL accessible)
- **To manually start BLE**: Run `import ble_provisioning` from REPL

## Recommended Development Workflow

1. **During Development** (current setup):
   - Boot to REPL
   - Manually start: `import ble_provisioning`
   - Test and iterate

2. **For Production**:
   - Re-enable auto-start in `boot.py`
   - OR implement Solution 3 (BLE characteristic provisioning)
   - OR use Solution 4 (boot pin selection)

## Common Issues

### "I can't see the REPL prompt"
- The auto-start is still enabled in `boot.py`
- Press Ctrl+C to interrupt the running script
- Modify `boot.py` to disable auto-start

### "BLE advertising doesn't start"
- You haven't imported the module yet
- Run: `import ble_provisioning` from REPL

### "Can't detect BLE device on phone"
- BLE script is not running
- Check if script printed "Started advertising as: BAT-PRO-3 BEE9"
- Try turning Bluetooth off/on on your device
- Get closer to the ESP32

### "REPL freezes after importing ble_provisioning"
- This is expected - the script runs an infinite loop
- Press Ctrl+C to stop it
- This is why auto-start conflicts with REPL

## Summary

The fundamental issue is that **serial-based input handling and REPL cannot coexist**. You must choose one of:

1. ✅ **REPL access** (boot.py auto-start disabled) - Current setup
2. **Auto-start BLE** (no serial REPL) - Production setup
3. **WebREPL** (WiFi REPL, serial for BLE) - Advanced setup
4. **BLE-based provisioning** (no serial dependency) - Professional solution

The current configuration prioritizes REPL access for development. When ready for production, re-enable auto-start in `boot.py`.
