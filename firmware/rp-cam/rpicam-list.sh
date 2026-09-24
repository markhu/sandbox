#!/bin/bash

echo "=== Raspberry Pi Camera Detection ==="
echo ""

# Check if rpicam-still is available
if ! command -v rpicam-still &> /dev/null; then
    echo "❌ rpicam-still not found. Please install libcamera tools:"
    echo "   sudo apt update && sudo apt install -y libcamera-apps"
    echo ""
    exit 1
fi

echo "✓ rpicam-still is installed"
echo ""

# List available cameras
echo "--- Listing Available Cameras ---"
rpicam-still --list-cameras 2>&1

echo ""
echo "=== Camera Troubleshooting Tips ==="
echo ""
echo "If no cameras are detected, try these steps:"
echo ""
echo "1. Enable the camera interface:"
echo "   sudo raspi-config"
echo "   → Interface Options → Camera → Enable"
echo ""
echo "2. Check if camera cable is properly connected:"
echo "   - Power off the Pi completely"
echo "   - Check that the ribbon cable is fully inserted"
echo "   - Blue side of cable should face the Ethernet port"
echo ""
echo "3. Verify camera is detected by the system:"
echo "   vcgencmd get_camera"
echo "   (should show: supported=1 detected=1)"
echo ""
echo "4. Check for camera in device tree:"
echo "   dmesg | grep -i camera"
echo ""
echo "5. Reboot after enabling camera:"
echo "   sudo reboot"
echo ""
echo "6. For older Raspberry Pi OS, add to /boot/config.txt:"
echo "   camera_auto_detect=1"
echo "   (Then reboot)"
echo ""
echo "=== Common Camera Models ==="
echo ""
echo "• Raspberry Pi Camera Module v1 (OV5647)"
echo "• Raspberry Pi Camera Module v2 (IMX219)"
echo "• Raspberry Pi Camera Module v3 (IMX708)"
echo "• Raspberry Pi HQ Camera (IMX477)"
echo ""
