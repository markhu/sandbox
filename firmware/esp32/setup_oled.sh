#!/bin/bash

# ESP32 OLED Display Setup Script
# This script installs the required SSD1306 library and uploads the OLED display script

set -e

echo "=========================================="
echo "ESP32 OLED Display Setup"
echo "=========================================="
echo ""

# Check if mpremote is available
if ! command -v mpremote &> /dev/null; then
    echo "Error: mpremote is not installed"
    echo "Install it with: pip install mpremote"
    exit 1
fi

echo "Step 1: Installing SSD1306 library..."
mpremote mip install ssd1306
echo "✓ SSD1306 library installed"
echo ""

echo "Step 2: Uploading OLED display script..."
mpremote fs cp esp32/oled_display.py :oled_display.py
echo "✓ OLED display script uploaded"
echo ""

echo "Step 3: Verifying installation..."
mpremote fs ls | grep -E "(ssd1306|oled_display)" && echo "✓ Files verified" || echo "⚠ Warning: Could not verify files"
echo ""

echo "=========================================="
echo "Setup Complete!"
echo "=========================================="
echo ""
echo "To run the OLED display script:"
echo "  mpremote run esp32/oled_display.py"
echo ""
echo "Or in the REPL:"
echo "  import oled_display"
echo ""
echo "See OLED_DISPLAY_README.md for more information."
echo ""
