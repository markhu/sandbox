#!/usr/bin/python3

# serial_get.py
# reads serial port data and optionally allows interactive command input

import sys
import time
import serial
import select
import termios
import tty
import json
from pathlib import Path
import argparse
from serial.tools import list_ports


CACHE_FILE = Path.home() / f".{Path(__file__).stem}_cache.json"


def load_cached_port():
    """Load the last used port and baud rate from cache file"""
    try:
        if CACHE_FILE.exists():
            with open(CACHE_FILE, 'r') as f:
                data = json.load(f)
                # Print previous values if they exist
                if 'previous_port' in data or 'previous_baud' in data:
                    prev_port = data.get('previous_port', 'N/A')
                    prev_baud = data.get('previous_baud', 'N/A')
                    print(f"Previous connection: {prev_port} @ {prev_baud} baud")
                return data.get('last_port'), data.get('last_baud')
    except Exception:
        pass
    return None, None


def save_cached_port(port, baud):
    """Save the port and baud rate to cache file, preserving previous values"""
    try:
        data = {'last_port': port, 'last_baud': baud}

        # Load existing cache to preserve previous values
        if CACHE_FILE.exists():
            with open(CACHE_FILE, 'r') as f:
                old_data = json.load(f)
                # Save previous values for troubleshooting
                if 'last_port' in old_data:
                    data['previous_port'] = old_data['last_port']
                if 'last_baud' in old_data:
                    data['previous_baud'] = old_data['last_baud']

        with open(CACHE_FILE, 'w') as f:
            json.dump(data, f, indent=2)
    except Exception:
        pass


def list_serial_ports():
    """List serial ports that are likely to be attached to dev boards"""
    ports = [p for p in list_ports.comports() if p.device.startswith('/dev/')]

    if not ports:
        print("No serial ports found under /dev/")
        return

    keywords = (
        "usb", "acm", "slab", "wch", "ch34", "esp", "cp210", "ftdi",
        "serial", "modem", "uart", "ttyusb", "ttyacm",
    )

    def is_likely(candidate):
        haystack = f"{candidate.device} {candidate.description} {candidate.hwid}".lower()
        return any(keyword in haystack for keyword in keywords)

    likely = [p for p in ports if is_likely(p)]
    others = [p for p in ports if p not in likely]

    print("Likely microcontroller serial ports:")
    if likely:
        for port in likely:
            desc = port.description or "No description"
            print(f"  {port.device}  |  {desc}")
    else:
        print("  None detected")

    if others:
        print("\nOther serial ports under /dev/:")
        for port in others:
            desc = port.description or "No description"
            print(f"  {port.device}  |  {desc}")


def send_mode(ser, message, duration):
    """Send a message and optionally read response for specified duration"""
    # Send the message
    ser.write((message + '\r\n').encode())
    print(f"Sent: {message}")

    # Read response if duration > 0
    if duration > 0:
        print(f"Reading response for {duration} seconds...")
        end_time = time.time() + duration
        while time.time() < end_time:
            data = ser.read(1024)
            if data:
                sys.stdout.write(data.decode(errors="replace"))
                sys.stdout.flush()


def read_mode(ser, duration):
    """Simple read mode - read for specified duration and exit"""
    end_time = time.time() + duration
    while time.time() < end_time:
        data = ser.read(1024)
        if data:
            sys.stdout.write(data.decode(errors="replace"))
            sys.stdout.flush()
    print(f"\nRead finished after {duration} seconds (timeout reached).")


def interactive_mode(ser):
    """Interactive mode - monitor output and allow sending commands"""
    print("=" * 60)
    print("Interactive Serial Console")
    print("=" * 60)
    print("Commands:")
    print("  Type and press ENTER to send commands")
    print("  Ctrl+C to exit")
    print("  Ctrl+D to exit")
    print("=" * 60)
    print()

    # Save original terminal settings
    old_settings = termios.tcgetattr(sys.stdin)

    try:
        # Set terminal to raw mode for character-by-character input
        tty.setraw(sys.stdin.fileno())

        input_buffer = ""

        while True:
            # Check for data from serial port
            if ser.in_waiting:
                data = ser.read(ser.in_waiting)
                if data:
                    # If we have buffered input, clear the line first
                    if input_buffer:
                        sys.stdout.write('\r' + ' ' * (len(input_buffer) + 2) + '\r')

                    # Write serial data
                    sys.stdout.write(data.decode(errors="replace"))

                    # Redraw input buffer if it exists
                    if input_buffer:
                        sys.stdout.write(f"> {input_buffer}")

                    sys.stdout.flush()

            # Check for keyboard input (non-blocking)
            if select.select([sys.stdin], [], [], 0.01)[0]:
                char = sys.stdin.read(1)

                # Handle Ctrl+C (0x03) or Ctrl+D (0x04)
                if char in ('\x03', '\x04'):
                    print("\n\nExiting...")
                    break

                # Handle Enter/Return
                elif char in ('\r', '\n'):
                    if input_buffer:
                        # Send the command
                        ser.write((input_buffer + '\r\n').encode())
                        sys.stdout.write('\r\n')
                        sys.stdout.flush()
                        input_buffer = ""
                    else:
                        sys.stdout.write('\r\n')
                        sys.stdout.flush()

                # Handle backspace/delete
                elif char in ('\x7f', '\x08'):
                    if input_buffer:
                        input_buffer = input_buffer[:-1]
                        sys.stdout.write('\r> ' + input_buffer + ' \r> ' + input_buffer)
                        sys.stdout.flush()

                # Handle printable characters
                elif char.isprintable():
                    input_buffer += char
                    sys.stdout.write(char)
                    sys.stdout.flush()

            time.sleep(0.01)

    finally:
        # Restore terminal settings
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
        print()


