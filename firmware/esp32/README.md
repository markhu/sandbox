# ESP32 Development Board - Getting Started Guide

## Overview

This guide covers the essential steps for getting started with a new ESP32 development board, including setup, driver installation, and initial configuration.

## Prerequisites

- macOS system
- USB cable (typically USB-A to Micro-USB or USB-C depending on your board)
- Internet connection for driver installation

## Finding the Virtual COM Port on macOS

### Method 1: Using Terminal (ls command)

1. Open Terminal
2. Before connecting the ESP32, list existing serial ports:

   ```bash
   ls /dev/tty.*
   ```

3. Connect your ESP32 board via USB
4. List serial ports again:

   ```bash
   ls /dev/tty.*
   ```

5. The new device that appears is your ESP32. Common patterns include:
   - `/dev/tty.usbserial-XXXXXXXX` (CP210x USB to UART Bridge)
   - `/dev/tty.SLAB_USBtoUART` (Silicon Labs CP210x)
   - `/dev/tty.usbserial-0001` (CH340 USB to UART)
   - `/dev/tty.wchusbserial*` (CH340/CH341)

### Method 2: Using System Information

1. Click the Apple menu → **About This Mac**
2. Click **System Report** or **More Info**
3. In the sidebar, under **Hardware**, select **USB**
4. Connect your ESP32 board
5. Look for entries like:
   - "CP2102 USB to UART Bridge Controller"
   - "CH340"
   - "USB Serial"

### Method 3: Using ioreg Command

```bash
ioreg -p IOUSB -l -w 0 | grep -i "usb serial"
```

This will show detailed information about USB serial devices.

## Driver Installation

### For CP210x Chipset (Most Common)

