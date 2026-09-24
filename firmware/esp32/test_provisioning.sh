#!/bin/bash
# Test script for BLE provisioning
# Usage: ./test_provisioning.sh [ble_name] [port_name]
#        ./test_provisioning.sh --help
#        ./test_provisioning.sh --ports

# Handle help and port discovery flags
if [[ "$1" == "--help" ]] || [[ "$1" == "-h" ]]; then
    echo "Usage: ./test_provisioning.sh [ble_name] [port_name]"
    echo ""
    echo "Arguments:"
    echo "  ble_name   - Bluetooth name for the device (default: BAT-PRO-3-2510)"
    echo "  port_name  - Serial port path (default: /dev/tty.usbserial-110)"
    echo ""
    echo "Options:"
    echo "  --help, -h   - Show this help message"
    echo "  --ports, -p  - Show commands to discover serial ports"
    echo ""
    echo "Examples:"
    echo "  ./test_provisioning.sh"
    echo "  ./test_provisioning.sh MY-DEVICE"
    echo "  ./test_provisioning.sh MY-DEVICE /dev/ttyUSB0"
    exit 0
fi

if [[ "$1" == "--ports" ]] || [[ "$1" == "-p" ]]; then
    show_port_discovery_commands() {
        echo ""
        echo "=== Commands to Find Active Serial Ports ==="
        echo ""
        if [[ "$OSTYPE" == "darwin"* ]]; then
            echo "  macOS:"
            echo "    ls /dev/tty.* | grep -i usb"
            echo "    system_profiler SPUSBDataType | grep -A 10 -i 'serial'"
            echo "    ioreg -p IOUSB -l -w 0 | grep -i serial"
        elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
            echo "  Linux:"
            echo "    ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null"
            echo "    dmesg | grep tty | tail -20"
            echo "    lsusb"
            echo "    udevadm info --query=all --name=/dev/ttyUSB0"
        else
            echo "  Windows (Git Bash/WSL):"
            echo "    ls /dev/ttyS* 2>/dev/null"
            echo "    mode"
        fi
        echo ""
        echo "  Common ESP32 port patterns:"
        echo "    /dev/tty.usbserial-*     (macOS - CH340/CP2102)"
        echo "    /dev/tty.SLAB_USBtoUART* (macOS - Silicon Labs)"
        echo "    /dev/ttyUSB*             (Linux - CH340/CP2102)"
        echo "    /dev/ttyACM*             (Linux - Native USB)"
        echo ""
    }
    show_port_discovery_commands
    exit 0
fi

BLE_NAME="${1:-BAT-PRO-3-2510}"
PORT="${2:-/dev/tty.usbserial-110}"

# Function to display commands for finding serial ports
show_port_discovery_commands() {
    echo ""
    echo "=== Commands to Find Active Serial Ports ==="
    echo ""
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "  macOS:"
        echo "    ls /dev/tty.* | grep -i usb"
        echo "    system_profiler SPUSBDataType | grep -A 10 -i 'serial'"
        echo "    ioreg -p IOUSB -l -w 0 | grep -i serial"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "  Linux:"
        echo "    ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null"
        echo "    dmesg | grep tty | tail -20"
        echo "    lsusb"
        echo "    udevadm info --query=all --name=/dev/ttyUSB0"
    else
        echo "  Windows (Git Bash/WSL):"
        echo "    ls /dev/ttyS* 2>/dev/null"
        echo "    mode"
    fi
    echo ""
    echo "  Common ESP32 port patterns:"
    echo "    /dev/tty.usbserial-*     (macOS - CH340/CP2102)"
    echo "    /dev/tty.SLAB_USBtoUART* (macOS - Silicon Labs)"
    echo "    /dev/ttyUSB*             (Linux - CH340/CP2102)"
    echo "    /dev/ttyACM*             (Linux - Native USB)"
    echo ""
}

# Check if the port exists
if [ ! -e "$PORT" ]; then
    echo "ERROR: Port $PORT does not exist!"
    echo ""
    echo "The serial port may have changed. Please check available ports."
    show_port_discovery_commands
    exit 1
fi

# Check if we have permission to access the port
if [ ! -r "$PORT" ] || [ ! -w "$PORT" ]; then
    echo "ERROR: No read/write permission for $PORT"
    echo "Try: sudo chmod 666 $PORT"
    echo "Or add your user to the dialout group (Linux): sudo usermod -a -G dialout $USER"
    exit 1
fi

echo "Connecting to ESP32 on $PORT..."
echo "Sending provisioning command: ble/name $BLE_NAME"
echo ""

# Configure serial port
if ! stty -f "$PORT" 115200 cs8 -cstopb -parenb 2>/dev/null; then
    echo "ERROR: Failed to configure serial port $PORT"
    echo "The device may have been disconnected or the port may have changed."
    show_port_discovery_commands
    exit 1
fi

# Send the provisioning command
if ! echo "ble/name $BLE_NAME" > "$PORT" 2>/dev/null; then
    echo "ERROR: Failed to send command to $PORT"
    echo "The device may have been disconnected or the port may have changed."
    show_port_discovery_commands
    exit 1
fi

echo "Command sent! The ESP32 should now be advertising as: $BLE_NAME"
echo ""
echo "To verify, you can:"
echo "  - Check Bluetooth settings on your phone/computer"
echo "  - Use 'screen $PORT 115200' to see the ESP32 output"
echo "  - Run: hcitool lescan (on Linux)"
echo ""
echo "If the device is not responding, the serial port may have changed."
echo "Run './test_provisioning.sh --help' to see port discovery commands."
