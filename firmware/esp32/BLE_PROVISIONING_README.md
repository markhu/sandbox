# ESP32 BLE Provisioning Script

This MicroPython script allows you to configure the BLE (Bluetooth Low Energy) advertising name of your ESP32 development board via serial port commands.

## Features

- Accept provisioning commands via serial port (UART)
- Set custom BLE advertising names dynamically
- Automatic BLE advertising with configurable name
- Command format validation
- Connection status monitoring
- Auto-restart advertising after disconnect

## Requirements

- ESP32 development board
- MicroPython firmware installed (v1.20 or later recommended)
- USB connection to computer
- Terminal program (screen, minicom, PuTTY, or Thonny IDE)

## Installation

1. Flash MicroPython to your ESP32 (see main ESP32 README.md)

2. Upload the script to your ESP32:

   ```bash
   # Using ampy
   ampy --port /dev/tty.usbserial-XXXXXXXX put ble_provisioning.py

   # Or rename it to main.py to run automatically on boot
   ampy --port /dev/tty.usbserial-XXXXXXXX put ble_provisioning.py main.py
   ```

3. Alternatively, use Thonny IDE:
   - Open `ble_provisioning.py` in Thonny
   - Save it to the ESP32 (File → Save As → MicroPython device)

## Usage

### Running the Script

**Option 1: Run directly from REPL**

```bash
# Connect to REPL
screen /dev/tty.usbserial-XXXXXXXX 115200

# In the REPL, run:
>>> import ble_provisioning
```

### Option 2: Run as main.py (auto-start on boot)

If you saved it as `main.py`, it will run automatically when the ESP32 boots.

**Option 3: Use ampy to run**

```bash
ampy --port /dev/tty.usbserial-XXXXXXXX run ble_provisioning.py
```

### Provisioning Commands

#### Set BLE Name

```
ble/name EXAMPLE-1234
```

**Format:**

- Command: `ble/name`
- Separator: single space
- Name: Any alphanumeric string (up to 29 characters)

**Examples:**

```tty
ble/name ESP32-Living-Room
ble/name SENSOR-001
ble/name MyDevice-ABC123
ble/name Office-Temp-Monitor
```

### Expected Output

When you run the script, you'll see:

```text
==================================================
ESP32 BLE Provisioning Script
==================================================
[INFO] Starting BLE provisioning system...
[BLE] Started advertising as: ESP32-Device

[READY] Waiting for provisioning commands...
[HELP] Send commands in format: ble/name EXAMPLE-1234
[HELP] Press Ctrl+C to exit
```

When you send a provisioning command:

```tty
ble/name EXAMPLE-1234
[CMD] Received: ble/name EXAMPLE-1234
[BLE] Name changed: ESP32-Device -> EXAMPLE-1234
[BLE] Stopped advertising
[BLE] Started advertising as: EXAMPLE-1234
```

### Testing BLE Advertising

You can verify the BLE advertising name using:

**iOS:**
- Open Settings → Bluetooth
- Look for your device name in the list

**Android:**
- Use "nRF Connect" app (free from Play Store)
- Scan for devices
- Look for your device name

**macOS:**
- Open System Settings → Bluetooth
- Look for your device in the list

**Linux:**
```bash
# Scan for BLE devices
sudo hcitool lescan

# Or use bluetoothctl
bluetoothctl
scan on
```

## Command Reference

| Command | Format | Description | Example |
|---------|--------|-------------|---------|
| `ble/name` | `ble/name <NAME>` | Set BLE advertising name | `ble/name SENSOR-42` |

## Limitations

- **Name length:** Maximum 29 characters (longer names will be truncated)
- **Characters:** Best to use alphanumeric characters and hyphens
- **Persistence:** Name is not saved to flash - resets to default on reboot

## Troubleshooting

### Script doesn't respond to commands

1. Make sure you're sending newline character (`\n` or `\r\n`)
2. Try typing commands directly in the terminal
3. Check that BLE is supported and enabled on your ESP32

### BLE name doesn't change on phone

1. Turn Bluetooth off and on again on your phone
2. Forget the device if it was previously paired
3. Move closer to the ESP32
4. Restart the BLE scan

### "ImportError: no module named 'bluetooth'"

- Your MicroPython build doesn't include Bluetooth support
- Download and flash a firmware with BLE support from [micropython.org](https://micropython.org/download/ESP32_GENERIC/)
- Ensure you're using ESP32 (not ESP8266)

### Script crashes or resets

- Check available memory: `import gc; gc.mem_free()`
- Some ESP32 boards have limited RAM for BLE operations
- Try reducing buffer sizes or simplifying the code

## Advanced Usage

### Saving Name to Flash

To persist the BLE name across reboots, you can modify the script to save/load from a configuration file:

```python
import json

# Save name
def save_config(name):
    with open('ble_config.json', 'w') as f:
        json.dump({'ble_name': name}, f)

# Load name on startup
def load_config():
    try:
        with open('ble_config.json', 'r') as f:
            config = json.load(f)
            return config.get('ble_name', DEFAULT_BLE_NAME)
    except:
        return DEFAULT_BLE_NAME
```

### Integration with Other Scripts

You can import the `BLEProvisioning` class in your own scripts:

```python
from ble_provisioning import BLEProvisioning

# Create instance
ble = BLEProvisioning()

# Set custom name
ble.set_name("MyCustomName")

# Your application code here
while True:
    # Do your work
    pass
```

## Example Session

```
$ screen /dev/tty.usbserial-0001 115200

==================================================
ESP32 BLE Provisioning Script
==================================================
[INFO] Starting BLE provisioning system...
[BLE] Started advertising as: ESP32-Device

[READY] Waiting for provisioning commands...
[HELP] Send commands in format: ble/name EXAMPLE-1234
[HELP] Press Ctrl+C to exit

ble/name LIVING-ROOM-SENSOR
[CMD] Received: ble/name LIVING-ROOM-SENSOR
[BLE] Name changed: ESP32-Device -> LIVING-ROOM-SENSOR
[BLE] Stopped advertising
[BLE] Started advertising as: LIVING-ROOM-SENSOR

ble/name BEDROOM-001
[CMD] Received: ble/name BEDROOM-001
[BLE] Name changed: LIVING-ROOM-SENSOR -> BEDROOM-001
[BLE] Stopped advertising
[BLE] Started advertising as: BEDROOM-001

^C
[INFO] Shutting down...
[BLE] Stopped advertising
[INFO] BLE advertising stopped
[INFO] Goodbye!
```

## License

This script is provided as-is for educational and development purposes.

## Resources

- [MicroPython Bluetooth Documentation](https://docs.micropython.org/en/latest/library/bluetooth.html)
- [ESP32 BLE Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/index.html)
- [Main ESP32 README](./README.md)