def main():
    if len(sys.argv) > 1 and sys.argv[1].lower() == 'ls':
        list_serial_ports()
        return

    parser = argparse.ArgumentParser(
        description="Read serial port data and optionally send commands",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Read for 5 seconds and exit (default)
  %(prog)s /dev/cu.usbserial-110

  # Read for 10 seconds at 115200 baud
  %(prog)s /dev/cu.usbserial-110 115200 10.0

  # Send a command and read response for 2 seconds
  %(prog)s /dev/cu.usbserial-110 --send "print('Hello')" --duration 2.0
  %(prog)s /dev/cu.usbserial-110 115200 --send "import machine; machine.reset()"

  # Send a command without waiting for response
  %(prog)s /dev/cu.usbserial-110 --send "led.on()" --duration 0

  # Interactive mode - monitor and send commands
  %(prog)s /dev/cu.usbserial-110 --interactive
  %(prog)s /dev/cu.usbserial-110 115200 --interactive
        """
    )

    parser.add_argument('device', nargs='?', help='Serial device path (e.g., /dev/cu.usbserial-110, COM1). If not specified, uses cached port from last run.')
    parser.add_argument('baudrate', nargs='?', type=int, default=None,
                       help='Baud rate for serial communication (default: uses cached value or 230400)')
    parser.add_argument('pos_duration', nargs='?', type=float, default=None,
                       help='Duration in seconds to read data (positional, default: 5.0, ignored in interactive mode)')
    parser.add_argument('-i', '--interactive', action='store_true',
                       help='Enable interactive mode to send commands')
    parser.add_argument('-s', '--send', type=str, metavar='MESSAGE',
                       help='Send a message/command to the serial port and optionally read response')
    parser.add_argument('-d', '--duration', '-t', '--timeout', type=float, dest='duration',
                       help='Duration to read response in seconds (default: 2.0 for --send mode, 5.0 for read mode)')

    args = parser.parse_args()

    cached_port, cached_baudrate = load_cached_port()

    # Handle port caching
    port = args.device
    used_cache = False
    if not port:
        port = cached_port
        if not port:
            print("Error: No device specified and no cached device found.")
            print("Please specify a device path on first use.")
            sys.exit(1)
        print(f"Using cached device: {port}")
        used_cache = True

    # Handle baudrate - use cached if not specified, otherwise use default
    if args.baudrate is not None:
        baudrate = args.baudrate
    elif cached_baudrate is not None:
        baudrate = cached_baudrate
        print(f"Using cached baud rate: {baudrate}")
    else:
        baudrate = 230400
        print(f"Using default baud rate: {baudrate}")

    # Save the connection details for next time (only update previous if not using cache)
    if not used_cache:
        save_cached_port(port, baudrate)
    elif baudrate != cached_baudrate:
        # Baudrate changed but port is from cache, still save
        save_cached_port(port, baudrate)

    # Determine duration: --duration flag takes precedence, then positional, then defaults
    if args.duration is not None:
        duration = args.duration
    elif args.pos_duration is not None:
        duration = args.pos_duration
    else:
        # Default duration depends on mode
        if args.send:
            duration = 2.0
        else:
            duration = 5.0

    print(f"Opening serial port {port} at {baudrate} baud...")

    # Use the same duration for send mode
    send_duration = duration

    try:
        with serial.Serial(port, baudrate, timeout=0.1) as ser:
            if args.interactive:
                interactive_mode(ser)
            elif args.send:
                send_mode(ser, args.send, send_duration)
            else:
                read_mode(ser, duration)
    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        sys.exit(0)


if __name__ == "__main__":
    main()
