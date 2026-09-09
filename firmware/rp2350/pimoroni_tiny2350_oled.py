#!/usr/bin/env python3
import sys

# Check if running on CircuitPython
try:
    from rp2350_helpers import require_circuitpython
    require_circuitpython("pimoroni_tiny2350_oled.py")
except ImportError:
    # Running on CircuitPython, helper module not available
    pass

import board
import busio
import time
import digitalio
from micropython import const

# Simple SSD1306 driver using framebuffer
class SSD1306:
    SET_CONTRAST = const(0x81)
    SET_ENTIRE_ON = const(0xA4)
    SET_NORM_INV = const(0xA6)
    SET_DISP = const(0xAE)
    SET_MEM_ADDR = const(0x20)
    SET_COL_ADDR = const(0x21)
    SET_PAGE_ADDR = const(0x22)
    SET_DISP_START_LINE = const(0x40)
    SET_SEG_REMAP = const(0xA0)
    SET_MUX_RATIO = const(0xA8)
    SET_COM_OUT_DIR = const(0xC0)
    SET_DISP_OFFSET = const(0xD3)
    SET_COM_PIN_CFG = const(0xDA)
    SET_DISP_CLK_DIV = const(0xD5)
    SET_PRECHARGE = const(0xD9)
    SET_VCOM_DESEL = const(0xDB)
    SET_CHARGE_PUMP = const(0x8D)

    def __init__(self, width, height, i2c, addr=0x3C):
        self.i2c = i2c
        self.addr = addr
        self.width = width
        self.height = height
        self.pages = self.height // 8
        self.buffer = bytearray(self.pages * self.width)
        self.init_display()

    def write_cmd(self, cmd):
        while not self.i2c.try_lock():
            pass
        try:
            self.i2c.writeto(self.addr, bytes([0x00, cmd]))
        finally:
            self.i2c.unlock()

    def write_data(self, buf):
        while not self.i2c.try_lock():
            pass
        try:
            # Write in chunks
            chunk_size = 16
            for i in range(0, len(buf), chunk_size):
                chunk = buf[i:i+chunk_size]
                self.i2c.writeto(self.addr, b'\x40' + chunk)
        finally:
            self.i2c.unlock()

    def init_display(self):
        for cmd in (
            self.SET_DISP | 0x00,  # off
            self.SET_MEM_ADDR, 0x00,  # horizontal
            self.SET_DISP_START_LINE | 0x00,
            self.SET_SEG_REMAP | 0x01,  # column 127 mapped to SEG0
            self.SET_MUX_RATIO, self.height - 1,
            self.SET_COM_OUT_DIR | 0x08,  # scan from COM[N] to COM0
            self.SET_DISP_OFFSET, 0x00,
            self.SET_COM_PIN_CFG, 0x02 if self.height == 32 else 0x12,
            self.SET_DISP_CLK_DIV, 0x80,
            self.SET_PRECHARGE, 0xF1,
            self.SET_VCOM_DESEL, 0x30,
            self.SET_CONTRAST, 0xFF,
            self.SET_ENTIRE_ON,
            self.SET_NORM_INV,
            self.SET_CHARGE_PUMP, 0x14,
            self.SET_DISP | 0x01):  # on
            self.write_cmd(cmd)
        self.fill(0)
        self.show()

    def fill(self, c):
        self.buffer[:] = bytes([c & 0xFF] * len(self.buffer))

    def pixel(self, x, y, c):
        if 0 <= x < self.width and 0 <= y < self.height:
            index = x + (y // 8) * self.width
            bit = y % 8
            if c:
                self.buffer[index] |= 1 << bit
            else:
                self.buffer[index] &= ~(1 << bit)

    def text(self, string, x, y, c=1):
        # Simple 5x7 font
        for char in string:
            self.char(char, x, y, c)
            x += 6

    def char(self, c, x, y, color=1):
        # Very basic 5x7 ASCII font
        font = {
            '0': [0x3E, 0x51, 0x49, 0x45, 0x3E],
            '1': [0x00, 0x42, 0x7F, 0x40, 0x00],
            '2': [0x42, 0x61, 0x51, 0x49, 0x46],
            '3': [0x21, 0x41, 0x45, 0x4B, 0x31],
            '4': [0x18, 0x14, 0x12, 0x7F, 0x10],
            '5': [0x27, 0x45, 0x45, 0x45, 0x39],
            '6': [0x3C, 0x4A, 0x49, 0x49, 0x30],
            '7': [0x01, 0x71, 0x09, 0x05, 0x03],
            '8': [0x36, 0x49, 0x49, 0x49, 0x36],
            '9': [0x06, 0x49, 0x49, 0x29, 0x1E],
            'A': [0x7E, 0x11, 0x11, 0x11, 0x7E],
            'B': [0x7F, 0x49, 0x49, 0x49, 0x36],
            'C': [0x3E, 0x41, 0x41, 0x41, 0x22],
            'D': [0x7F, 0x41, 0x41, 0x22, 0x1C],
            'E': [0x7F, 0x49, 0x49, 0x49, 0x41],
            'F': [0x7F, 0x09, 0x09, 0x09, 0x01],
            'G': [0x3E, 0x41, 0x49, 0x49, 0x7A],
            'H': [0x7F, 0x08, 0x08, 0x08, 0x7F],
            'I': [0x00, 0x41, 0x7F, 0x41, 0x00],
            'P': [0x7F, 0x09, 0x09, 0x09, 0x06],
            'R': [0x7F, 0x09, 0x19, 0x29, 0x46],
            'S': [0x46, 0x49, 0x49, 0x49, 0x31],
            'T': [0x01, 0x01, 0x7F, 0x01, 0x01],
            'W': [0x7F, 0x20, 0x18, 0x20, 0x7F],
            'X': [0x63, 0x14, 0x08, 0x14, 0x63],
            'Y': [0x07, 0x08, 0x70, 0x08, 0x07],
            ' ': [0x00, 0x00, 0x00, 0x00, 0x00],
            ':': [0x00, 0x36, 0x36, 0x00, 0x00],
            '!': [0x00, 0x00, 0x5F, 0x00, 0x00],
            'x': [0x44, 0x28, 0x10, 0x28, 0x44],
            's': [0x48, 0x54, 0x54, 0x54, 0x20],
        }
        c = c.upper()
        if c not in font:
            c = ' '
        for col, bits in enumerate(font[c]):
            for row in range(8):
                if bits & (1 << row):
                    self.pixel(x + col, y + row, color)

    def show(self):
        self.write_cmd(self.SET_COL_ADDR)
        self.write_cmd(0)
        self.write_cmd(self.width - 1)
        self.write_cmd(self.SET_PAGE_ADDR)
        self.write_cmd(0)
        self.write_cmd(self.pages - 1)
        self.write_data(self.buffer)

# Initialize RGB LED
led_g = digitalio.DigitalInOut(board.LED_G)
led_g.direction = digitalio.Direction.OUTPUT
print("Green LED initialized")

# Initialize I2C
i2c = busio.I2C(board.SCL, board.SDA)
print("I2C initialized")

# Scan I2C
def scan_i2c():
    devices = []
    while not i2c.try_lock():
        pass
    try:
        for addr in range(0x08, 0x78):
            try:
                i2c.writeto(addr, b'')
                devices.append(addr)
            except OSError:
                pass
    finally:
        i2c.unlock()
    return devices

devices = scan_i2c()
print(f"I2C devices: {[hex(a) for a in devices]}")

# Initialize OLED - try both sizes starting with 32
oled = None
for height in [32, 64]:
    try:
        test_oled = SSD1306(128, height, i2c, addr=0x3C)
        # Test if display responds correctly
        test_oled.fill(0)
        test_oled.show()
        oled = test_oled
        print(f"OLED 128x{height} initialized!")
        break
    except Exception as e:
        print(f"Failed 128x{height}: {e}")

if oled:
    oled.fill(0)
    oled.text("RP2350 READY", 5, 5)
    oled.text("I2C: 0x3C", 5, 18)
    oled.show()
    print(f"Display updated! Size: {oled.width}x{oled.height}")
    led_g.value = True
    time.sleep(3)  # Longer pause to see initial message
    led_g.value = False

# Main loop
print("Starting main loop...")
counter = 0
while True:
    counter += 1
    led_g.value = True
    time.sleep(0.1)
    led_g.value = False
    
    if oled:
        oled.fill(0)
        oled.text(f"COUNT {counter}", 5, 4)
        oled.text(f"TIME {int(time.monotonic())}s", 5, 16)
        oled.show()
    
    print(f"Loop {counter}")
    time.sleep(5)
