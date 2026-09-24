
# 00 Top choice name: domino_double12_is31fl3741.py
"""
Display random double-12 dominoes on Adafruit IS31FL3741 13x9 RGB matrix.
- Layout: see double-12-requirements.md file
- Colors: standard double-12 scheme.
"""

import time
import random

import board
import neopixel
import adafruit_is31fl3741
from adafruit_is31fl3741.adafruit_rgbmatrixqt import Adafruit_RGBMatrixQT


# ----- Debug Config -----
SEQUENTIAL_DEBUG = False  # True    # Set to True to step through values 0-12 sequentially for debugging

# ----- Config -----
# Layout: Normal orientation, two 5x7 halves side-by-side with 1-pixel gutter
DOMINO_LEFT_X = 1          # left margin to center 11-wide domino (5+1+5) in 13 columns
DOMINO_TOP_Y = 1           # top margin to center 7-tall domino in 9 rows
DOMINO_WIDTH = 5           # width of each domino half
DOMINO_HEIGHT = 7          # height of each domino half
GUTTER_COL = DOMINO_LEFT_X + 5  # single-column gutter at x=6

# Pip colors (RGB)
pip_colors = {
    0: (0, 0, 0),         # blank
    1: (255, 255, 255),   # white (override: single pip must be visible)
    2: (0, 255, 0),       # green
    3: (255, 0, 0),       # red
    4: (0, 0, 255),       # blue
    5: (255, 255, 0),     # yellow
    6: (128, 0, 128),     # purple
    7: (139, 69, 19),     # brown
    8: (255, 192, 203),   # pink
    9: (255, 165, 0),     # orange
    10: (0, 128, 128),    # teal/aqua
    11: (173, 216, 230),  # light blue
    12: (255, 255, 255),  # white
}

# Pip coordinates for values 0..12 within a 5x7 grid (r,c), 0-indexed
# Exact patterns as specified: 5=2+1+2, 6=3+0+3, 7=3+1+3, 8=3+2+3, 9=3+3+3, 10=4+2+4, 11=4+3+4, 12=4+4+4
pip_positions = {
    0: [],  # blank
    1: [(3, 2)],  # center
    2: [(1, 1), (5, 3)],  # diagonal corners
    3: [(1, 1), (3, 2), (5, 3)],  # diagonal line
    4: [(1, 1), (1, 3), (5, 1), (5, 3)],  # four corners
    5: [(1, 1), (1, 3), (3, 2), (5, 1), (5, 3)],  # 2+1+2 pattern: 2 corners + 1 center + 2 corners
    6: [(1, 1), (1, 2), (1, 3), (5, 1), (5, 2), (5, 3)],  # 3+0+3 pattern: 3 top + 0 middle + 3 bottom
    7: [(1, 1), (1, 2), (1, 3), (3, 2), (5, 1), (5, 2), (5, 3)],  # 3+1+3 pattern: 3 top + 1 center + 3 bottom
    8: [(1, 1), (1, 2), (1, 3), (3, 1), (3, 3), (5, 1), (5, 2), (5, 3)],  # 3+2+3 pattern: 3 top + 2 middle + 3 bottom
    9: [(1, 1), (1, 2), (1, 3), (3, 1), (3, 2), (3, 3), (5, 1), (5, 2), (5, 3)],  # 3+3+3 pattern: 3 top + 3 middle + 3 bottom
    10: [(1, 0), (1, 1), (1, 2), (1, 3),
                 (3, 1), (3, 2),
         (5, 0), (5, 1), (5, 2), (5, 3)],  # 4+2+4 pattern: 4 top + 2 middle + 4 bottom
    11: [(1, 0), (1, 1), (1, 2), (1, 3),
                 (3, 1), (3, 2), (3, 3),
         (5, 0), (5, 1), (5, 2), (5, 3)],  # 4+3+4 pattern: 4 top + 3 middle + 4 bottom
    12: [(1, 0), (1, 1), (1, 2), (1, 3),
         (3, 0), (3, 1), (3, 2), (3, 3),
         (5, 0), (5, 1), (5, 2), (5, 3)],  # 4+4+4 pattern: 4 top + 4 middle + 4 bottom
}


def rgb_to_packed(rgb_tuple):
    """Convert RGB tuple to packed color value."""
    r, g, b = rgb_tuple
    return (r << 16) | (g << 8) | b


def clear(matrix):
    """Turn off all LEDs."""
    w, h = matrix.width, matrix.height
    for x in range(w):
        for y in range(h):
            matrix.pixel(x, y, 0)  # Use pixel method with packed color


def draw_border(matrix, origin_x, origin_y, width, height, color=(255, 255, 255)):
    """Draw a border around the specified area."""
    packed_color = rgb_to_packed(color)

    # Top and bottom borders
    for x in range(origin_x - 1, origin_x + width + 1):
        if 0 <= x < matrix.width:
            if 0 <= origin_y - 1 < matrix.height:
                matrix.pixel(x, origin_y - 1, packed_color)  # Top border
            if 0 <= origin_y + height < matrix.height:
                matrix.pixel(x, origin_y + height, packed_color)  # Bottom border

    # Left and right borders
    for y in range(origin_y - 1, origin_y + height + 1):
        if 0 <= y < matrix.height:
            if 0 <= origin_x - 1 < matrix.width:
                matrix.pixel(origin_x - 1, y, packed_color)  # Left border
            if 0 <= origin_x + width < matrix.width:
                matrix.pixel(origin_x + width, y, packed_color)  # Right border


