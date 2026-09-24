#!/usr/bin/env python3
"""
Helper module for RP2350 CircuitPython scripts.
Provides environment detection and helpful error messages for running scripts on macOS vs RP2350.
"""
import sys


def detect_environment():
    """Check if running on RP2350 board with CircuitPython"""
    try:
        import board
        import busio
        import digitalio
        # Just check for basic CircuitPython modules
        # Different RP2350 boards may have different pin names
        return True
    except ImportError:
        return False


def detect_circuitpython(called_by="this script"):
    """
    Check if running on CircuitPython. If not, print helpful message and exit.

    Args:
        called_by: Name of the script for display in error message
    """
    if not detect_environment():
        print(f"Error: {called_by} requires CircuitPython on an RP2350 board.")
        print()
        print("To run on your RP2350:")
        print(f"  1. Copy to CIRCUITPY volume:")
        print(f"     cp {called_by} /Volumes/CIRCUITPY/code.py")
        print()
        print("  Or on Linux:")
        print(f"     cp {called_by} /media/$USER/CIRCUITPY/code.py")
        print()
        print("  2. The script will auto-run when copied to code.py")
        print()
        print("  3. To see serial output:")
        print("     screen /dev/tty.usbmodem* 115200  (macOS)")
        print("     screen /dev/ttyACM0 115200        (Linux)")
        print()
        print("Note: The board should appear as CIRCUITPY volume when plugged in.")
        print("      If not, hold BOOTSEL button while plugging in to enter BOOTSEL mode.")
        sys.exit(1)