1. Download drivers from [Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
2. Install the driver package
3. Restart your Mac if prompted
4. Reconnect your ESP32 board

### For CH340/CH341 Chipset

1. Download drivers from the manufacturer or use Homebrew:

   ```bash
   brew tap homebrew/cask-drivers
   brew install --cask wch-ch34x-usb-serial-driver
   ```

2. Restart your Mac
3. Go to **System Preferences → Security & Privacy** and allow the driver
4. Reconnect your ESP32 board

## Entering Bootloader Mode (Flash/Download Mode)

**Note:** On your board, "RESET" and "EN" (Enable) are typically the same button - both reset the ESP32. Some boards label it "EN", others "RESET", but they serve the same function.

To enter bootloader mode for flashing new firmware (like MicroPython), you need to put the ESP32 into download mode:

### Manual Method (Works on all ESP32 boards)

1. **Hold down the BOOT button** (sometimes labeled "FLASH" or "IO0")
2. **While holding BOOT, press and release the RESET/EN button**
3. **Release the BOOT button**
4. The ESP32 is now in bootloader mode and ready to receive new firmware

### During Flashing (Automated Method)

Most flashing tools will automatically handle this, but if they fail:

1. Start the flash command
2. When you see "Connecting..." or similar message:
   - **Hold BOOT button**
   - **Press RESET/EN briefly**
   - **Release BOOT button**

### Confirming Bootloader Mode

When successfully in bootloader mode, you'll see this on the serial monitor:

```tty
rst:0x1 (POWERON_RESET),boot:0x3 (DOWNLOAD_BOOT(UART0/UART1/SDIO_REI_REO_V2))
waiting for download
```

**What this means:**

- `rst:0x1 (POWERON_RESET)` - Reset reason
- `boot:0x3 (DOWNLOAD_BOOT...)` - **Confirms bootloader mode is active**
- `waiting for download` - **Ready to receive firmware**

If you don't see this message, try the button sequence again.

**Other indicators:**

- Some boards have an LED that changes behavior when in bootloader mode
- The board will not run any previously uploaded firmware

## Installing MicroPython

### Step 1: Install esptool Espressif chips ROM Bootloader Utility

- `pip3 install esptool`

### Step 2: Erase Flash (Optional but Recommended)

- `esptool.py --port /dev/tty.usbserial-XXXXXXXX erase_flash`

**Button sequence for erase:**

1. Run the command above
2. When you see "Connecting...", enter bootloader mode:
   - Hold BOOT button
   - Press RESET/EN button (tap it once)
   - Release BOOT button
3. If successful, you'll see the bootloader message:

   ```tty
   rst:0x1 (POWERON_RESET),boot:0x3 (DOWNLOAD_BOOT(UART0/UART1/SDIO_REI_REO_V2))
   waiting for download
   ```

4. The erase process will then begin automatically

### Step 3: Download MicroPython Firmware

- from [MicroPython Downloads](https://micropython.org/download/ESP32_GENERIC/)

### Step 4: Flash MicroPython

```bash
esptool.py --chip esp32 --port /dev/tty.usbserial-XXXXXXXX write_flash -z 0x1000 ESP32_GENERIC-20231005-v1.21.0.bin
```

**Button sequence for flashing:**

1. Run the command above
2. When you see "Connecting...", enter bootloader mode:
   - Hold BOOT button
   - Press RESET/EN button (tap it once)
   - Release BOOT button
3. Verify bootloader mode with the serial message:

   ```tty
   rst:0x1 (POWERON_RESET),boot:0x3 (DOWNLOAD_BOOT(UART0/UART1/SDIO_REI_REO_V2))
   waiting for download
   ```

4. Wait for flashing to complete (typically 10-30 seconds)
5. Press RESET/EN once more to boot into MicroPython

### Step 5: Test MicroPython

```bash
# Connect to the REPL
screen /dev/tty.usbserial-XXXXXXXX 115200
```

You should see the MicroPython prompt:

```python
>>>
```

Test with:

```python
>>> print("Hello from MicroPython!")
>>> import machine
>>> machine.reset()  # Reset the board
```

Exit screen with: `Ctrl+A`, then `K`, then `Y`

### Alternative Tools

**Thonny IDE** (Easiest for beginners):

1. Download from [thonny.org](https://thonny.org/)
2. Install MicroPython via: **Tools → Options → Interpreter → Install or update firmware**
3. Select your port and click "Install"
4. Thonny handles the bootloader mode automatically

**ampy** (File management):

```bash
pip3 install adafruit-ampy

# Upload a file
ampy --port /dev/tty.usbserial-XXXXXXXX put main.py

# List files
ampy --port /dev/tty.usbserial-XXXXXXXX ls

# Run a script
ampy --port /dev/tty.usbserial-XXXXXXXX run test.py
```

## Setting Up the Development Environment

### Option 1: Arduino IDE

1. Install Arduino IDE from [arduino.cc](https://www.arduino.cc/en/software)
2. Add ESP32 board support:
   - Open Arduino IDE
   - Go to **Preferences**
   - Add to Additional Board Manager URLs: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Go to **Tools → Board → Boards Manager**
   - Search for "ESP32" and install

### Option 2: PlatformIO

1. Install VS Code
2. Install PlatformIO extension
3. Create new project with ESP32 board
4. Example `platformio.ini`:

   ```ini
   [env:esp32dev]
   platform = espressif32
   board = esp32dev
   framework = arduino
   monitor_speed = 115200
   ```

### Option 3: ESP-IDF (Official Espressif Framework)

1. Install prerequisites: `brew install cmake ninja dfu-util`

2. Clone ESP-IDF:

   ```bash
   mkdir -p ~/esp
   cd ~/esp
   git clone --recursive https://github.com/espressif/esp-idf.git
   ```

3. Run installation script:

   ```bash
   cd ~/esp/esp-idf
   ./install.sh esp32
   ```

4. Set up environment (add to `.zshrc` or `.bash_profile`):

   ```bash
   alias get_idf='. $HOME/esp/esp-idf/export.sh'
   ```

## Testing Your Setup

### Basic Blink Example (Arduino)

```cpp
#define LED_PIN 2  // Built-in LED on most ESP32 boards

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  Serial.println("ESP32 Ready!");
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
  delay(1000);
}
```

### Uploading Code

1. Select your board: **Tools → Board → ESP32 Dev Module** (or your specific board)
2. Select your port: **Tools → Port → /dev/tty.usbserial-XXXXXXXX**
3. Click **Upload**
4. If upload fails, hold the **BOOT** button while uploading

## Common Issues and Solutions

### Port Not Found

- Ensure drivers are installed correctly
- Try a different USB cable (some are power-only)
- Try a different USB port
- Check System Preferences → Security & Privacy for blocked drivers

### Upload Failed

- Hold the BOOT button during upload
- Lower upload speed: **Tools → Upload Speed → 115200**
- Press EN (reset) button after upload starts

### Permission Denied

```bash
sudo chmod 666 /dev/tty.usbserial-XXXXXXXX
```

### Driver Not Loading (macOS Security)

1. Go to **System Preferences → Security & Privacy**
2. Click **Allow** for the blocked driver
3. Restart your Mac

## Useful Commands

```bash
# List all USB devices
system_profiler SPUSBDataType

# Monitor serial output (using screen)
screen /dev/tty.usbserial-XXXXXXXX 115200

# Exit screen: Ctrl+A, then K, then Y

# Monitor with PlatformIO
pio device monitor

# Monitor with ESP-IDF
idf.py monitor
```

## Board Specifications (Common ESP32)

- CPU: Dual-core Xtensa 32-bit LX6 @ 240MHz
- RAM: 520 KB SRAM
- Flash: 4MB (typical)
- WiFi: 802.11 b/g/n
- Bluetooth: v4.2 BR/EDR and BLE
- GPIO: 34 pins
- ADC: 18 channels, 12-bit
- DAC: 2 channels, 8-bit
- Touch Sensors: 10
- SPI, I2C, I2S, UART interfaces

## Additional Resources

- [ESP32 Official Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [PlatformIO ESP32 Platform](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- [Random Nerd Tutorials](https://randomnerdtutorials.com/projects-esp32/)

## Next Steps

1. Test basic GPIO with an LED
2. Connect to WiFi
3. Explore built-in sensors (Hall effect, temperature)
4. Try BLE or classic Bluetooth
5. Experiment with deep sleep modes
6. Build your first IoT project!
