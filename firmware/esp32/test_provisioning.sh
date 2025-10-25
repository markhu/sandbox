#!/bin/bash
# Test script for BLE provisioning
# Usage: ./test_provisioning.sh [ble_name] [port_name]

BLE_NAME="${1:-BAT-PRO-3-2510}"
PORT="${2:-/dev/tty.usbserial-110}"

echo "Connecting to ESP32 on $PORT..."
echo "Sending provisioning command: ble/name $BLE_NAME"
echo ""

# Configure serial port
stty -f "$PORT" 115200 cs8 -cstopb -parenb

# Send the provisioning command
echo "ble/name $BLE_NAME" > "$PORT"

echo "Command sent! The ESP32 should now be advertising as: $BLE_NAME"
echo ""
echo "To verify, you can:"
echo "  - Check Bluetooth settings on your phone/computer"
echo "  - Use 'screen $PORT 115200' to see the ESP32 output"
echo "  - Run: hcitool lescan (on Linux)"
