#!/usr/bin/env python3
"""
RP2350 Board Detection Script

This script detects USB-connected RP2350-based development boards including:
- Raspberry Pi Pico 2
- Adafruit Fruit Jam

Usage:
    python detect_board.py [options]

Options:
    -v, --verbose    Show detailed device information
    -l, --list-all   List all USB devices (not just RP2350 boards)
    -h, --help       Show this help message

Requirements:
    - pyserial >= 3.5
    - pyusb >= 1.2.1 (optional, for detailed USB info)
"""

import sys
import argparse
from typing import List, Dict, Optional, Tuple

try:
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False
    print("Warning: pyserial not installed. Install with: pip install pyserial")

try:
    import usb.core
    import usb.util
    USB_AVAILABLE = True
except ImportError:
    USB_AVAILABLE = False
    # This is optional, we can still use serial detection


# Known RP2350 board configurations
KNOWN_BOARDS = {
    # Raspberry Pi devices
    (0x2E8A, 0x0003): "Raspberry Pi Pico (RP2040) - Boot Mode",
    (0x2E8A, 0x0005): "Raspberry Pi Pico (RP2040) - MicroPython",
    (0x2E8A, 0x000A): "Raspberry Pi Pico (RP2040) - CircuitPython",
    (0x2E8A, 0x000F): "Raspberry Pi Pico 2 (RP2350) - Boot Mode",
    (0x2E8A, 0x1000): "Raspberry Pi Pico 2 (RP2350) - Application Mode",

    # Adafruit devices
    (0x239A, 0x00F1): "Adafruit Fruit Jam - CircuitPython",
    (0x239A, 0x0101): "Adafruit Fruit Jam - Boot Mode",
    (0x239A, 0x80F1): "Adafruit Board - CircuitPython (Generic)",
    (0x239A, 0xCAFE): "Adafruit Fruit Jam RP2350 - CircuitPython",
}

# Manufacturer names for filtering
RP2350_MANUFACTURERS = [
    "Raspberry Pi",
    "Adafruit",
    "MicroPython",
    "CircuitPython",
]


class BoardInfo:
    """Container for board detection information"""

    def __init__(self, port: str, vid: int, pid: int,
                 serial_number: Optional[str] = None,
                 manufacturer: Optional[str] = None,
                 product: Optional[str] = None,
                 description: Optional[str] = None):
        self.port = port
        self.vid = vid
        self.pid = pid
        self.serial_number = serial_number
        self.manufacturer = manufacturer
        self.product = product
        self.description = description

    @property
    def board_type(self) -> str:
        """Get the board type from known VID/PID combinations"""
        return KNOWN_BOARDS.get((self.vid, self.pid), "Unknown RP2350 Board")

    @property
    def is_rp2350(self) -> bool:
        """Check if this is likely an RP2350-based board"""
        # Check by VID/PID
        if (self.vid, self.pid) in KNOWN_BOARDS:
            return True

        # Check by manufacturer name
        if self.manufacturer:
            for mfr in RP2350_MANUFACTURERS:
                if mfr.lower() in self.manufacturer.lower():
                    return True

        # Check by product description
        if self.product:
            rp_keywords = ["pico", "rp2040", "rp2350", "fruit jam"]
            for keyword in rp_keywords:
                if keyword.lower() in self.product.lower():
                    return True

        return False

    def __str__(self) -> str:
        """String representation of board info"""
        lines = [
            f"Board Found: {self.board_type}",
            f"  Port: {self.port}",
            f"  VID:PID: {self.vid:04X}:{self.pid:04X}",
        ]

        if self.manufacturer:
            lines.append(f"  Manufacturer: {self.manufacturer}")
        if self.product:
            lines.append(f"  Product: {self.product}")
        if self.serial_number:
            lines.append(f"  Serial Number: {self.serial_number}")
        if self.description:
            lines.append(f"  Description: {self.description}")

        return "\n".join(lines)


