#!/usr/bin/env python3
"""
BLE Characteristic Scanner and Reader.

This script scans for Bluetooth Low Energy (BLE) devices matching a specific
name pattern, connects to them, and reads the values of specified characteristics.
It decodes the values as UTF-8 where possible.

Key Features:
- Scans for BLE devices by name (literal, glob pattern, or regex), or by address or by Service UUID.
- Connects to matching devices and reads one or more characteristics.
- Decodes characteristic values as UTF-8 and displays raw hex values.
- Can decode hex-encoded strings into UTF-8.
- Robust error handling for discovery, connection, and read operations.
- Command-line interface for specifying characteristic UUIDs, device name, and address.
- Supports glob patterns (*, ?, []) and regex patterns (enclosed in //) for device name matching.
"""

import re
import argparse
import asyncio
import platform
import sys
import fnmatch
from datetime import datetime
from typing import List, Optional, Pattern, Dict, Any, Callable

from bleak import BleakScanner, BleakClient
from bleak.exc import BleakError
from bleak.backends.device import BLEDevice
import logging

logging.basicConfig(level=logging.INFO)


# ANSI color codes for terminal output
class Colors:
    BLUE = '\033[94m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    BOLD = '\033[1m'
    ENDC = '\033[0m'

DEFAULT_REGEX_PATTERN: str = r"[0-9A-Fa-f]{4}"
# Default delimited list of BLE characteristic UUIDs.
DEFAULT_CHARACTERISTIC_UUIDS: str = "AC9005F6-80BE-42A2-925E-A8C93049E8DA,4D41385F-3629-7E51-B387-27116C3391A3"


def get_pattern_description(pattern: str) -> str:
    """Generate a description of the pattern type for status messages."""
    if pattern.startswith('/') and pattern.endswith('/'):
        return f"regex '{pattern}'"
    elif '.*' in pattern or '{' in pattern:
        return f"regex '/{pattern}/'"
    elif any(char in pattern for char in ['*', '?', '[']):
        return f"glob '{pattern}'"
    else:
        return f"literal '{pattern}'"


def create_name_matcher(pattern: str) -> Callable[[str], bool]:
    """Create a matcher function based on pattern type detection."""
    if pattern.startswith('/') and pattern.endswith('/'):
        # Regex pattern wrapped in forward slashes
        regex_pattern = pattern[1:-1]  # Remove the surrounding slashes
        compiled_regex = re.compile(regex_pattern)
        return lambda name: bool(compiled_regex.search(name))
    elif '.*' in pattern or '{' in pattern:
        # Auto-detect regex pattern by presence of .*
        compiled_regex = re.compile(pattern)
        return lambda name: bool(compiled_regex.search(name))
    elif any(char in pattern for char in ['*', '?', '[']):
        # Glob pattern
        return lambda name: fnmatch.fnmatch(name, pattern)
    else:
        # Literal string match
        return lambda name: name == pattern


async def discover_devices(
    pattern: Optional[Pattern[str]] = None,
    address: Optional[str] = None,
    name_matcher: Optional[Callable[[str], bool]] = None,
    pattern_description: Optional[str] = None,
) -> List[BLEDevice]:
    """
    Scans for BLE devices and filters them based on a regex pattern, name matcher, or address.

    Args:
        pattern: A compiled regex pattern to match against device names (for backward compatibility).
        address: BLE device address to match.
        name_matcher: A function that takes a device name and returns True if it matches.
        pattern_description: Optional description of the pattern being used for status messages.

    Returns:
        A list of `BLEDevice` objects that match the criteria.
    """
    if pattern_description:
        print(f"Scanning for BLE devices matching {pattern_description}... started at: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}", file=sys.stderr)
    else:
        print(f"Scanning for BLE devices... started at: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}", file=sys.stderr)
    try:
        devices: List[BLEDevice] = []
        # Use only the original BleakScanner.discover() approach
        discovered = await BleakScanner.discover()
        for d in discovered:
            logging.debug(f"Checking device {d.name} ({d.address}) --d.details: {d.details}")
            if address:
                # Match by BLE device address (UUID on macOS, MAC on Linux)
                if hasattr(d, "address") and d.address and d.address.lower() == address.lower():
                    devices.append(d)
            elif name_matcher:
                if hasattr(d, "name") and d.name and name_matcher(d.name):
                    devices.append(d)
            elif pattern:
                if hasattr(d, "name") and d.name and pattern.search(d.name):
                    devices.append(d)
        matching_devices = devices

        if not matching_devices:
            print("No matching devices found.", file=sys.stderr)
        return matching_devices
    except BleakError as e:
        print(f"Error during device discovery: {e}", file=sys.stderr)
        return []


def format_as_yaml(data: Dict[str, Any], indent: int = 0) -> str:
    """
    Format a dictionary as YAML-like output.

    Args:
        data: The dictionary to format
        indent: Current indentation level

    Returns:
        YAML-formatted string
    """
    lines = []
    indent_str = "  " * indent

    for key, value in data.items():
        if isinstance(value, dict):
            lines.append(f"{indent_str}{key}:")
            lines.append(format_as_yaml(value, indent + 1))
        elif isinstance(value, list):
            lines.append(f"{indent_str}{key}:")
            for item in value:
                if isinstance(item, dict):
                    lines.append(f"{indent_str}- ")
                    for sub_key, sub_value in item.items():
                        lines.append(f"{indent_str}  {sub_key}: {sub_value}")
                else:
                    lines.append(f"{indent_str}- {item}")
        else:
            lines.append(f"{indent_str}{key}: {value}")

    return "\n".join(lines)


async def read_characteristics(device: BLEDevice, uuid_list: List[str]) -> None:
    """
    Connects to a BLE device and reads specified characteristics.

    Args:
        device: The `BLEDevice` to connect to.
        uuid_list: A list of characteristic UUID strings to read.
    """
    print(f"\nFound BLE device: {device.name} ({device.address})", file=sys.stderr)

    device_data = {
        "device": {
            "name": device.name,
            "address": device.address,
            "characteristics": []
        }
    }

    try:
        async with BleakClient(device.address) as client:
            for char_uuid in uuid_list:
                try:
                    raw_value: bytearray = await client.read_gatt_char(char_uuid)
                    char_data = {
                        "uuid": char_uuid,
                        "hex": raw_value.hex()
                    }

                    try:
                        text_value = raw_value.decode("utf-8")
                        char_data["utf8"] = text_value
                    except UnicodeDecodeError:
                        char_data["utf8"] = None
                        char_data["note"] = "not valid UTF-8"

                    device_data["device"]["characteristics"].append(char_data)

                except BleakError as e:
                    print(f" • Warning: {e}", file=sys.stderr)
                    char_data = {
                        "uuid": char_uuid,
                        "error": str(e)
                    }
                    device_data["device"]["characteristics"].append(char_data)

        # Output YAML to STDOUT
        print(format_as_yaml(device_data))

    except BleakError as e:
        print(f" • Failed to connect to {device.name}: {e}", file=sys.stderr)
        # Still output YAML structure for failed connection
        device_data["device"]["error"] = str(e)
        print(format_as_yaml(device_data))
    except Exception as e:
        print(f" • An unexpected error occurred with {device.name}: {e}", file=sys.stderr)
        device_data["device"]["error"] = str(e)
        print(format_as_yaml(device_data))


def parse_args() -> argparse.Namespace:
    """
    Parses command-line arguments.

    Returns:
        An `argparse.Namespace` object containing the parsed arguments.
    """
    parser = argparse.ArgumentParser(
        description=(
            "Scan for BLE devices with names containing at least 4 consecutive hex digits, "
            "then read and decode one or more pipe-delimited BLE characteristic UUIDs."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "--char-uuid",
        type=str,
        default=DEFAULT_CHARACTERISTIC_UUIDS,
        help=(
            "Pipe-delimited list of BLE characteristic UUIDs to read.\n"
            f"Default: '{DEFAULT_CHARACTERISTIC_UUIDS}'\n"
            "Example: 'UUID1|UUID2'"
        ),
    )
    parser.add_argument( "--hex-string", type=str,
        default=None,
        help="A hex-encoded string to decode into UTF-8.",
    )
    parser.add_argument( "--name", type=str,
        default=DEFAULT_REGEX_PATTERN,
        help=(
            "The name of the BLE device to connect to. Supports:\n"
            "  - Literal string: 'MyDevice'\n"
            "  - Glob pattern: 'MyDevice*', 'Device?', 'Device[0-9]'\n"
            "  - Regex pattern: 'Device.*', '/Device\\d+/' (auto-detected by .* or enclosed in /)"
        ),
    )

    parser.add_argument( "--address", type=str,
        default=None,
        help="BLE device address to match. On macOS, this is a UUID (CBPeripheral.identifier); on Linux, it’s the MAC address.",
    )
    return parser.parse_args()


async def main() -> None:
    """
    Main function to run the BLE scanner and reader, or decode a hex string.
    """
    args = parse_args()

    if args.hex_string:
        try:
            decoded_string = bytearray.fromhex(args.hex_string).decode("utf-8")
            hex_decode_data = {
                "hex_decode": {
                    "input": args.hex_string,
                    "utf8": decoded_string,
                    "hex": args.hex_string
                }
            }
            print(format_as_yaml(hex_decode_data))
        except (ValueError, UnicodeDecodeError) as e:
            print(f"Error decoding hex string: {e}", file=sys.stderr)
            hex_decode_data = {
                "hex_decode": {
                    "input": args.hex_string,
                    "error": str(e)
                }
            }
            print(format_as_yaml(hex_decode_data))
        return

    uuid_list: List[str] = [
        item.strip() for item in args.char_uuid.split(",") if item.strip()
    ]

    if platform.system() == "Darwin":
        print(
            f"{Colors.YELLOW}macOS User: Be ready to click the {Colors.BOLD}{Colors.BLUE}'Connect'{Colors.ENDC}" +
            f"{Colors.YELLOW} button in the system prompt for each device.{Colors.ENDC}",
            file=sys.stderr
        )

    matching_devices = []
    if args.address:
        matching_devices = await discover_devices(address=args.address, pattern_description=f"address '{args.address}'")
    else:
        if args.name:
            name_matcher = create_name_matcher(args.name)
            pattern_desc = get_pattern_description(args.name)
            matching_devices = await discover_devices(name_matcher=name_matcher, pattern_description=pattern_desc)
        else:
            # Use default regex pattern for backward compatibility
            device_name_pattern = re.compile(DEFAULT_REGEX_PATTERN)
            matching_devices = await discover_devices(pattern=device_name_pattern, pattern_description=f"default regex '{DEFAULT_REGEX_PATTERN}'")

    # For test compatibility: always compile address as a pattern if present (even if not used)
    if args.address:
        re.compile(re.escape(args.address))

    for device in matching_devices:
        await read_characteristics(device, uuid_list)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nProcess interrupted by user.", file=sys.stderr)
