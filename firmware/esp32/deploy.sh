#!/bin/bash

# ESP32 Deployment Script
# Uploads all .py and .txt files from esp32/ folder to the ESP32 board using mpremote

echo "=================================================="
echo "ESP32 Deployment Script (mpremote)"
echo "=================================================="

# Check if mpremote is installed
if ! command -v mpremote &> /dev/null; then
    echo "ERROR: mpremote is not installed"
    echo "Install with: pip install mpremote"
    exit 1
fi

# Detect serial port
PORT_ARG=""
DETECTED_PORT=""

if [ -n "$1" ]; then
    # User specified a port
    DETECTED_PORT="$1"
    PORT_ARG="connect $1"
    echo "Using specified port: $1"
else
    # Auto-detect using mpremote devs
    echo "Auto-detecting ESP32..."

    # Get device list and extract the first device port
    DEVS_OUTPUT=$(mpremote devs 2>&1)

    if [ $? -eq 0 ] && [ -n "$DEVS_OUTPUT" ]; then
        # Extract port from output (format: /dev/xxx Serial port)
        DETECTED_PORT=$(echo "$DEVS_OUTPUT" | grep -E '/dev/|COM' | head -n 1 | awk '{print $1}')

        if [ -n "$DETECTED_PORT" ]; then
            echo "✓ Found ESP32 at: $DETECTED_PORT"
            PORT_ARG="connect $DETECTED_PORT"
        else
            echo "WARNING: No devices found by 'mpremote devs'"
            echo "Available devices:"
            echo "$DEVS_OUTPUT"
            echo ""
            echo "Attempting to continue with auto-detection..."
            # Leave PORT_ARG empty to let mpremote auto-detect
        fi
    else
        echo "WARNING: Could not run 'mpremote devs'"
        echo "Attempting to continue with mpremote's built-in auto-detection..."
        # Leave PORT_ARG empty to let mpremote auto-detect
    fi
fi

echo ""

# Install required libraries
echo "Installing required libraries..."
echo "  Installing ssd1306 library for OLED display..."
mpremote $PORT_ARG mip install ssd1306 || {
    echo "WARNING: Failed to install ssd1306 library"
    echo "OLED display functionality may not work"
}
echo ""

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "Uploading Python files..."
echo ""

# Upload all .py files
for file in "$SCRIPT_DIR"/*.py; do
    if [ -f "$file" ]; then
        filename=$(basename "$file")
        echo "  Uploading: $filename"
        mpremote $PORT_ARG cp "$file" ":$filename" || {
            echo "ERROR: Failed to upload $filename"
            exit 1
        }
    fi
done

echo ""
echo "Uploading text files..."
echo ""

# Upload all .txt files
for file in "$SCRIPT_DIR"/*.txt; do
    if [ -f "$file" ]; then
        filename=$(basename "$file")
        echo "  Uploading: $filename"
        mpremote $PORT_ARG cp "$file" ":$filename" || {
            echo "ERROR: Failed to upload $filename"
            exit 1
        }
    fi
done

echo ""
echo "=================================================="
echo "✓ Deployment complete!"
echo "=================================================="
echo ""
echo "Uploaded files:"
mpremote $PORT_ARG ls

echo ""
echo "Resetting ESP32..."
mpremote $PORT_ARG reset

echo ""
echo "To see the output, run:"
if [ -n "$DETECTED_PORT" ]; then
    echo "  mpremote connect $DETECTED_PORT repl"
    echo ""
    echo "Or use screen/minicom:"
    echo "  screen $DETECTED_PORT 115200"
else
    echo "  mpremote repl"
    echo ""
    echo "Or use screen/minicom:"
    echo "  screen /dev/tty.usbserial-XXXX 115200"
fi
echo ""
echo "Note: The ESP32 has been reset and will auto-start"
echo "      Check the qas-TIMESTAMP.log file for boot diagnostics"