def detect_boards_serial() -> List[BoardInfo]:
    """
    Detect RP2350 boards using serial port enumeration.

    Returns:
        List of BoardInfo objects for detected boards
    """
    if not SERIAL_AVAILABLE:
        return []

    boards = []
    ports = serial.tools.list_ports.comports()

    for port in ports:
        # Create BoardInfo from port information
        board = BoardInfo(
            port=port.device,
            vid=port.vid or 0,
            pid=port.pid or 0,
            serial_number=port.serial_number,
            manufacturer=port.manufacturer,
            product=port.product,
            description=port.description
        )

        boards.append(board)

    return boards


def detect_boards_usb() -> List[Dict]:
    """
    Detect USB devices using pyusb for more detailed information.

    Returns:
        List of device dictionaries with USB information
    """
    if not USB_AVAILABLE:
        return []

    devices = []
    usb_devices = usb.core.find(find_all=True)

    for dev in usb_devices:
        try:
            device_info = {
                'vid': dev.idVendor,
                'pid': dev.idProduct,
                'manufacturer': usb.util.get_string(dev, dev.iManufacturer) if dev.iManufacturer else None,
                'product': usb.util.get_string(dev, dev.iProduct) if dev.iProduct else None,
                'serial': usb.util.get_string(dev, dev.iSerialNumber) if dev.iSerialNumber else None,
                'bus': dev.bus,
                'address': dev.address,
            }
            devices.append(device_info)
        except (ValueError, usb.core.USBError):
            # Skip devices we can't read
            continue

    return devices


def print_board_summary(boards: List[BoardInfo], verbose: bool = False):
    """
    Print a summary of detected boards.

    Args:
        boards: List of detected BoardInfo objects
        verbose: If True, show detailed information
    """
    rp2350_boards = [b for b in boards if b.is_rp2350]

    print("\n" + "="*70)
    print("RP2350 BOARD DETECTION RESULTS")
    print("="*70)

    if not rp2350_boards:
        print("\n❌ No RP2350-based boards detected.")
        print("\nTroubleshooting:")
        print("  1. Ensure the board is connected via USB")
        print("  2. Check that USB drivers are installed")
        print("  3. Try pressing the BOOTSEL button while plugging in")
        print("  4. On Linux, you may need udev rules or sudo access")
    else:
        print(f"\n✅ Found {len(rp2350_boards)} RP2350 board(s):\n")

        for i, board in enumerate(rp2350_boards, 1):
            print(f"\n{'─'*70}")
            print(f"Device #{i}")
            print('─'*70)
            print(board)

    if verbose and boards:
        other_boards = [b for b in boards if not b.is_rp2350]
        if other_boards:
            print(f"\n\n{'='*70}")
            print(f"OTHER USB DEVICES ({len(other_boards)} found)")
            print('='*70)

            for i, board in enumerate(other_boards, 1):
                print(f"\n{'─'*70}")
                print(f"Device #{i}")
                print('─'*70)
                print(board)

    print("\n" + "="*70 + "\n")


def main():
    """Main entry point for the script"""
    parser = argparse.ArgumentParser(
        description="Detect USB-connected RP2350 development boards",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python detect_board.py              # Basic detection
  python detect_board.py -v           # Verbose output with all USB devices
  python detect_board.py --list-all   # List all serial ports
        """
    )

    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Show detailed information including non-RP2350 devices')
    parser.add_argument('-l', '--list-all', action='store_true',
                       help='List all serial ports, not just RP2350 boards')

    args = parser.parse_args()

    # Check if required libraries are available
    if not SERIAL_AVAILABLE:
        print("\n❌ Error: pyserial is required but not installed.")
        print("\nInstall it with:")
        print("  pip install pyserial")
        return 1

    print("\n🔍 Scanning for RP2350-based development boards...")

    # Detect boards using serial enumeration
    all_boards = detect_boards_serial()

    if args.list_all:
        print_board_summary(all_boards, verbose=True)
    else:
        print_board_summary(all_boards, verbose=args.verbose)

    # Return appropriate exit code
    rp2350_boards = [b for b in all_boards if b.is_rp2350]
    return 0 if rp2350_boards else 1


if __name__ == "__main__":
    sys.exit(main())
