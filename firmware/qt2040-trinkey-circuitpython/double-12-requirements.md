

# Lava Domino

Requirements for a CircuitPython program for an Adafruit IS31FL3741 13×9 RGB LED matrix that displays random double-12 domino tiles. Requirements:

1. Each domino consists of two halves (left and right), each a 5-wide by 7-high pip grid, separated by a 1-pixel gutter column.
    - Note that due to the orientation of the 13×9 RGB LED matrix, each half of the dominue is to be displayed sideways.
    - The allowable pip positions allways allow a blank pixel in between each pip, both
2. The program should randomly pick two values from 0 to 12 (inclusive), representing pip counts.
3. Draw pips in the standard double-12 symmetrical pip layout, e.g. 2 and 3 are "diagonal"
    - 5 is 2 + 1 + 2 pattern with extra empty lines, to fill out to the corners.
    - 6 is 3 + 0 + 3
    - 7 is 3 + 1 + 3
    - 8 is 3 + 2 + 3
    - 9 is 3 + 3 + 3
    - 10 is 4 + 2 + 4
    - 11 is 4 + 3 + 4
    - 12 is 4 + 4 + 4
4. Light pip LEDs using the following color scheme:
    - 0: blank (no pips)
    - 1: white
    - 2: green
    - 3: red
    - 4: blue
    - 5: yellow
    - 6: purple
    - 7: brown
    - 8: pink
    - 9: orange
    - 10: teal/aqua
    - 11: light blue
    - 12: white
5. Background stays off (all LEDs unlit except pips).
6. After displaying a tile for ~2-5 seconds, draw another random domino --turning off any pips as needed.
7. Organize the code so there’s a reusable function like draw_domino(left_value, right_value).
8. Blink the neopixel to reflect the color of each half.
9. After any code-changes, deploy with deploy.sh FILENAME, and monitor using the serial_monitor.py script.
10. emit domino numeric values to the serial port
11. If the SEQUENTIAL_DEBUG flag is defined/true, then serial port should prompt user to hit a key to advance numerically in order starting at 1|2.
