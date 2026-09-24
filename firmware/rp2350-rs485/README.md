# RP2350 RS485 Communication Project

A PlatformIO project for RS485 serial communication using the Raspberry Pi RP2350 microcontroller (Pico 2).

## Overview

This project demonstrates how to implement RS485 communication on the RP2350 microcontroller. RS485 is a differential serial communication standard that provides:

- Long-distance communication (up to 1200 meters)
- High noise immunity
- Multi-drop capability (up to 32 devices on one bus)
- Half-duplex communication

## Hardware Requirements

### Components

- Raspberry Pi Pico 2 (RP2350) board
- RS485 transceiver module (e.g., MAX485, MAX3485, or SN75176)
- USB cable for programming and debugging
- Optional: Additional RS485 devices for testing

### Pin Connections

| RP2350 Pin | Function | RS485 Module Pin |
|------------|----------|------------------|
| GP0 (UART0 TX) | Transmit | DI (Driver Input) |
| GP1 (UART0 RX) | Receive | RO (Receiver Output) |
| GP2 (RTS) | Direction Control | DE & RE (tied together) |
| 3.3V | Power | VCC |
| GND | Ground | GND |

**Note:** The DE (Driver Enable) and RE (Receiver Enable) pins on the RS485 module should be connected together and controlled by GP2.

### RS485 Bus Wiring

- Connect A to A and B to B across all devices
- Use twisted pair cable for the A/B lines
- Add 120Ω termination resistors at both ends of the bus
- Keep the bus length within specifications (max 1200m)

## Software Configuration

### Pin Definitions in [`src/main.cpp`](src/main.cpp:23)

```cpp
#define RS485_TX_PIN 0    // UART0 TX (GP0)
#define RS485_RX_PIN 1    // UART0 RX (GP1)
#define RS485_DE_PIN 2    // RTS/Driver Enable / Receiver Enable (GP2)
```

### Communication Settings

- **Baud Rate:** 230400 (configurable in [`src/main.cpp`](src/main.cpp:54))
- **Data Format:** 8N1 (8 data bits, no parity, 1 stop bit)
- **Mode:** Half-duplex with automatic direction control

## Building and Uploading

### Prerequisites

1. Install [PlatformIO](https://platformio.org/install)
2. Install PlatformIO IDE extension for VS Code (recommended) or use PlatformIO CLI

### Build Commands

```bash
# Build the project
pio run

# Upload to the board
pio run --target upload

# Open serial monitor
pio device monitor

# Build, upload, and monitor in one command
pio run --target upload && pio device monitor

```

### Using VS Code

1. Open this folder in VS Code
2. PlatformIO should automatically detect the project
3. Use the PlatformIO toolbar or Command Palette:
   - **Build:** PlatformIO: Build
   - **Upload:** PlatformIO: Upload
   - **Monitor:** PlatformIO: Serial Monitor

## Code Structure

### Main Functions

#### [`setupRS485()`](src/main.cpp:79)

Initializes the RS485 hardware, configures UART1, and sets up the direction control pin.

#### [`rs485Transmit()`](src/main.cpp:95)

Transmits data over RS485. Automatically switches to transmit mode, sends data, and returns to receive mode.

#### [`rs485Receive()`](src/main.cpp:115)

Receives and processes incoming RS485 data. Prints received messages to the USB serial monitor.

#### [`setRS485Mode()`](src/main.cpp:128)

Controls the RS485 transceiver direction (transmit or receive mode).

## Usage Example

The default code transmits a message every 2 seconds and continuously listens for incoming messages:

```cpp
// Transmitted message format
"[RP2350-XXXX|yyyyMMddhhmm] Uptime: XXXXX ms"
```

### Customizing the Code

1. **Change transmission interval:**

   ```cpp
   const unsigned long transmitInterval = 2000; // Change to desired milliseconds
   ```

2. **Modify the message:**

   ```cpp
   snprintf(message, sizeof(message), "Your custom message here");
   ```

3. **Change baud rate:**

   ```cpp
   #define RS485_BAUD 115200  // Or any supported baud rate
   ```

## Testing

### Single Device Test

1. Upload the code to your RP2350
2. Open the serial monitor at 115200 baud
3. You should see transmitted messages every 2 seconds
4. Use an RS485 USB adapter to connect to a PC and test bidirectional communication

### Multi-Device Test

1. Upload the code to multiple RP2350 boards
2. Connect all devices to the same RS485 bus
3. Each device will transmit and receive messages from others
4. Monitor one device's USB serial output to verify communication

## Troubleshooting

### No Communication

- Verify wiring connections (A-to-A, B-to-B)
- Check that DE and RE pins are tied together
- Ensure termination resistors are installed
- Verify baud rate matches on all devices
- Check power supply to RS485 transceiver

### Garbled Data

- Check baud rate configuration
- Verify proper grounding
- Ensure twisted pair cable is used
- Add or check termination resistors
- Reduce transmission speed or distance

### Only One-Way Communication

- Verify DE/RE control pin is working
- Check direction control logic
- Ensure RS485 transceiver is powered correctly

## Advanced Features

### Adding Modbus Protocol

To add Modbus support, update [`platformio.ini`](platformio.ini:30):

```ini
lib_deps =
    4-20ma/ModbusMaster@^2.0.1
```

### Error Detection

Consider adding CRC or checksum validation for reliable communication:

```cpp
// Example: Simple checksum
uint8_t calculateChecksum(const char* data, size_t len);
```

## References

- [RS485 Standard](https://en.wikipedia.org/wiki/RS-485)
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [Arduino-Pico Core](https://github.com/earlephilhower/arduino-pico)

## License

This project is provided as-is for educational and commercial use.

## Contributing

Feel free to submit issues and enhancement requests!
