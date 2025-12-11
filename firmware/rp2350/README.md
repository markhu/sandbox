# RP2350 Development Project

A Python-based development toolkit for RP2350 microcontroller boards, including the Raspberry Pi Pico 2 and Adafruit Fruit Jam.

## Project Status

🟢 **Active Development** - Phase 1: Device Detection Complete

## Features

### Current (Phase 1)
- ✅ **USB Device Detection**: Automatically detect and identify connected RP2350 boards
- ✅ **Board Identification**: Recognize Raspberry Pi Pico 2 and Adafruit Fruit Jam boards
- ✅ **Device Information**: Display VID/PID, serial number, and port information
- ✅ **Cross-Platform**: Works on macOS, Linux, and Windows

### Planned
- 🔄 Serial communication and REPL access
- 🔄 Automated code deployment
- 🔄 Multi-board management
- 🔄 Firmware update tools

## Quick Start

### Installation

1. **Clone or navigate to the project directory:**
   ```bash
   cd rp2350
   ```

2. **Install dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

### Usage

#### Detect Connected Boards

Basic detection:
```bash
python scripts/detect_board.py
```

Verbose output (shows all USB devices):
```bash
python scripts/detect_board.py -v
```

List all serial ports:
```bash
python scripts/detect_board.py --list-all
```

#### Example Output

```
🔍 Scanning for RP2350-based development boards...

======================================================================
RP2350 BOARD DETECTION RESULTS
======================================================================

✅ Found 1 RP2350 board(s):

──────────────────────────────────────────────────────────────────────
Device #1
──────────────────────────────────────────────────────────────────────
Board Found: Adafruit Fruit Jam RP2350 - CircuitPython
  Port: /dev/cu.usbmodem214301
  VID:PID: 239A:CAFE
  Manufacturer: Adafruit
  Product: Fruit Jam RP2350
  Serial Number: 22621FD029CF9CE7
  Description: Fruit Jam RP2350

======================================================================
```

## Supported Boards

### Raspberry Pi Pico 2 (RP2350)
- **Vendor ID**: 0x2E8A
- **Product IDs**:
  - 0x000F: Boot Mode (BOOTSEL)
  - 0x1000: Application Mode
  - 0x0005: MicroPython
  - 0x000A: CircuitPython

### Adafruit Fruit Jam (RP2350)
- **Vendor ID**: 0x239A
- **Product IDs**:
  - 0xCAFE: CircuitPython Mode
  - 0x00F1: CircuitPython (alternate)
  - 0x0101: Boot Mode

## Project Structure

```
rp2350/
├── README.md              # This file
├── requirements.txt       # Python dependencies
├── docs/                  # Documentation
│   ├── requirements.md    # Project requirements specification
│   └── progress.md        # Development progress tracking
└── scripts/               # Python scripts
    └── detect_board.py    # USB device detection script
```

## Requirements

### Software
- Python 3.8 or higher
- pyserial >= 3.5
- pyusb >= 1.2.1 (optional, for detailed USB info)

### Hardware
- Raspberry Pi Pico 2 or Adafruit Fruit Jam development board
- USB cable (Type-C for Pico 2)

### Operating System
- macOS 11.0+
- Windows 10+
- Linux (recent kernel with USB support)

## Installation Notes

### macOS
No additional drivers needed. USB devices should be automatically recognized.

### Linux
You may need to add udev rules for USB device access without sudo:

```bash
# Create udev rule (adjust VID/PID as needed)
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="2e8a", MODE="0666"' | sudo tee /etc/udev/rules.d/99-pico.rules
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="239a", MODE="0666"' | sudo tee -a /etc/udev/rules.d/99-pico.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### Windows
Install the appropriate USB drivers for your board:
- Raspberry Pi Pico: Usually works with built-in Windows drivers
- Adafruit boards: May require [Adafruit drivers](https://learn.adafruit.com/welcome-to-circuitpython/installing-circuitpython#windows-7-drivers-3-7)

## Development

### Running Tests

The detection script can be tested with any connected RP2350 board:

```bash
# Make the script executable (macOS/Linux)
chmod +x scripts/detect_board.py

# Run detection
python scripts/detect_board.py
```

### Adding Support for New Boards

To add support for a new RP2350-based board, edit [`scripts/detect_board.py`](scripts/detect_board.py) and add the VID/PID to the `KNOWN_BOARDS` dictionary:

```python
KNOWN_BOARDS = {
    # Add your board here
    (0xVVVV, 0xPPPP): "Your Board Name - Mode",
    ...
}
```

## Documentation

- [**Requirements Specification**](docs/requirements.md) - Detailed project requirements
- [**Progress Tracking**](docs/progress.md) - Development status and roadmap
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [CircuitPython Documentation](https://docs.circuitpython.org/)

## Troubleshooting

### No boards detected

1. **Check USB connection**: Ensure the board is properly connected
2. **Try BOOTSEL mode**: Hold BOOTSEL button while plugging in the board
3. **Check drivers**: Verify USB drivers are installed (Windows)
4. **Check permissions**: On Linux, ensure you have USB device permissions
5. **Verify dependencies**: Run `pip install -r requirements.txt`

### Script errors

If you see import errors:
```bash
pip install --upgrade pyserial pyusb
```

### Permission denied (Linux)

```bash
# Run with sudo (temporary)
sudo python scripts/detect_board.py

# Or add udev rules (permanent, see Installation Notes above)
```

## Contributing

This is a development project. Feel free to:
- Report issues with board detection
- Submit VID/PID values for additional boards
- Suggest new features

## License

This project is provided as-is for educational and development purposes.

## Changelog

### Version 1.0.0 (2025-10-02)
- ✅ Initial project setup
- ✅ USB device detection implementation
- ✅ Support for Raspberry Pi Pico 2
- ✅ Support for Adafruit Fruit Jam
- ✅ Comprehensive documentation
- ✅ Cross-platform compatibility

## Roadmap

### Phase 2: Serial Communication
- Interactive REPL access
- Serial monitor functionality
- Configurable baud rates

### Phase 3: Code Deployment
- File transfer to boards
- Automated deployment scripts
- Configuration management

### Phase 4: Advanced Features
- Multi-board orchestration
- Firmware update automation
- Debug tools integration

---

**Project Created:** 2025-10-02
**Last Updated:** 2025-10-02
**Status:** Active Development
