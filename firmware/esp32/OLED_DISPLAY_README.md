# ESP32 OLED Display for BLE Name

This script monitors the `ble_name.txt` file created by the BLE provisioning system and displays the current BLE device name on a 128x64 OLED display connected via I2C.

## Hardware Requirements

- ESP32 development board
- 128x64 OLED display (SSD1306 or compatible)
- I2C connection:
  - **SCL** → GPIO22 (default, configurable)
  - **SDA** → GPIO21 (default, configurable)
  - **VCC** → 3.3V
  - **GND** → GND

## Software Requirements

### MicroPython SSD1306 Driver

The script requires the SSD1306 OLED driver for MicroPython. You need to install it on your ESP32.

#### Option 1: Using mpremote (Recommended)

```bash
mpremote mip install ssd1306
```

#### Option 2: Manual Installation

1. Download the `ssd1306.py` file from the MicroPython library:
   ```bash
   wget https://raw.githubusercontent.com/micropython/micropython-lib/master/micropython/drivers/display/ssd1306/ssd1306.py
   ```

2. Upload it to your ESP32:
   ```bash
   mpremote fs cp ssd1306.py :ssd1306.py
   ```

## Installation

1. **Upload the OLED display script to your ESP32:**
   ```bash
   mpremote fs cp esp32/oled_display.py :oled_display.py
   ```

2. **Verify the files are on the ESP32:**
   ```bash
   mpremote fs ls
   ```

   You should see:
   - `oled_display.py`
   - `ssd1306.py`
   - `ble_name.txt` (created after first provisioning)

## Usage

### Running the Display Script

Connect to your ESP32 and run:

```bash
mpremote run esp32/oled_display.py
```

Or connect via serial and run in the REPL:

```python
import oled_display
```

### Running with BLE Provisioning

You can run both scripts simultaneously in separate terminal windows:

**Terminal 1 - BLE Provisioning:**
```bash
mpremote run esp32/ble_provisioning.py
```

**Terminal 2 - OLED Display:**
```bash
mpremote run esp32/oled_display.py
```

When you send a provisioning command like `ble/name DEVICE-123`, the display will automatically update to show the new name.

## Display Features

### Main Display
- **Header:** "BLE Device Name:"
- **Separator line:** Visual division between header and content
- **Name display:** Centered text showing the current BLE name
- **Update indicator:** Shows when the display was last updated

### Default State
When no BLE name has been provisioned yet, the display shows:
```
BLE Device Name:
----------------
    Not Set
  Waiting for
provisioning...
```

### Long Names
The script handles long BLE names by:
- Splitting at hyphens (`-`) for natural word breaks
- Displaying across multiple lines (up to 4 lines)
- Centering each line for better readability

## Configuration

You can customize the I2C pins by modifying the constants at the top of [`oled_display.py`](oled_display.py):

```python
# I2C configuration
I2C_SCL_PIN = 22  # Change to your SCL pin
I2C_SDA_PIN = 21  # Change to your SDA pin
I2C_FREQ = 400000  # I2C frequency (400kHz)

# OLED configuration
OLED_WIDTH = 128   # Display width in pixels
OLED_HEIGHT = 64   # Display height in pixels

# Update interval
UPDATE_INTERVAL = 1000  # Check file every 1 second (in milliseconds)
```

## How It Works

1. **Initialization:**
   - Sets up I2C communication on specified pins
   - Scans for I2C devices and reports their addresses
   - Initializes the SSD1306 OLED display
   - Clears the display

2. **File Monitoring:**
   - Checks for the existence of `ble_name.txt`
   - Monitors the file's modification time
   - When the file changes, reads the new BLE name
   - Updates the display with the new name

3. **Display Update:**
   - Clears the previous content
   - Draws the header and separator
   - Formats and centers the BLE name
   - Shows update indicator

## Troubleshooting

### "No I2C devices found" Error

**Causes:**
- OLED display not connected
- Incorrect wiring
- Wrong I2C pins configured

**Solutions:**
1. Check your wiring matches the pin configuration
2. Verify the OLED display is powered (3.3V and GND)
3. Try running an I2C scan:
   ```python
   from machine import Pin, SoftI2C
   i2c = SoftI2C(scl=Pin(22), sda=Pin(21))
   print(i2c.scan())  # Should show device address (typically 0x3c)
   ```

