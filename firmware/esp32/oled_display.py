"""
ESP32 OLED Display Script for BLE Name

This script monitors the ble_name.txt file and displays the BLE name
on a 128x64 OLED display connected via I2C.

Hardware Requirements:
- ESP32 development board
- 128x64 OLED display (SSD1306 or compatible) connected via I2C
- Default I2C pins: SCL=GPIO22, SDA=GPIO21 (configurable)

The script will:
1. Initialize the OLED display
2. Read the BLE name from ble_name.txt
3. Display the name on the OLED
4. Monitor the file for changes and update the display
"""

import machine
import time
import os
from machine import Pin, SoftI2C

# Try to import SSD1306 driver
try:
    from ssd1306 import SSD1306_I2C
except ImportError:
    print("[ERROR] SSD1306 driver not found!")
    print("[INFO] Please install the ssd1306 library")
    print("[INFO] Download from: https://github.com/micropython/micropython-lib")
    raise

# I2C configuration
I2C_SCL_PIN = 22  # GPIO22 for SCL
I2C_SDA_PIN = 21  # GPIO21 for SDA
I2C_FREQ = 400000  # 400kHz

# OLED configuration
OLED_WIDTH = 128
OLED_HEIGHT = 64

# File to monitor
BLE_NAME_FILE = 'ble_name.txt'

# Update interval (milliseconds)
UPDATE_INTERVAL = 2345  # ms


