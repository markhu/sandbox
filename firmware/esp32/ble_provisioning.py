"""
ESP32 BLE Provisioning Script

This script accepts provisioning commands via the serial port to configure
the BLE advertising name for the ESP32 development board.

Command format: ble/name EXAMPLE-1234

The script will:
1. Listen for serial input on UART
2. Parse provisioning commands in the format "ble/name <NAME>"
3. Start BLE advertising with the specified name
4. Continuously advertise until a new provisioning command is received
"""

import bluetooth
import time
import sys
from micropython import const

# BLE event constants
_IRQ_CENTRAL_CONNECT = const(1)
_IRQ_CENTRAL_DISCONNECT = const(2)
_IRQ_GATTS_WRITE = const(3)

# Default BLE name
DEFAULT_BLE_NAME = "ESP32-Device"

class BLEProvisioning:
    def __init__(self):
        self.ble = bluetooth.BLE()
        self.ble.active(True)
        self.ble.irq(self._irq_handler)
        self.ble_name = DEFAULT_BLE_NAME
        self.is_advertising = False
        self.connected = False
        self.last_file_check = 0
        self.file_check_interval = 5000  # Check file every 5 seconds
        # Try to load saved name from file
        self._load_name_from_file()

    def _irq_handler(self, event, data):
        """Handle BLE events"""
        if event == _IRQ_CENTRAL_CONNECT:
            conn_handle, addr_type, addr = data
            self.connected = True
            print(f"[BLE] Device connected: {addr}")

        elif event == _IRQ_CENTRAL_DISCONNECT:
            conn_handle, addr_type, addr = data
            self.connected = False
            print(f"[BLE] Device disconnected: {addr}")
            # Restart advertising after disconnect
            self.start_advertising()

    def start_advertising(self):
        """Start BLE advertising with the current name"""
        try:
            # Stop any existing advertising
            if self.is_advertising:
                self.ble.gap_advertise(None)
                time.sleep_ms(100)

            # Create advertising payload
            # Flags: General discoverable mode
            payload = bytearray([
                0x02, 0x01, 0x06,  # Flags
            ])

            # Add complete local name
            name_bytes = self.ble_name.encode('utf-8')
            name_len = len(name_bytes) + 1
            payload.extend(bytearray([name_len, 0x09]))  # Complete local name
            payload.extend(name_bytes)

            # Start advertising (interval in microseconds: 100ms = 100000us)
            self.ble.gap_advertise(100000, adv_data=payload)
            self.is_advertising = True
            print(f"[BLE] Started advertising as: {self.ble_name}")

        except Exception as e:
            print(f"[ERROR] Failed to start advertising: {e}")
            self.is_advertising = False

    def stop_advertising(self):
        """Stop BLE advertising"""
        if self.is_advertising:
            try:
                self.ble.gap_advertise(None)
                self.is_advertising = False
                print("[BLE] Stopped advertising")
            except Exception as e:
                print(f"[ERROR] Failed to stop advertising: {e}")

    def set_name(self, name):
        """Set the BLE advertising name and restart advertising"""
        if not name or len(name) == 0:
            print("[ERROR] Invalid name - cannot be empty")
            return False

        if len(name) > 29:
            print(f"[WARNING] Name too long ({len(name)} chars), truncating to 29 chars")
            name = name[:29]

        old_name = self.ble_name
        self.ble_name = name

        print(f"[BLE] Name changed: {old_name} -> {self.ble_name}")

        # Write the new name to filesystem
        self._save_name_to_file()

        # Restart advertising with new name
        self.stop_advertising()
        time.sleep_ms(200)
        self.start_advertising()

        return True

    def _save_name_to_file(self):
        """Save the BLE name to a file on the ESP32 filesystem"""
        try:
            with open('ble_name.txt', 'w') as f:
                f.write(self.ble_name)
            print(f"[FILE] Saved name to ble_name.txt: {self.ble_name}")
        except Exception as e:
            print(f"[ERROR] Failed to save name to file: {e}")

    def _load_name_from_file(self):
        """Load the BLE name from file if it exists"""
        try:
            with open('ble_name.txt', 'r') as f:
                name = f.read().strip()
                if name:
                    self.ble_name = name
                    print(f"[FILE] Loaded name from ble_name.txt: {self.ble_name}")
                    return True
        except OSError:
            # File doesn't exist yet, use default
            print(f"[FILE] No saved name found, using default: {self.ble_name}")
        except Exception as e:
            print(f"[ERROR] Failed to load name from file: {e}")
        return False

    def check_file_for_updates(self):
        """Periodically check if ble_name.txt has been updated"""
        current_time = time.ticks_ms()

        # Check if enough time has passed since last check
        if time.ticks_diff(current_time, self.last_file_check) >= self.file_check_interval:
            self.last_file_check = current_time

            try:
                with open('ble_name.txt', 'r') as f:
                    name = f.read().strip()

                    # Print polling status
                    print(f"[POLL] Checking ble_name.txt... Current: {self.ble_name}, File: {name}")

                    # If name has changed, update it
                    if name and name != self.ble_name:
                        print(f"[POLL] Name changed detected!")
                        self.set_name(name)
                    else:
                        print(f"[POLL] No change detected")

            except OSError:
                print(f"[POLL] ble_name.txt not found")
            except Exception as e:
                print(f"[ERROR] Failed to check file: {e}")

    def process_command(self, command):
        """Process a provisioning command from serial input"""
        command = command.strip()

        if not command:
            return

        print(f"[CMD] Received: {command}")

        # Parse command format: ble/name EXAMPLE-1234
        if command.startswith("ble/name "):
            name = command[9:].strip()  # Extract name after "ble/name "

            if name:
                self.set_name(name)
            else:
                print("[ERROR] No name provided in command")
                print("[HELP] Usage: ble/name EXAMPLE-1234")
        else:
            print(f"[ERROR] Unknown command: {command}")
            print("[HELP] Available commands:")
            print("[HELP]   ble/name <NAME>  - Set BLE advertising name")

def main():
    """Main loop - initialize BLE and process serial commands"""
    print("=" * 50)
    print("ESP32 BLE Provisioning Script")
    print("=" * 50)
    print("[INFO] Starting BLE provisioning system...")

    # Create BLE provisioning instance
    ble_prov = BLEProvisioning()

    # Start with default name
    ble_prov.start_advertising()

    print("\n[READY] Waiting for provisioning commands...")
    print("[HELP] Send commands in format: ble/name EXAMPLE-1234")
    print("[HELP] Press Ctrl+C to exit\n")
    print("[INFO] Running in file-polling mode (checking ble_name.txt every 5 seconds)")
    print("")

    try:
        while True:
            # Check for file updates periodically
            ble_prov.check_file_for_updates()

            # Small delay to prevent busy waiting
            time.sleep_ms(100)

    except KeyboardInterrupt:
        print("\n[INFO] Shutting down...")
        ble_prov.stop_advertising()
        print("[INFO] BLE advertising stopped")
        print("[INFO] Goodbye!")
        print("[INFO] Goodbye!")


if __name__ == "__main__":
    # Use polling mode for better compatibility with MicroPython
    main()
