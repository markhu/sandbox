#!/usr/bin/env python3
"""
health_check.py - Verify connectivity to an M5Stack (ESP32) dev board over USB serial.

Checks performed:
  1. Serial port exists and can be opened at the target baud rate.
  2. esptool.py can talk to the ESP32 bootloader (chip_id) - proves the MCU
     itself responds, not just the FTDI USB-serial bridge.
  3. (optional) Live serial monitor to watch boot/log output from running firmware.

Usage:
  ./health_check.py                          # run port + chip checks
  ./health_check.py --monitor                # also open a live serial monitor
  ./health_check.py --port /dev/cu.xxxx       # override auto-detected port
  ./health_check.py --serial 3552BBE64F       # match by FTDI serial number suffix
"""

import argparse
import glob
import subprocess
import sys
import time

DEFAULT_BAUD = 115200
ESPTOOL_CANDIDATES = [
    "esptool.py",
    "esptool",
    "/Users/markhudson/Library/Python/3.9/bin/esptool.py",
]


def find_esptool() -> str:
    for candidate in ESPTOOL_CANDIDATES:
        try:
            result = subprocess.run(
                [candidate, "version"], capture_output=True, text=True, timeout=10
            )
            if result.returncode == 0:
                return candidate
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue
    return ""


def find_port(serial_suffix: str = "") -> str:
    """Find a likely M5Stack/FTDI usbserial device node."""
    candidates = sorted(glob.glob("/dev/cu.usbserial-*"))
    if serial_suffix:
        candidates = [c for c in candidates if serial_suffix in c]
    return candidates[0] if candidates else ""


def check_port_open(port: str, baud: int) -> bool:
    """Try to open and configure the serial port (no data exchange)."""
    try:
        import serial  # pyserial

        with serial.Serial(port, baudrate=baud, timeout=1):
            pass
        return True
    except Exception as exc:
        print(f"  [FAIL] Could not open {port} @ {baud} baud: {exc}")
        return False


def check_chip_id(esptool_bin: str, port: str) -> bool:
    """Ask esptool to talk to the ESP32 bootloader over the given port."""
    try:
        result = subprocess.run(
            [esptool_bin, "--port", port, "chip_id"],
            capture_output=True,
            text=True,
            timeout=30,
        )
        print(result.stdout)
        if result.returncode != 0:
            print(result.stderr)
            return False
        return "MAC:" in result.stdout or "Chip is" in result.stdout
    except FileNotFoundError:
        print(f"  [FAIL] esptool binary not found: {esptool_bin}")
        return False
    except subprocess.TimeoutExpired:
        print("  [FAIL] esptool chip_id timed out")
        return False


def monitor(port: str, baud: int, duration: float = 5.0) -> None:
    """Open a short-lived serial monitor and print anything received."""
    try:
        import serial
    except ImportError:
        print("  [SKIP] pyserial not installed, cannot monitor")
        return

    print(f"\nMonitoring {port} @ {baud} baud for {duration:.0f}s "
          f"(Ctrl+C to stop early)...")
    try:
        with serial.Serial(port, baudrate=baud, timeout=0.5) as ser:
            start = time.time()
            saw_data = False
            while time.time() - start < duration:
                line = ser.readline()
                if line:
                    saw_data = True
                    try:
                        print(" ", line.decode(errors="replace").rstrip())
                    except Exception:
                        print(" ", line)
            if not saw_data:
                print("  (no data received - board may be idle/no running firmware)")
    except KeyboardInterrupt:
        pass
    except Exception as exc:
        print(f"  [FAIL] Monitor error: {exc}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial device path (auto-detected if omitted)")
    parser.add_argument("--serial", default="", help="FTDI serial number (or suffix) to match")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="Baud rate")
    parser.add_argument("--monitor", action="store_true", help="Open a live serial monitor after checks")
    parser.add_argument("--monitor-seconds", type=float, default=5.0, help="Monitor duration in seconds")
    args = parser.parse_args()

    port = args.port or find_port(args.serial)
    if not port:
        print("[FAIL] No usbserial device found. Is the board plugged in?")
        return 1

    print(f"Target port: {port}")
    print(f"Baud rate:   {args.baud}\n")

    print("1. Checking serial port can be opened...")
    port_ok = check_port_open(port, args.baud)
    print("  [OK] Port opened successfully" if port_ok else "")

    print("\n2. Checking ESP32 bootloader responds (esptool chip_id)...")
    esptool_bin = find_esptool()
    if not esptool_bin:
        print("  [FAIL] esptool not found on PATH")
        chip_ok = False
    else:
        chip_ok = check_chip_id(esptool_bin, port)
        print("  [OK] Chip responded" if chip_ok else "  [FAIL] Chip did not respond")

    if args.monitor:
        monitor(port, args.baud, args.monitor_seconds)

    print("\n--- Summary ---")
    print(f"Port open:  {'PASS' if port_ok else 'FAIL'}")
    print(f"Chip reply: {'PASS' if chip_ok else 'FAIL'}")

    return 0 if (port_ok and chip_ok) else 1


if __name__ == "__main__":
    sys.exit(main())
