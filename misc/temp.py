"""
test/demo of LCD display via STEMMA_I2C
"""

import board
import digitalio
import displayio
import neopixel
import terminalio
import time
from adafruit_display_text import label
import adafruit_displayio_ssd1306
import microcontroller

pixel = neopixel.NeoPixel(board.NEOPIXEL, 1)

button = digitalio.DigitalInOut(board.BUTTON)
button.switch_to_input(pull=digitalio.Pull.UP)

displayio.release_displays()

i2c = board.STEMMA_I2C()
display_bus = displayio.I2CDisplay(i2c, device_address=0x3d)
display = adafruit_displayio_ssd1306.SSD1306(display_bus, width=128, height=64)

"""
output temperature and time, + poll button!
"""
while True:
    F = microcontroller.cpu.temperature * (9 / 5) + 32
    b = button.value
    print("\n%.1f\t %02d %s" % (F, (time.time() % 60),"" if b else "!"), end="")
    if not button.value:
        pixel.fill((255, 0, 0))
    else:
        pixel.fill((0, 0, 0))
    time.sleep(1.0)

"""
initialize the display using displayio and draw a solid white
background, a smaller black rectangle, and some white text.
"""
# Make the display context
splash = displayio.Group()
display.show(splash)

color_bitmap = displayio.Bitmap(128, 32, 1)
color_palette = displayio.Palette(1)
color_palette[0] = 0xFFFFFF  # White

bg_sprite = displayio.TileGrid(color_bitmap, pixel_shader=color_palette, x=0, y=0)
splash.append(bg_sprite)

# Draw a smaller inner rectangle
inner_bitmap = displayio.Bitmap(118, 24, 1)
inner_palette = displayio.Palette(1)
inner_palette[0] = 0x000000  # Black
inner_sprite = displayio.TileGrid(inner_bitmap, pixel_shader=inner_palette, x=5, y=4)
splash.append(inner_sprite)

# Draw a label
text = "Hello World!"
text_area = label.Label(terminalio.FONT, text=text, color=0xFFFF00, x=28, y=15)
splash.append(text_area)

while True:
    time.sleep(0.5)
#     display.show(splash)
    text = "%s" % time.time()
    print(text)
#     text_area = label.Label(terminalio.FONT, text=text, color=0xFFFF00, x=28, y=15)
#     splash.append(text_area)
