# ESP32 USB Storage Alternatives

## Why ESP32 Can't Act as USB Drive

The ESP32 with MicroPython **does not support USB Mass Storage mode** for several reasons:

1. **Hardware limitation**: ESP32 doesn't have native USB (only UART over USB-to-serial chip)
2. **MicroPython limitation**: Even ESP32-S2/S3 with native USB don't implement USB MSC in MicroPython
3. **Firmware constraint**: USB Mass Storage requires specific firmware support not available in standard MicroPython

## Alternative Solutions

### Solution 1: CircuitPython (ESP32-S2/S3 only)

If you have an **ESP32-S2 or ESP32-S3** board, you can use CircuitPython which supports USB drive mode:

1. Flash CircuitPython firmware (instead of MicroPython)
2. Board appears as `CIRCUITPY` drive when connected
3. Drag and drop Python files directly
4. Files run automatically on boot

**Limitations:**
- Only works on ESP32-S2/S3 (not original ESP32)
- Must completely replace MicroPython with CircuitPython
- Different Python implementation (some code changes needed)

**How to switch:**
```bash
# Download CircuitPython for ESP32-S2/S3 from:
# https://circuitpython.org/downloads

# Flash it
esptool.py --chip esp32s3 erase_flash
esptool.py --chip esp32s3 write_flash -z 0x0 circuitpython.bin

# Board will appear as USB drive
```

### Solution 2: WebREPL File Transfer (Recommended)

Use WiFi to transfer files instead of USB:

1. Enable WebREPL on ESP32
2. Access filesystem via web browser
3. Upload/download files wirelessly
4. No need to interrupt running scripts

**Setup:**

Add to `boot.py`:
```python
import network
import webrepl

# Connect to WiFi
wlan = network.WLAN(network.STA_IF)
wlan.active(True)
wlan.connect('YOUR_SSID', 'YOUR_PASSWORD')

# Start WebREPL
webrepl.start()
```

**Usage:**
1. Connect to WiFi (ESP32 gets IP address)
2. Open browser: http://micropython.org/webrepl/
3. Connect to ESP32's IP address
4. Transfer files through web interface

**Advantages:**
- Works with running scripts (no REPL needed)
- Can update files remotely
- No physical connection required

### Solution 3: FTP Server on ESP32

Run an FTP server on the ESP32 to access files like a network drive:

**Install uftpd:**
```python
# On ESP32 REPL (one-time setup)
import mip
mip.install('github:cpopp/MicroPythonFTP/ftp.py')
```

**Add to your code:**
```python
import network
from ftp import FTP

# Connect to WiFi
wlan = network.WLAN(network.STA_IF)
wlan.active(True)
wlan.connect('YOUR_SSID', 'YOUR_PASSWORD')

# Start FTP server
ftp = FTP()
```

**Access:**
- Use FTP client (FileZilla, Cyberduck, Finder)
- Connect to ESP32's IP address
- Drag and drop files

### Solution 4: HTTP File Upload Server

Create a web interface for file uploads:

```python
import network
from microdot import Microdot

app = Microdot()

@app.route('/upload', methods=['POST'])
def upload(request):
    file = request.files['file']
    with open(file.filename, 'wb') as f:
        f.write(file.stream.read())
    return 'File uploaded'

wlan = network.WLAN(network.STA_IF)
wlan.active(True)
wlan.connect('SSID', 'PASSWORD')

app.run(host='0.0.0.0', port=80)
```

### Solution 5: Keep Using mpremote with Deployment Script

The current solution with the deployment script is actually quite efficient:

```bash
# One command to update everything
cd esp32
./deploy.sh
```

The script:
- Uploads all files automatically
- Resets the device
- Shows status

**This is the simplest solution for your use case.**

## Comparison Table

| Method | ESP32 Support | Running Scripts | Ease of Use | Notes |
|--------|--------------|-----------------|-------------|-------|
| **USB Drive (MSC)** | ❌ No | N/A | ⭐⭐⭐⭐⭐ | Not supported |
| **CircuitPython** | ESP32-S2/S3 only | ✅ Yes | ⭐⭐⭐⭐⭐ | Different firmware |
| **WebREPL** | ✅ Yes | ✅ Yes | ⭐⭐⭐⭐ | Requires WiFi |
| **FTP Server** | ✅ Yes | ✅ Yes | ⭐⭐⭐⭐ | Requires WiFi |
| **HTTP Upload** | ✅ Yes | ✅ Yes | ⭐⭐⭐ | Requires WiFi |
| **mpremote + deploy.sh** | ✅ Yes | ⚠️ Requires reset | ⭐⭐⭐⭐ | Current solution |

## Recommended Solution for Your Setup

Given that you:
1. Have BLE provisioning running on boot
2. Need to update `ble_name.txt` and Python files
3. Want simplicity

**I recommend: WebREPL (Solution 2)**

### Why WebREPL is Best Here:

✅ **Works while BLE runs** - No REPL conflict
✅ **Update files remotely** - No USB cable needed
✅ **Simple web interface** - Just open a browser
✅ **No firmware change** - Keep using MicroPython
✅ **Can monitor output** - REPL still available via WiFi

### Quick WebREPL Setup

Want me to modify your `boot.py` to enable WebREPL? I can add WiFi configuration and WebREPL startup, so you can:

1. Connect ESP32 to WiFi
2. Access files via browser
3. Update `ble_name.txt` without interrupting BLE
4. See polling output via WebREPL

Would you like me to implement this?

## Summary

- ❌ ESP32 **cannot** act as USB drive with MicroPython
- ✅ ESP32-S2/S3 **can** with CircuitPython (different firmware)
- ✅ **WebREPL** is the best alternative for your use case
- ✅ Current **deploy.sh** script works well for development

Let me know if you'd like me to set up WebREPL for wireless file access!
