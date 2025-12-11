# Pimoroni Tiny 2350 Development Guide

## Quick Start

**Device:** Pimoroni Tiny 2350 (RP2350 dual-core microcontroller)
**Firmware:** CircuitPython 10.0.3
**Location:** `/dev/sdc` (when mounted) or `/mnt/rp2350/` (CIRCUITPY filesystem)
**USB Port:** /dev/ttyACM0

## Device Status

- ✅ CircuitPython firmware installed
- ✅ OLED display support libraries available (adafruit_ssd1306.mpy, adafruit_displayio_ssd1306.mpy)
- ✅ I2C scanner and display utilities configured
- ✅ Font file (font5x8.bin) available

## Current Code

**Active script:** `code.py` on device (copied from `rp2350_code.py`)

The script:
1. Detects CircuitPython environment on RP2350
2. Initializes I2C bus (board.SCL, board.SDA)
3. Initializes onboard LED (GPIO25)
4. Attempts to initialize OLED display at I2C address 0x3C (0.91" 128x32)
5. Scans for I2C devices and logs results to boot_out.txt
6. Displays status on OLED ("RP2350 Ready!", "OLED: Online", etc.)
7. Blinks LED based on number of devices found

**Backup:** `code.py.bak` contains original I2C scanner code (from CircuitPython first boot)

## Filesystem Layout

```
/mnt/rp2350/
├── code.py                    # Active CircuitPython script
├── code.py.bak               # Original code (I2C scanner)
├── boot_out.txt              # Boot log and runtime output
├── settings.toml             # CircuitPython settings
├── font5x8.bin               # Bitmap font for OLED
├── lib/
│   ├── adafruit_ssd1306.mpy
│   ├── adafruit_displayio_ssd1306.mpy
│   ├── adafruit_bus_device/
│   └── adafruit_display_text/
└── sd/                        # SD card mount point (if available)
```

## Mounting the Device

### Automatic Mount (Recommended)
```bash
# Device should auto-mount to /mnt/rp2350 when connected
lsblk  # Verify /dev/sdc appears
```

### Manual Mount
```bash
# Enter bootloader mode: Hold BOOTSEL, press RESET
# Then mount:
sudo mkdir -p /mnt/rp2350
sudo mount /dev/sdc1 /mnt/rp2350

# Unmount when done:
sudo umount /mnt/rp2350
```

## Uploading Code via USB

### Method 1: Direct File Copy (Fastest)
```bash
# Mount the device
sudo mount /dev/sdc1 /mnt/rp2350

# Copy your script
cp my_script.py /mnt/rp2350/code.py
# or rename to code.py if it's not already named that

# Unmount
sudo umount /mnt/rp2350
```

### Method 2: Using ampy (Serial/REPL)
```bash
# Requires adafruit-ampy: pip install adafruit-ampy

# Check port (usually /dev/ttyACM0):
ls /dev/ttyACM*

# Upload file:
ampy --port /dev/ttyACM0 put my_script.py

# List files on device:
ampy --port /dev/ttyACM0 ls

# Run REPL:
ampy --port /dev/ttyACM0 repl
```

## Troubleshooting

### Device not showing in lsblk
- Check USB cable is data-capable (not charging-only)
- Check dmesg for USB errors: `dmesg | grep -i "rp2350\|pimoroni"`
- Press RESET button on board
- Hold BOOTSEL then press RESET to force bootloader mode

### Read-only filesystem error
- Device may still be in bootloader mode
- Press RESET to boot into CircuitPython normally
- CIRCUITPY filesystem should then be writable

### OLED not initializing
- Verify OLED is connected on I2C (address 0x3C)
- Run I2C scan to check: `ampy --port /dev/ttyACM0 repl` then:
  ```python
  import board, busio
  i2c = busio.I2C(board.SCL, board.SDA)
  while not i2c.try_lock(): pass
  for addr in range(0x08, 0x78):
      try:
          i2c.writeto(addr, b'')
          print(f"Device at 0x{addr:02x}")
      except: pass
  i2c.unlock()
  ```

### Serial connection not working
```bash
# Install pyserial if needed:
pip install pyserial

# Try other common ports:
ls /dev/ttyACM* /dev/ttyUSB* /dev/tty.usbmodem*
```

## Key Files in Source Directory

- **rp2350_code.py** - Main application source (deployed as code.py)
- **disp.py, disp2.py** - Display/graphics utilities
- **oled_ip.py** - OLED IP display utility
- **oled_status.py** - OLED status display utility
- **rb_sensor_driver.py** - Sensor driver module
- **query_lorawan_device.py** - LoRaWAN utilities
- **rp2350_files/** - Precompiled libs and resources for deployment
  - lib/ - CircuitPython library modules
  - font5x8.bin - Bitmap font file

## Useful CircuitPython Documentation

- [CircuitPython Welcome](https://learn.adafruit.com/welcome-to-circuitpython/)
- [CircuitPython API Reference](https://circuitpython.readthedocs.io/)
- [SSD1306 OLED Library](https://github.com/adafruit/Adafruit_CircuitPython_SSD1306)
- [Pimoroni Tiny 2350 Pinout](https://shop.pimoroni.com/products/tiny-2350)

## Common Tasks

### Edit and Deploy
```bash
# 1. Edit code.py on your computer
nano code.py

# 2. Mount device
sudo mount /dev/sdc1 /mnt/rp2350

# 3. Copy updated code
cp code.py /mnt/rp2350/

# 4. Unmount
sudo umount /mnt/rp2350

# 5. Check boot_out.txt for logs:
sudo cat /mnt/rp2350/boot_out.txt
```

### View Serial Output
```bash
# Use picocom or miniterm to view REPL output
pip install pyserial
python -m serial.tools.miniterm /dev/ttyACM0 115200

# Or with picocom:
sudo apt install picocom
sudo picocom /dev/ttyACM0 -b 115200
# Exit: Ctrl+A, then Ctrl+X
```

### Add New Libraries
1. Find library in [CircuitPython Bundle](https://github.com/adafruit/Adafruit_CircuitPython_Bundle)
2. Extract .mpy file to local rp2350_files/lib/
3. Mount device and copy to /mnt/rp2350/lib/
4. Unmount and restart device

## Hardware Info

- **Microcontroller:** Raspberry Pi RP2350 (dual-core Arm Cortex-M33)
- **RAM:** 520 KB
- **Flash:** 4 MB (with UF2 bootloader)
- **I2C Pins:** board.SCL, board.SDA (GPIO3, GPIO4 typical)
- **LED:** board.LED (GPIO25)
- **USB:** Native CDC serial over USB

## Device Enumeration

```
Bus 001 Device XXX: ID 2e8a:10a4 Pimoroni Tiny 2350
```

Device appears as:
- `/dev/ttyACM0` - Serial/REPL port
- `/dev/sdc1` - CIRCUITPY USB mass storage (when mounted)

## Next Steps

1. Connect device via USB
2. Mount filesystem: `sudo mount /dev/sdc1 /mnt/rp2350`
3. Check boot_out.txt: `cat /mnt/rp2350/boot_out.txt`
4. Edit code.py as needed
5. Unmount and reset device to test changes

Happy coding! 🎉