class OLEDDisplay:
    def __init__(self, scl_pin=I2C_SCL_PIN, sda_pin=I2C_SDA_PIN,
                 width=OLED_WIDTH, height=OLED_HEIGHT):
        """Initialize the OLED display"""
        try:
            # Initialize I2C
            self.i2c = SoftI2C(scl=Pin(scl_pin), sda=Pin(sda_pin), freq=I2C_FREQ)

            # Scan for I2C devices
            devices = self.i2c.scan()
            if not devices:
                raise RuntimeError("No I2C devices found!")

            print(f"[I2C] Found devices at addresses: {[hex(addr) for addr in devices]}")

            # Initialize OLED display
            self.oled = SSD1306_I2C(width, height, self.i2c)
            self.width = width
            self.height = height

            # Clear display
            self.oled.fill(0)
            self.oled.show()

            print(f"[OLED] Display initialized ({width}x{height})")

        except Exception as e:
            print(f"[ERROR] Failed to initialize OLED: {e}")
            raise

    def clear(self):
        """Clear the display"""
        self.oled.fill(0)
        self.oled.show()

    def clear_rect(self, x, y, width, height):
        """Clear a rectangular region of the display"""
        self.oled.fill_rect(x, y, width, height, 0)

    def display_text(self, text, x=0, y=0):
        """Display text at specified position"""
        self.oled.text(text, x, y)
        self.oled.show()

    def display_centered_text(self, text, y=None):
        """Display text centered horizontally"""
        # Calculate center position
        text_width = len(text) * 8  # Each character is 8 pixels wide
        x = max(0, (self.width - text_width) // 2)

        # Use middle of screen if y not specified
        if y is None:
            y = (self.height - 8) // 2  # Each character is 8 pixels tall

        self.oled.text(text, x, y)
        self.oled.show()

    def display_ble_name(self, name, mtime=None, first_draw=False):
        """Display BLE name with formatting

        Args:
            name: BLE device name to display
            mtime: File modification time (optional)
            first_draw: If True, redraw everything. If False, only update timestamps
        """
        import time

        # On first draw, clear everything and draw static content
        if first_draw:
            self.clear()

            # Display header
            self.oled.text("BLE Device Name:", 0, 0)

            # Draw a separator line
            for x in range(0, self.width):
                self.oled.pixel(x, 12, 1)

            # Display the BLE name (centered)
            # Split into multiple lines if needed
            max_chars_per_line = self.width // 8  # 16 chars for 128px width

            if len(name) <= max_chars_per_line:
                # Single line
                text_width = len(name) * 8
                x = max(0, (self.width - text_width) // 2)
                self.oled.text(name, x, 22)
            else:
                # Multiple lines - split the name
                words = name.split('-')
                lines = []
                current_line = ""

                for word in words:
                    test_line = current_line + ('-' if current_line else '') + word
                    if len(test_line) <= max_chars_per_line:
                        current_line = test_line
                    else:
                        if current_line:
                            lines.append(current_line)
                        current_line = word

                if current_line:
                    lines.append(current_line)

                # Display lines
                start_y = 16
                for i, line in enumerate(lines[:3]):  # Max 3 lines to leave room for timestamps
                    text_width = len(line) * 8
                    x = max(0, (self.width - text_width) // 2)
                    self.oled.text(line, x, start_y + (i * 10))

        # Always update timestamps (only clear the timestamp area, not whole screen)
        # Clear timestamp region (bottom 30 pixels)
        self.clear_rect(0, self.height - 30, self.width, 30)

        current_time = time.localtime()
        timestamp = "{:02d}:{:02d}:{:02d}".format(
            current_time[3], current_time[4], current_time[5]
        )

        # Display file modification time if provided
        if mtime is not None:
            self.oled.text("mtime: %d" % mtime, 0, self.height - 24)

        self.oled.text("Now: %s" % timestamp, 0, self.height - 14)

        self.oled.show()


class BLENameMonitor:
    def __init__(self, display, filename=BLE_NAME_FILE):
        """Initialize the BLE name monitor"""
        self.display = display
        self.filename = filename
        self.last_name = None
        self.last_mtime = 0

    def read_ble_name(self):
        """Read the BLE name from file"""
        try:
            with open(self.filename, 'r') as f:
                name = f.read().strip()
                return name if name else None
        except OSError:
            # File doesn't exist
            return None
        except Exception as e:
            print(f"[ERROR] Failed to read {self.filename}: {e}")
            return None

    def get_file_mtime(self):
        """Get the modification time of the file"""
        try:
            stat = os.stat(self.filename)
            return stat[8]  # st_mtime
        except OSError:
            return 0

    def check_for_updates(self):
        """Check if the file has been updated and update display if needed"""
        current_mtime = self.get_file_mtime()
        name = self.read_ble_name()

        # Always update display to show current time and mtime
        if name:
            # Check if file was modified or name changed
            first_draw = False
            if current_mtime != self.last_mtime or name != self.last_name:
                print(f"[MONITOR] BLE name updated: {name} (mtime: {current_mtime})")
                self.last_name = name
                self.last_mtime = current_mtime
                first_draw = True  # Redraw everything on name change

            # Refresh display: full redraw on changes, timestamps only otherwise
            self.display.display_ble_name(name, current_mtime, first_draw=first_draw)
            return True

        return False

    def display_default_message(self):
        """Display a default message when no BLE name is set"""
        self.display.clear()
        self.display.oled.text("BLE Device Name:", 0, 0)

        # Draw separator
        for x in range(0, self.display.width):
            self.display.oled.pixel(x, 12, 1)

        self.display.display_centered_text("Not Set", 28)
        self.display.oled.text("Waiting for", 20, 44)
        self.display.oled.text("provisioning...", 8, 54)
        self.display.oled.show()


def main():
    """Main loop - monitor BLE name file and update OLED display"""
    print("=" * 50)
    print("ESP32 OLED Display for BLE Name")
    print("=" * 50)
    print("[INFO] Initializing...")

    try:
        # Initialize OLED display
        display = OLEDDisplay()

        # Initialize monitor
        monitor = BLENameMonitor(display)

        # Try to read initial BLE name
        initial_name = monitor.read_ble_name()

        if initial_name:
            print(f"[INFO] Current BLE name: {initial_name}")
            mtime = monitor.get_file_mtime()
            display.display_ble_name(initial_name, mtime, first_draw=True)
            monitor.last_name = initial_name
            monitor.last_mtime = mtime
        else:
            print("[INFO] No BLE name found, waiting for provisioning...")
            monitor.display_default_message()

        print("\n[READY] Monitoring for BLE name changes...")
        print("[INFO] Press Ctrl+C to exit\n")

        # Main monitoring loop
        while True:
            monitor.check_for_updates()
            time.sleep_ms(UPDATE_INTERVAL)

    except KeyboardInterrupt:
        print("\n[INFO] Shutting down...")
        display.clear()
        display.display_centered_text("Goodbye!", 28)
        time.sleep(1)
        display.clear()
        print("[INFO] Display cleared")
        print("[INFO] Goodbye!")

    except Exception as e:
        print(f"[ERROR] Unexpected error: {e}")
        raise


if __name__ == "__main__":
    main()