### "SSD1306 driver not found" Error

**Cause:**
- The `ssd1306.py` library is not installed

**Solution:**
Install the library using one of the methods described in [Software Requirements](#software-requirements)

### Display Shows Garbled Text

**Causes:**
- Wrong display dimensions configured
- I2C communication issues

**Solutions:**
1. Verify your display is 128x64 pixels
2. If different, update `OLED_WIDTH` and `OLED_HEIGHT` constants
3. Lower the I2C frequency if communication is unreliable:
   ```python
   I2C_FREQ = 100000  # Try 100kHz instead of 400kHz
   ```

### Display Not Updating

**Causes:**
- `ble_name.txt` file not being created
- File system issues
- Script not running

**Solutions:**
1. Verify the BLE provisioning script is creating the file:
   ```bash
   mpremote fs cat ble_name.txt
   ```
2. Check script output for errors
3. Reduce `UPDATE_INTERVAL` for faster updates (default: 1 second)

## Integration with BLE Provisioning

The OLED display script integrates seamlessly with the [BLE Provisioning system](BLE_PROVISIONING_README.md):

1. **BLE Provisioning** ([`ble_provisioning.py`](ble_provisioning.py)):
   - Receives provisioning commands via serial
   - Updates BLE advertising name
   - **Writes name to `ble_name.txt`** (new feature)

2. **OLED Display** ([`oled_display.py`](oled_display.py)):
   - Monitors `ble_name.txt` for changes
   - Displays current BLE name on OLED
   - Updates automatically when name changes

### Complete Workflow

```
Serial Command → BLE Provisioning Script → ble_name.txt → OLED Display
     ↓                      ↓                               ↓
ble/name FOO-123    Updates BLE name         Shows "FOO-123" on display
                    Saves to file
```

## Running as Boot Script

To automatically run the OLED display on ESP32 boot:

1. **Rename the script to `main.py`** (or add to existing `main.py`):
   ```bash
   mpremote fs cp esp32/oled_display.py :main.py
   ```

2. **Or create a `boot.py` that imports it:**
   ```python
   # boot.py
   import oled_display
   ```

**Note:** If running on boot, ensure the SSD1306 library is also uploaded to the ESP32.

## Example Session

```bash
# Terminal 1: Start BLE provisioning
$ mpremote run esp32/ble_provisioning.py
ESP32 BLE Provisioning Script
[INFO] Starting BLE provisioning system...
[BLE] Started advertising as: ESP32-Device
[READY] Waiting for provisioning commands...

# Terminal 2: Start OLED display
$ mpremote run esp32/oled_display.py
ESP32 OLED Display for BLE Name
[I2C] Found devices at addresses: ['0x3c']
[OLED] Display initialized (128x64)
[INFO] Current BLE name: ESP32-Device
[READY] Monitoring for BLE name changes...

# Terminal 1: Send provisioning command
ble/name SENSOR-001
[CMD] Received: ble/name SENSOR-001
[BLE] Name changed: ESP32-Device -> SENSOR-001
[FILE] Saved name to ble_name.txt: SENSOR-001
[BLE] Stopped advertising
[BLE] Started advertising as: SENSOR-001

# Terminal 2: Display updates automatically
[MONITOR] BLE name updated: SENSOR-001
```

The OLED display will now show "SENSOR-001" in a nicely formatted layout.

## API Reference

### OLEDDisplay Class

```python
class OLEDDisplay(scl_pin=22, sda_pin=21, width=128, height=64)
```

**Methods:**
- `clear()` - Clear the display
- `display_text(text, x=0, y=0)` - Display text at position
- `display_centered_text(text, y=None)` - Display centered text
- `display_ble_name(name)` - Display BLE name with formatting

### BLENameMonitor Class

```python
class BLENameMonitor(display, filename='ble_name.txt')
```

**Methods:**
- `read_ble_name()` - Read name from file
- `check_for_updates()` - Check and update if file changed
- `display_default_message()` - Show "Not Set" message

## License

This code is provided as-is for educational and development purposes.

## Related Documentation

- [BLE Provisioning README](BLE_PROVISIONING_README.md)
- [ESP32 Main README](README.md)