def draw_half(matrix, value, origin_x, origin_y):
    """Draw a 5x7 pip field at (origin_x, origin_y) for the given value."""
    color = pip_colors.get(value, (255, 255, 255))
    packed_color = rgb_to_packed(color)
    for (r, c) in pip_positions[value]:
        x = origin_x + c
        y = origin_y + r
        matrix.pixel(x, y, packed_color)  # Use pixel method with packed color


def draw_domino(matrix, left_value, right_value):
    """Draw two 5x7 halves side-by-side with 1-col gutter, centered on 13x9 matrix."""
    clear(matrix)
    # Left half
    left_x = DOMINO_LEFT_X
    left_y = DOMINO_TOP_Y
    # Right half - skip gutter column
    right_x = GUTTER_COL + 1  # Start right half at column 7
    right_y = DOMINO_TOP_Y

    draw_half(matrix, left_value, left_x, left_y)
    draw_half(matrix, right_value, right_x, right_y)
    matrix.show()  # Update the display


def draw_domino_debug(matrix, value):
    """Draw single 5x7 half with border for debug mode, centered on matrix."""
    clear(matrix)
    print(f"Debug: Drawing single half for {value}")
    # Center the single domino half on the matrix
    debug_x = (matrix.width - DOMINO_WIDTH) // 2  # Center horizontally
    debug_y = DOMINO_TOP_Y

    # Draw the border around the pip area
    draw_border(matrix, debug_x, debug_y, DOMINO_WIDTH, DOMINO_HEIGHT, (32, 16, 16))  # XXX border

    # Draw the pips
    draw_half(matrix, value, debug_x, debug_y)
    matrix.show()  # Update the display


# ----- Init hardware -----
print("Initializing IS31FL3741 RGB Matrix...")

# Initialize I2C
i2c = board.I2C()  # uses board.SCL and board.SDA
print("I2C bus initialized")

# Initialize the IS31FL3741 as RGB Matrix (13x9 matrix)
matrix = Adafruit_RGBMatrixQT(i2c, address=0x30, allocate=adafruit_is31fl3741.PREFER_BUFFER)

# Configure the LED matrix
matrix.set_led_scaling(0xFF)  # Full LED scaling
matrix.global_current = 0x80  # Set to medium current to avoid overheating
matrix.enable = True          # Enable the matrix

print(f"IS31FL3741 initialized at address 0x30")
print(f"Matrix size: {matrix.width}x{matrix.height} pixels")
print(f"Global current: {matrix.global_current}")
print(f"Enabled: {matrix.enable}")

# Initialize NeoPixel
pixel = neopixel.NeoPixel(board.NEOPIXEL, 1)
pixel.brightness = 0.3
pixel[0] = (0, 255, 0)  # Green for success


def wait_for_input_with_timeout(is_first_wait=False):
    """Wait for ENTER key for up to 10 seconds initially, then auto-advance every 5 seconds."""
    import sys

    if is_first_wait:
        timeout = 10.0  # 10 seconds timeout for first input
        print("Press ENTER to advance (auto-advance in 10s)...")
    else:
        timeout = 5.0   # 5 seconds for subsequent auto-advances
        print("Press ENTER to advance (auto-advance in 5s)...")

    start_time = time.time()

    try:
        # Simple approach: try to read with timeout
        while True:
            elapsed = time.time() - start_time

            # Check if we've reached the timeout
            if elapsed >= timeout:
                print("Auto-advancing...")
                return

            # Try to read any available input (non-blocking)
            try:
                # This may not work perfectly in all CircuitPython versions
                if hasattr(sys.stdin, 'read'):
                    # Try a very short read
                    char = sys.stdin.read(1) if sys.stdin.readable() else None
                    if char and (char == '\n' or char == '\r' or len(char.strip()) > 0):
                        print("Input received, advancing...")
                        return
                    else:
                        print(char if char else ".")  # XXX
            except:
                pass

            # Small delay to prevent busy waiting
            time.sleep(0.1)

    except:
        # If input handling fails, just wait the timeout period
        time.sleep(timeout)
        print("Auto-advancing...")

# ----- Main loop -----
print("Double-12 Dominoes Display Started")

if SEQUENTIAL_DEBUG:
    print("DEBUG MODE: Stepping through values 0-12 sequentially")
    print("Press ENTER to advance to next domino")
    debug_counter = 0
    first_iteration = True
    total_iterations = 0
else:
    print("Displaying random domino combinations (0-12 | 0-12)")
    print("NeoPixel will blink between left and right domino colors")

print("=" * 40)

while True:
    if SEQUENTIAL_DEBUG:
        # Step through values sequentially for debugging, starting at 1|2
        # In debug mode, cycle through single values 0-12
        value = debug_counter % 13
        debug_counter += 1
        if debug_counter > 12:  # Reset after showing all values 0-12
            debug_counter = 0
    else:
        # Random selection as per requirements
        left = random.randint(0, 12)
        right = random.randint(0, 12)

    if SEQUENTIAL_DEBUG:
        print(f"Debug: {value:2d}")
        draw_domino_debug(matrix, value)

        # Get color for the single value
        color = pip_colors.get(value, (255, 255, 255))
        pixel[0] = color
        wait_for_input_with_timeout(is_first_wait=(total_iterations == 0))
        total_iterations += 1
    else:
        print(f"Domino: {left:2d} | {right:2d}")
        draw_domino(matrix, left, right)

        # Get colors for each half
        left_color = pip_colors.get(left, (255, 255, 255))
        right_color = pip_colors.get(right, (255, 255, 255))

        # Normal mode: Blink NeoPixel between the two colors (5 seconds total)
        # First half - left domino color
        pixel[0] = left_color
        time.sleep(2.5)

        # Second half - right domino color
        pixel[0] = right_color
        time.sleep(2.5)

        # end-0
