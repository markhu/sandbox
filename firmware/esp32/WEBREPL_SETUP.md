# WebREPL Setup Guide

WebREPL allows you to wirelessly access your ESP32's filesystem and REPL through a web browser - perfect for updating files while your BLE provisioning is running!

## Quick Setup

### Step 1: Configure WiFi

Edit [`wifi_config.py`](wifi_config.py:1) with your WiFi credentials:

```python
WIFI_SSID = "YourWiFiName"
WIFI_PASSWORD = "YourWiFiPassword"
WEBREPL_PASSWORD = "micropython"  # Change this!
```

### Step 2: Deploy to ESP32

```bash
cd esp32
./deploy.sh
```

### Step 3: Find ESP32's IP Address

After deployment, watch the serial output (or use `mpremote repl`):

```
==================================================
ESP32 Boot Sequence
==================================================
[WIFI] Configuration loaded
[WIFI] Connecting to YourWiFiName...
[WIFI] ✓ Connected!
[WIFI] IP Address: 192.168.1.123    ← This is your ESP32's IP
[WIFI] Subnet: 255.255.255.0
[WIFI] Gateway: 192.168.1.1
[WIFI] DNS: 192.168.1.1
[WEBREPL] Starting WebREPL...
[WEBREPL] ✓ WebREPL started on ws://192.168.1.123:8266
[WEBREPL] Connect via: http://micropython.org/webrepl/
[WEBREPL] Password: micropython
==================================================
```

### Step 4: Connect via Browser

1. Open: **http://micropython.org/webrepl/**
2. Click **"Connect"** button
3. Enter IP address when prompted: `ws://192.168.1.123:8266`
4. Enter password (default: `micropython`)
5. You're connected! 🎉

## Using WebREPL

### File Transfer (The Main Feature!)

**Upload a file:**
1. Click **"Choose File"** in the WebREPL interface
2. Select your file (e.g., `ble_name.txt`)
3. Click **"Send to device"**
4. File is uploaded - your BLE script will detect it on next poll!

**Download a file:**
1. Enter filename in the "Get from device" field
2. Click **"Get from device"**
3. File downloads to your computer

**Update BLE name while running:**
```bash
# 1. Edit ble_name.txt on your computer
echo "NEW-DEVICE-42" > ble_name.txt

# 2. Upload via WebREPL web interface
# (Click "Choose File", select ble_name.txt, "Send to device")

# 3. Within 5 seconds, the ESP32 will detect and apply the change!
```

### Interactive REPL

Type Python commands directly in the WebREPL:

```python
>>> import os
>>> os.listdir('/')
['boot.py', 'main.py', 'ble_provisioning.py', 'ble_name.txt', ...]

>>> # Read current BLE name
>>> with open('ble_name.txt', 'r') as f:
...     print(f.read())
BAT-PRO-3 BEE9

>>> # Update BLE name programmatically
>>> with open('ble_name.txt', 'w') as f:
...     f.write('TESTING-123')
>>> # BLE script will pick this up within 5 seconds!
```

## Workflow: Update BLE Name Wirelessly

### Method 1: Via WebREPL File Upload
```bash
# On your computer:
echo "PRODUCTION-DEVICE-001" > ble_name.txt

# Then in browser:
# 1. Open http://micropython.org/webrepl/
# 2. Connect to ESP32
# 3. Upload ble_name.txt
# 4. Done! ESP32 updates within 5 seconds
```

### Method 2: Via WebREPL REPL
```python
# In WebREPL interface, type:
>>> with open('ble_name.txt', 'w') as f:
...     f.write('NEW-NAME-HERE')
>>> # Done! Updates within 5 seconds
```

### Method 3: Via Serial Command (still works!)
```bash
# Connect to serial
screen /dev/tty.usbserial-XXXX 115200

# Type:
ble/name NEW-NAME-HERE
```

## Monitoring Your ESP32

You can see the polling output in WebREPL:

```
[POLL] Checking ble_name.txt... Current: BAT-PRO-3 BEE9, File: BAT-PRO-3 BEE9
[POLL] No change detected

[POLL] Checking ble_name.txt... Current: BAT-PRO-3 BEE9, File: NEW-DEVICE-42
[POLL] Name changed detected!
[BLE] Name changed: BAT-PRO-3 BEE9 -> NEW-DEVICE-42
[FILE] Saved name to ble_name.txt: NEW-DEVICE-42
[BLE] Stopped advertising
[BLE] Started advertising as: NEW-DEVICE-42
```

## Troubleshooting

### Can't Connect to WiFi

Check serial output:
```bash
mpremote repl
# or
screen /dev/tty.usbserial-XXXX 115200
```

Look for:
- `[ERROR] WiFi connection failed!` - Check SSID/password in `wifi_config.py`
- `[INFO] WiFi not configured` - You forgot to edit `wifi_config.py`

### WebREPL Won't Connect

1. **Verify IP address** - Check serial output for the correct IP
2. **Check password** - Default is `micropython`
3. **Firewall** - Some networks block WebSocket connections
4. **Try from phone** - Use phone browser on same WiFi network

### Can't Upload Files

1. **File too large** - WebREPL has size limits (~1MB)
2. **Connection timeout** - Refresh browser and reconnect
3. **Wrong filename** - Check exact filename on ESP32

### WebREPL Not Starting

Check that:
- WiFi credentials are correct in `wifi_config.py`
- ESP32 connected to WiFi successfully
- `webrepl.start()` is called in `boot.py`

## Advanced: WebREPL from Command Line

You can also use WebREPL from Python scripts:

```bash
# Install webrepl client
pip install webrepl

# Upload file
webrepl_cli.py -p micropython ble_name.txt 192.168.1.123:/ble_name.txt

# Download file
webrepl_cli.py -p micropython 192.168.1.123:/ble_name.txt ./ble_name.txt
```

## Security Notes

⚠️ **Important:**
- WebREPL has **no encryption** - don't use on untrusted networks
- Change the default password in `wifi_config.py`
- WebREPL gives **full filesystem access** to anyone with the password
- Only use on your private WiFi network

## Benefits Over USB/mpremote

✅ **No REPL conflict** - Works while BLE script runs
✅ **Wireless** - No USB cable needed
✅ **Real-time updates** - Change files without reset
✅ **Remote access** - Update from anywhere on network
✅ **Multiple files** - Upload/download multiple files easily
✅ **Live monitoring** - See serial output in browser

## Summary

WebREPL solves the original problem:

**Before:**
- ❌ Can't update files while BLE runs (REPL conflict)
- ❌ Must reset ESP32 to upload files
- ❌ Need USB cable connected

**After (with WebREPL):**
- ✅ Update files while BLE runs
- ✅ No reset needed
- ✅ Wireless access from browser
- ✅ See live polling output

Perfect for your BLE provisioning use case! 🎉

## Next Steps

1. Edit `wifi_config.py` with your WiFi credentials
2. Run `./deploy.sh` to upload everything
3. Note the IP address from serial output
4. Open http://micropython.org/webrepl/ in browser
5. Connect and start updating files wirelessly!
