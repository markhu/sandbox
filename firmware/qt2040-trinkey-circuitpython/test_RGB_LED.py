#!/usr/bin/python3
"""
RGB LED Test - Works on both QT2040 Trinkey and RP2350 boards
Auto-detects board type and uses appropriate LED interface
"""

import time
import board

print("Starting RGB LED test...")

# Detect board capabilities and initialize appropriate LED interface
led_type = None
pixel = None
led_r = led_g = led_b = None

# Try NeoPixel first (QT2040 Trinkey)
try:
    import neopixel
    if hasattr(board, 'NEOPIXEL'):
        pixel = neopixel.NeoPixel(board.NEOPIXEL, 1)
        pixel.brightness = 0.3
        led_type = "neopixel"
        print("✓ Detected NeoPixel (QT2040 Trinkey)")
except (ImportError, AttributeError):
    pass

# Try discrete RGB LEDs (RP2350 Pimoroni Tiny2350)
if led_type is None:
    try:
        import digitalio
        if hasattr(board, 'LED_R') and hasattr(board, 'LED_G') and hasattr(board, 'LED_B'):
            led_r = digitalio.DigitalInOut(board.LED_R)
            led_r.direction = digitalio.Direction.OUTPUT
            led_g = digitalio.DigitalInOut(board.LED_G)
            led_g.direction = digitalio.Direction.OUTPUT
            led_b = digitalio.DigitalInOut(board.LED_B)
            led_b.direction = digitalio.Direction.OUTPUT
            led_type = "discrete_rgb"
            print("✓ Detected discrete RGB LEDs (RP2350)")
    except (ImportError, AttributeError):
        pass

# Fallback to single LED
if led_type is None:
    try:
        import digitalio
        if hasattr(board, 'LED'):
            led_g = digitalio.DigitalInOut(board.LED)
            led_g.direction = digitalio.Direction.OUTPUT
            led_type = "single_led"
            print("✓ Detected single LED")
    except (ImportError, AttributeError):
        pass

if led_type is None:
    print("✗ No LED detected on this board!")
    raise RuntimeError("No compatible LED found")

def set_color(r, g, b):
    """Set LED color - works with both NeoPixel and discrete RGB"""
    if led_type == "neopixel":
        pixel[0] = (r, g, b)
    elif led_type == "discrete_rgb":
        # Discrete LEDs: 0=on, 1=off (inverted logic on many boards)
        led_r.value = not (r > 0)
        led_g.value = not (g > 0)
        led_b.value = not (b > 0)
    elif led_type == "single_led":
        # Single LED: just blink for any non-zero color
        led_g.value = (r > 0 or g > 0 or b > 0)

print("Test running...")
print("Cycling through colors: Red → Green → Blue")

counter = 0
colors = [
    (255, 0, 0),    # Red
    (0, 255, 0),    # Green
    (0, 0, 255),    # Blue
    (255, 255, 0),  # Yellow
    (255, 0, 255),  # Magenta
    (0, 255, 255),  # Cyan
    (255, 255, 255) # White
]

color_names = ["Red", "Green", "Blue", "Yellow", "Magenta", "Cyan", "White"]

while True:
    color_idx = counter % len(colors)
    set_color(*colors[color_idx])
    print(f"Loop {counter}: {color_names[color_idx]}")
    counter += 1
    
    time.sleep(2)
    
    if counter >= 14:  # Two full cycles
        break

# Turn off LED
set_color(0, 0, 0)
print("Test completed!")
