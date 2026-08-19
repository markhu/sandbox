#!/usr/bin/env python3
"""
serial_cmd.py - Send one or more commands to the ClickLogger over serial and
print whatever comes back, without needing to hand-write a pyserial snippet
every time.

Usage:
  python3 serial_cmd.py CMD [CMD ...]
  python3 serial_cmd.py --wait 3 CMD [CMD ...]   # longer settle time per command
  python3 serial_cmd.py --port /dev/cu.xxx CMD

Examples:
  python3 serial_cmd.py STATUS
  python3 serial_cmd.py DUMP
  python3 serial_cmd.py "THRESH 4500" HALLON CLEAR
  python3 serial_cmd.py --wait 8 SERVE          # SERVE needs longer to connect
  python3 serial_cmd.py --listen 20             # just listen, no commands sent

NOTE: unlike health_check.py, this does NOT touch esptool/chip_id, so it
never resets the board - safe to use during an active data-collection run.
"""

import argparse
import glob
import sys
import time

import serial


def find_port(serial_suffix: str = "") -> str:
    candidates = sorted(glob.glob("/dev/cu.usbserial-*"))
    if serial_suffix:
        candidates = [c for c in candidates if serial_suffix in c]
    return candidates[0] if candidates else ""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("commands", nargs="*", help="Commands to send, in order (e.g. STATUS, DUMP, 'THRESH 4500')")
    parser.add_argument("--port", help="Serial device path (auto-detected if omitted)")
    parser.add_argument("--serial", default="3552BBE64F", help="FTDI serial suffix to match when auto-detecting (default: this board's)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--wait", type=float, default=1.0, help="Seconds to wait for a response after each command (default 1.0)")
    parser.add_argument("--listen", type=float, default=0, help="Just listen for SECONDS with no commands sent (e.g. to catch async click events)")
    args = parser.parse_args()

    port = args.port or find_port(args.serial)
    if not port:
        print("[FAIL] No usbserial device found.", file=sys.stderr)
        return 1

    ser = serial.Serial(port, baudrate=args.baud, timeout=1)
    time.sleep(0.3)
    ser.reset_input_buffer()

    if args.listen > 0:
        print(f"Listening on {port} for {args.listen:.0f}s...")
        start = time.time()
        buf = ""
        while time.time() - start < args.listen:
            n = ser.in_waiting
            if n:
                buf += ser.read(n).decode(errors="replace")
        print(buf if buf else "(no serial activity)")

    for cmd in args.commands:
        ser.reset_input_buffer()
        ser.write((cmd + "\n").encode())
        time.sleep(args.wait)
        out = ser.read(ser.in_waiting).decode(errors="replace")
        print(f"--- {cmd} ---")
        print(out if out else "(no response)")

    ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
