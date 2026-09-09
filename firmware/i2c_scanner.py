#!/usr/bin/env python3
"""
Smart I2C Scanner - Self-deploying script for ESP32
Detects if running on host (Mac) or target (ESP32) and acts accordingly
"""

import sys
import time
import argparse

# Detect environment by trying to import machine module
try:
    from machine import Pin, I2C
    RUNNING_ON_ESP32 = True
except ImportError:
    RUNNING_ON_ESP32 = False

# ============================================================================
# ESP32 CODE - Runs when deployed to the device
# ============================================================================
if RUNNING_ON_ESP32:
    print("\n" + "="*50)
    print("Running on ESP32 - Starting I2C scan...")
    print("="*50 + "\n")
    
    # Common I2C pin configurations for ESP32 dev boards
    configs = [
        {"scl": 22, "sda": 21, "freq": 400000, "name": "Standard ESP32"},
        {"scl": 15, "sda": 4, "freq": 400000, "name": "OLED variant 1"},
        {"scl": 5, "sda": 4, "freq": 400000, "name": "OLED variant 2"},
        {"scl": 14, "sda": 2, "freq": 400000, "name": "Alternative config"},
    ]
    
    found_any = False
    
    for idx, config in enumerate(configs):
        try:
            print(f"Config {idx+1} ({config['name']}): SCL=GPIO{config['scl']}, SDA=GPIO{config['sda']}")
            i2c = I2C(0, scl=Pin(config['scl']), sda=Pin(config['sda']), freq=config['freq'])
            
            devices = i2c.scan()
            
            if devices:
                found_any = True
                print(f"  ✓ SUCCESS! Found {len(devices)} device(s):")
                for device in devices:
                    print(f"    • I2C Address: 0x{device:02X} (decimal {device})")
                    
                    # Identify common devices
                    if device in [0x3C, 0x3D]:
                        print(f"      → SSD1306 OLED Display (128x64 or 128x32)")
                    elif device == 0x78:
                        print(f"      → Possible OLED (7-bit shifted address)")
                    elif device in [0x68, 0x69]:
                        print(f"      → MPU6050 or DS3231 RTC")
                    elif device == 0x76 or device == 0x77:
                        print(f"      → BMP280/BME280 Sensor")
                print()
            else:
                print(f"  ✗ No devices found\n")
                
        except Exception as e:
            print(f"  ✗ Error: {e}\n")
    
    if not found_any:
        print("⚠ No I2C devices detected on any configuration")
        print("  Check: 1) Device is powered, 2) Correct pins, 3) Pull-up resistors")
    
    print("="*50)
    print("Scan complete!")
    print("="*50)

