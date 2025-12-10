# QT2040 Trinkey CircuitPython Deployment Notes

## Deploy Script Usage

### Basic Usage
```bash
./deploy.sh [source_file]
```

### Examples
```bash
# Deploy code.py (default)
./deploy.sh

# Deploy a specific Python file (like lava-domino.py)
./deploy.sh lava-domino.py

# Deploy any other script
./deploy.sh my-script.py
```

### How deploy.sh Works
1. **Default Source**: If no argument provided, uses `code.py`
2. **Custom Source**: Takes first argument as source filename
3. **Target Device**: Always deploys to `/Volumes/CIRCUITPY/code.py`
4. **Device Detection**: Checks if `/Volumes/CIRCUITPY` is mounted
5. **Auto-restart**: Device automatically restarts with new code

### Deploy Script Logic
- Source file: `${1:-code.py}` (first argument or default to code.py)
- Target device: `/Volumes/CIRCUITPY`
- Always copies to `code.py` on the device (regardless of source filename)
- Provides success/error feedback

## CircuitPython Library Requirements

### ⚠️ IMPORTANT: Library Dependencies
Many CircuitPython scripts require additional libraries that must be copied to the device manually.

### Required Libraries for Current Projects
- **adafruit_is31fl3741** - For I2C LED matrix control
- **neopixel** - For built-in NeoPixel control
- **adafruit_debouncer** - For button handling (if used)

### Library Installation Process
1. **Download Adafruit CircuitPython Bundle**:
   - Get from: https://circuitpython.org/libraries
   - Extract the bundle

2. **Copy Required Libraries**:
   ```bash
   # Copy individual library files to device
   cp adafruit-circuitpython-bundle-*/lib/adafruit_is31fl3741 /Volumes/CIRCUITPY/lib/
   cp adafruit-circuitpython-bundle-*/lib/neopixel.mpy /Volumes/CIRCUITPY/lib/
   ```

3. **Library Location on Device**:
   - Device path: `/Volumes/CIRCUITPY/lib/`
   - Must create `lib` directory if it doesn't exist

### Current Project Library Status
- **Bundle Present**: `adafruit-circuitpython-bundle-10.x-mpy-20250924/` (in project directory)
- **Required for lava-domino.py**:
  - `adafruit_is31fl3741` (for I2C matrix)
  - `neopixel.mpy` (for status LED)

### Troubleshooting
- **Import Errors**: Usually means missing libraries in `/Volumes/CIRCUITPY/lib/`
- **Device Not Found**: Check USB connection and that device is mounted
- **Permission Errors**: Device might be read-only or not properly mounted

### Best Practices
1. Always check library dependencies before deploying
2. Keep a local copy of the CircuitPython bundle
3. Test deployment on a simple script first
4. Monitor device serial output for import errors
5. Use version-specific libraries matching your CircuitPython version

## Device Information
- **Target Device**: QT2040 Trinkey
- **Mount Point**: `/Volumes/CIRCUITPY`
- **Main Script**: `code.py` (auto-runs on startup)
- **Library Directory**: `/Volumes/CIRCUITPY/lib/`

## Quick Reference Commands

### Check Device Connection
```bash
ls -la /Volumes/CIRCUITPY/
```

### Check Library Status
```bash
ls -la /Volumes/CIRCUITPY/lib/ | grep -E "(is31fl3741|neopixel)"
```

### Deploy Script
```bash
./deploy.sh lava-domino.py
```

### Monitor Device Output (if serial monitor available)
```bash
python3 serial_monitor.py
```

## Current Status
- ✅ QT2040 Trinkey connected at `/Volumes/CIRCUITPY`
- ✅ Required libraries present: `adafruit_is31fl3741`, `neopixel.mpy`
- ✅ lava-domino.py successfully deployed and running
