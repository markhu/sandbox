import time
import board
import neopixel

print("Starting minimal test...")

# Set up the built-in NeoPixel
pixel = neopixel.NeoPixel(board.NEOPIXEL, 1)
pixel.brightness = 0.5

print("NeoPixel initialized")

# Simple test - just set to green and stay there
pixel[0] = (0, 255, 0)
print("Set to green")

# Simple loop - just flash between green and blue
while True:
    pixel[0] = (0, 255, 0)  # Green
    time.sleep(1)
    pixel[0] = (0, 0, 255)  # Blue
    time.sleep(1)
    print("Loop iteration")