# ============================================================================
# HOST CODE - Runs on Mac/Linux to deploy to ESP32
# ============================================================================
else:
    import os
    import glob
    
    # Parse command line arguments
    parser = argparse.ArgumentParser(
        description='Smart I2C Scanner - Self-deploying script for ESP32',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  %(prog)s                    # Auto-detect and scan ESP32
  %(prog)s --port /dev/ttyUSB0  # Use specific port
  %(prog)s --baud 9600        # Use different baud rate

Features:
  • Automatically detects connected ESP32 development boards
  • Displays board information before connecting
  • Uploads and executes I2C scanner on the device
  • Scans multiple common pin configurations
  • Identifies known I2C devices (OLED, sensors, etc.)

Requirements:
  • Python 3.x
  • pyserial (install: pip3 install pyserial)
  • ESP32 with MicroPython firmware
        '''
    )
    parser.add_argument('--port', '-p', 
                        help='Serial port (default: auto-detect)')
    parser.add_argument('--baud', '-b', type=int, default=115200,
                        help='Baud rate (default: 115200)')
    
    args = parser.parse_args()
    
    print("\n" + "="*50)
    print("Smart I2C Scanner - Host Mode")
    print("="*50 + "\n")
    
    # Auto-detect ESP32 serial port
    def find_esp32_port():
        """Find ESP32 serial port on Mac/Linux"""
        patterns = [
            '/dev/tty.usbserial*',
            '/dev/tty.SLAB_USBtoUART*',
            '/dev/tty.wchusbserial*',
            '/dev/cu.usbserial*',
        ]
        
        for pattern in patterns:
            ports = glob.glob(pattern)
            if ports:
                return ports[0]
        return None
    
    def identify_board(port_name):
        """Identify the type of dev board based on port name"""
        if 'usbserial' in port_name.lower():
            if 'SLAB' in port_name or 'CP210' in port_name:
                return "ESP32 DevKit (CP2102 USB-to-UART)"
            elif 'wchusbserial' in port_name:
                return "ESP32 DevKit (CH340 USB-to-UART)"
            else:
                return "ESP32 DevKit (Generic USB-to-UART)"
        return "Unknown ESP32/USB Serial Device"
    
    if args.port:
        port = args.port
        print(f"✓ Using specified port: {port}")
    else:
        port = find_esp32_port()
    
    if not port:
        print("❌ No ESP32 detected!")
        print("\nSearching for USB serial devices:")
        all_ports = glob.glob('/dev/tty.*usb*') + glob.glob('/dev/cu.*usb*')
        if all_ports:
            print("Found these USB devices:")
            for p in all_ports:
                print(f"  • {p}")
            port = all_ports[0]
            print(f"\n⚠ Using first device: {port}")
        else:
            print("  (none found)")
            print("\nPlease:")
            print("  1. Connect your ESP32 via USB")
            print("  2. Install USB drivers if needed (CH340, CP2102, etc.)")
            print("  3. Run with --help for more options")
            sys.exit(1)
    
    # Display detected board information
    board_type = identify_board(port)
    print(f"✓ Detected: {board_type}")
    print(f"  Port: {port}")
    print(f"  Baud rate: {args.baud}")
    print("\nReady to upload I2C scanner to device.")
    
    # Prompt user to continue
    try:
        response = input("\nPress ENTER to continue or Ctrl-C to cancel... ")
    except KeyboardInterrupt:
        print("\n\n⚠ Cancelled by user")
        sys.exit(0)
    
    # Import serial library
    try:
        import serial
    except ImportError:
        print("\n❌ PySerial not installed!")
        print("Run: pip3 install --user pyserial")
        sys.exit(1)
    
    print(f"\n✓ Connecting to device...")
    
    try:
        ser = serial.Serial(port, args.baud, timeout=2)
        time.sleep(0.5)
        
        # Interrupt any running program
        print("✓ Interrupting current program...")
        ser.write(b'\x03\x03')  # Ctrl-C twice
        time.sleep(0.5)
        ser.read(ser.in_waiting)  # Clear buffer
        
        # Enter paste mode for reliable multi-line upload
        print("✓ Entering paste mode...")
        ser.write(b'\x05')  # Ctrl-E: paste mode
        time.sleep(0.3)
        
        # Upload only the ESP32 code section
        print("✓ Uploading I2C scanner code...")
        esp32_code = '''
from machine import Pin, I2C
import time

print("\\n" + "="*50)
print("Running on ESP32 - Starting I2C scan...")
print("="*50 + "\\n")

# Common I2C pin configurations for ESP32 dev boards
configs = [
    {"scl": 22, "sda": 21, "freq": 400000, "name": "Standard ESP32"},
    {"scl": 15, "sda": 4, "freq": 400000, "name": "OLED variant 1"},
    {"scl": 5, "sda": 4, "freq": 400000, "name": "OLED variant 2"},
    {"scl": 14, "sda": 2, "freq": 400000, "name": "Alternative config"},
]

found_any = False

for idx, config in enumerate(configs):
    try:
        print(f"Config {idx+1} ({config['name']}): SCL=GPIO{config['scl']}, SDA=GPIO{config['sda']}")
        i2c = I2C(0, scl=Pin(config['scl']), sda=Pin(config['sda']), freq=config['freq'])
        
        devices = i2c.scan()
        
        if devices:
            found_any = True
            print(f"  ✓ SUCCESS! Found {len(devices)} device(s):")
            for device in devices:
                print(f"    • I2C Address: 0x{device:02X} (decimal {device})")
                
                # Identify common devices
                if device in [0x3C, 0x3D]:
                    print(f"      → SSD1306 OLED Display (128x64 or 128x32)")
                elif device == 0x78:
                    print(f"      → Possible OLED (7-bit shifted address)")
                elif device in [0x68, 0x69]:
                    print(f"      → MPU6050 or DS3231 RTC")
                elif device == 0x76 or device == 0x77:
                    print(f"      → BMP280/BME280 Sensor")
            print()
        else:
            print(f"  ✗ No devices found\\n")
            
    except Exception as e:
        print(f"  ✗ Error: {e}\\n")

if not found_any:
    print("⚠ No I2C devices detected on any configuration")
    print("  Check: 1) Device is powered, 2) Correct pins, 3) Pull-up resistors")

print("="*50)
print("Scan complete!")
print("="*50)
'''
        
        ser.write(esp32_code.encode('utf-8'))
        time.sleep(0.2)
        
        # Exit paste mode and execute
        ser.write(b'\x04')  # Ctrl-D: execute
        time.sleep(0.5)
        
        print("✓ Executing on ESP32...\n")
        print("="*50)
        print("ESP32 OUTPUT:")
        print("="*50)
        
        # Read output for 5 seconds
        start_time = time.time()
        output_buffer = ""
        
        while time.time() - start_time < 5:
            if ser.in_waiting:
                chunk = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                output_buffer += chunk
                print(chunk, end='', flush=True)
            time.sleep(0.1)
        
        print("\n" + "="*50)
        
        # Check if we got meaningful output
        if "I2C Address:" in output_buffer or "0x" in output_buffer:
            print("\n✅ Scan completed successfully!")
        elif "Error" in output_buffer or "not found" in output_buffer.lower():
            print("\n⚠ Scan completed with errors (see output above)")
        else:
            print("\n⚠ Unexpected output - device may need reset")
        
        ser.close()
        
    except serial.SerialException as e:
        print(f"\n❌ Serial error: {e}")
        print("\nTroubleshooting:")
        print("  1. Check if another program is using the port")
        print("  2. Try pressing the EN/RST button on the ESP32")
        print("  3. Disconnect and reconnect the USB cable")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\n⚠ Interrupted by user")
        ser.close()
        sys.exit(0)
