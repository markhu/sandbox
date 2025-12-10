# How to Update BLE Name on Running ESP32

## The Problem

When the BLE provisioning script is running, it occupies the serial port's stdin, which prevents `mpremote` from entering raw REPL mode to upload files. You'll get:

```
mpremote.transport.TransportError: could not enter raw repl
```

## Solutions

### Solution 1: Reset, Upload, Reset (Recommended)

Use the deployment script which handles the reset cycle:

```bash
cd esp32
./deploy.sh
```

This will:
1. Upload all files (including updated `ble_name.txt`)
2. Reset the ESP32
3. The new name will be loaded on boot

### Solution 2: Manual Reset and Upload

1. **Press and hold** the RESET button on your ESP32
2. While holding RESET, run:
   ```bash
   mpremote fs cp esp32/ble_name.txt :ble_name.txt
   ```
3. **Release** the RESET button
4. The ESP32 will boot with the new name

### Solution 3: Use Serial Commands (Original Method)

Send commands directly through the serial port:

```bash
# Connect to serial
screen /dev/tty.usbserial-XXXX 115200

# Type the command (you won't see a prompt):
ble/name NEW-DEVICE-NAME

# Press Enter
```

The script will process the command and update the BLE name immediately.

### Solution 4: Edit File Before First Boot

1. Edit `ble_name.txt` on your computer
2. Deploy all files:
   ```bash
   cd esp32
   ./deploy.sh
   ```
3. ESP32 will boot with the name from the file

### Solution 5: Use Boot Pin to Access REPL (Advanced)

If you implement the boot pin solution from `BLE_REPL_CONFLICT_SOLUTION.md`, you can:

1. Hold GPIO 0 to GND during boot
2. ESP32 boots to REPL instead of auto-starting
3. Upload files with `mpremote`
4. Release GPIO 0 and reset

## Quick Reference

### Change Name Before Deployment
```bash
# Edit the file
echo "MY-NEW-DEVICE-123" > esp32/ble_name.txt

# Deploy
cd esp32
./deploy.sh
```

### Change Name on Running Device (via serial command)
```bash
# Option A: Echo to serial port
echo "ble/name MY-NEW-DEVICE-123" > /dev/tty.usbserial-XXXX

# Option B: Use screen
screen /dev/tty.usbserial-XXXX 115200
# Then type: ble/name MY-NEW-DEVICE-123
# Press Ctrl+A, then K to exit
```

### Force Reset to Upload Files
```bash
# The deploy script handles this automatically
cd esp32
./deploy.sh

# Or manually with mpremote
mpremote reset
sleep 1
mpremote fs cp esp32/ble_name.txt :ble_name.txt
mpremote reset
```

## Understanding the Polling Feature

The BLE provisioning script now checks `ble_name.txt` every 5 seconds and outputs to serial:

```
[POLL] Checking ble_name.txt... Current: BAT-PRO-3 BEE9, File: BAT-PRO-3 BEE9
[POLL] No change detected
```

**However**, you cannot use `mpremote` to update the file while the script is running due to the REPL conflict. You must use one of the solutions above.

## Why the Conflict Exists

1. **BLE script runs on boot** → Takes over serial stdin
2. **mpremote needs REPL** → Cannot interrupt stdin-reading script
3. **Solution**: Reset → Upload → Reset cycle

This is a fundamental limitation of how MicroPython handles serial I/O.

## Recommended Workflow

**For Development:**
```bash
# 1. Edit ble_name.txt locally
echo "TEST-DEVICE-42" > esp32/ble_name.txt

# 2. Deploy (handles reset automatically)
cd esp32
./deploy.sh

# 3. Monitor output
mpremote repl
```

**For Production:**
Use serial commands to change names without redeploying:
```bash
echo "ble/name PROD-DEVICE-001" > /dev/tty.usbserial-XXXX
```

## Monitoring the Device

To see the polling output and verify BLE is working:

```bash
# Option 1: mpremote (after deployment)
mpremote repl

# Option 2: screen
screen /dev/tty.usbserial-XXXX 115200

# You should see every 5 seconds:
# [POLL] Checking ble_name.txt... Current: YOUR-NAME, File: YOUR-NAME
# [POLL] No change detected
