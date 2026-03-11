#!/usr/bin/env python3
import sys

# Check if running on CircuitPython
try:
    from rp2350_helpers import detect_circuitpython
    detect_circuitpython(called_by="rp2350_code.py")
except ImportError:
    # Running on CircuitPython, helper module not available
    pass

import board
import busio
import time
import digitalio

# Try to import optional display module
try:
    import adafruit_ssd1306
except ImportError:
    adafruit_ssd1306 = None

# Log file (CircuitPython filesystem root)
log_file = "boot_out.txt"

def log(message):
    """Log message to file and console"""
    timestamp = time.time()
    log_msg = f"[{timestamp:.1f}] {message}"
    print(log_msg)
    try:
        with open(log_file, "a") as f:
            f.write(log_msg + "\n")
    except Exception as e:
        print(f"Log error: {e}")

# Initialize I2C
i2c = None
try:
    i2c = busio.I2C(board.SCL, board.SDA)
except (AttributeError, RuntimeError, OSError) as e:
    log(f"I2C initialization failed: {e}")

# Initialize LED (Pimoroni Tiny2350 has an onboard LED on GPIO25)
led = None
try:
    led = digitalio.DigitalInOut(board.LED)
    led.direction = digitalio.Direction.OUTPUT
except AttributeError as e:
    log(f"LED initialization failed: {e}")

# OLED display initialization (0x3C is typical I2C address for 0.91" 128x32 displays)
oled = None
oled_width = 128
oled_height = 32
oled_address = 0x3C

def scan_i2c():
    """Scan and return list of I2C addresses"""
    if i2c is None:
        return []
    devices = []
    while not i2c.try_lock():
        pass
    try:
        for address in range(0x08, 0x78):
            try:
                i2c.writeto(address, b'')
                devices.append(address)
            except (OSError, Exception):
                pass
    finally:
        i2c.unlock()
    return devices

def initialize_oled():
    """Initialize OLED display if available"""
    global oled
    if adafruit_ssd1306 is None or i2c is None:
        log("OLED driver or I2C not available")
        return False
    try:
        oled = adafruit_ssd1306.SSD1306_I2C(oled_width, oled_height, i2c, addr=oled_address)
        log("OLED display detected and initialized!")
        return True
    except Exception as e:
        log(f"OLED not found at 0x{oled_address:02x}: {e}")
        return False

def display_message(text):
    """Display message on OLED"""
    if oled is None:
        return
    try:
        oled.fill(0)
        oled.text(text, 0, 0, 1)
        oled.show()
    except Exception as e:
        log(f"OLED display error: {e}")

def blink_led(times=3, duration=0.2):
    """Blink LED"""
    if led is None:
        return
    for _ in range(times):
        led.value = True
        time.sleep(duration)
        led.value = False
        time.sleep(duration)

log("CircuitPython I2C Scanner Started")
log("=" * 40)

# Scan for I2C devices
devices = scan_i2c()

if devices:
    log(f"Found {len(devices)} I2C device(s):")
    for addr in devices:
        log(f"  0x{addr:02x}")
    blink_led(len(devices), 0.3)
else:
    log("No I2C devices found")
    blink_led(5, 0.1)  # Fast blink if no devices

# Try to initialize OLED
if initialize_oled():
    display_message("RP2350 Ready!")
    blink_led(2, 0.2)

# Scan for a few seconds then exit
log("Scanning for 5 seconds...")
start_time = time.time()
while time.time() - start_time < 5:
    time.sleep(1)
    devices = scan_i2c()
    device_addrs = ', '.join([f'0x{a:02x}' for a in devices]) if devices else 'none'
    log(f"Found {len(devices)} device(s): {device_addrs}")
    if 0x3C in devices:
        display_message("OLED: Online")
    else:
        if oled is not None:
            log("OLED display disconnected")

log("Scan complete, exiting.")
