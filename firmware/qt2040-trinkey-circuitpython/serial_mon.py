#!/usr/bin/env python3
import argparse
import re
import serial
import sys
import time
import threading
import select

DEFAULT_TIMEOUT = 30.0  # seconds

def parse_timeout(timeout_str):
    """Parse timeout string like '5s', '10', '2.5s' and return float in seconds."""
    if timeout_str is None:
        return DEFAULT_TIMEOUT  # Default timeout

    if timeout_str == '0':
        return float('inf')  # Infinite timeout if '0' specified

    # Match patterns like "5s", "10", "2.5s"
    match = re.match(r'^(\d+(?:\.\d+)?)([s]?)$', timeout_str.lower())
    if not match:
        raise ValueError(f"Invalid timeout format: {timeout_str}. Use format like '5s' or '10'")

    value = float(match.group(1))
    unit = match.group(2)

    # If no unit specified, assume seconds
    if unit == 's' or unit == '':
        return value

    return value

def monitor_serial(ser, stop_event):
    """Thread function to monitor incoming serial data."""
    while not stop_event.is_set():
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8').rstrip()
                if line:
                    # print(f"RX: {line}")
                    print(f"{line}")
        except UnicodeDecodeError:
            # Handle any encoding issues
            pass
        except Exception as e:
            if not stop_event.is_set():
                print(f"Error reading serial: {e}")
            break
        time.sleep(0.01)  # Small delay to prevent excessive CPU usage

def handle_user_input(ser, stop_event, interactive_mode):
    """Thread function to handle user input for sending data."""
    if not interactive_mode:
        return

    print("Interactive mode enabled. Type messages to send, or CTRL-C to exit.")
    print("-" * 50)

    while not stop_event.is_set():
        try:
            # Check if input is available (non-blocking)
            if sys.stdin in select.select([sys.stdin], [], [], 0.1)[0]:
                user_input = input().strip()

                # if user_input.lower() in ['quit', 'exit']:
                #     print("Exiting...")
                #     stop_event.set()
                #     break

                if user_input:
                    # Send the input with a newline
                    ser.write((user_input + '\n').encode('utf-8'))
                    print(f"TX: {user_input}")

        except KeyboardInterrupt:
            print("\nExiting...")
            stop_event.set()
            break
        except EOFError:
            # Handle Ctrl+D
            print("\nExiting...")
            stop_event.set()
            break
        except Exception as e:
            if not stop_event.is_set():
                print(f"Error handling input: {e}")

def main():
    parser = argparse.ArgumentParser(description='Serial monitor for CircuitPython devices with optional interactive input')
    parser.add_argument('--timeout', '-t', type=str, default=str(DEFAULT_TIMEOUT),
                        help='Duration to monitor serial port before closing (e.g., 5s, 10, 2.5s). Default: 30s, 0=infinite')
    parser.add_argument('--port', '-p', type=str, default='/dev/cu.usbmodem1301',
                        help='Serial port. Default: /dev/cu.usbmodem1301')
    parser.add_argument('--baudrate', '-b', type=int, default=115200,
                        help='Baud rate. Default: 115200')
    parser.add_argument('--monitor-only', '-m', action='store_true',
                        help='Monitor only mode (disable interactive input) - useful for scripts')

    args = parser.parse_args()

    try:
        monitor_timeout = parse_timeout(args.timeout)
    except ValueError as e:
        print(f"Error: {e}")
        return 1

    port = args.port
    baudrate = args.baudrate
    interactive_mode = not args.monitor_only

    try:
        # Open serial connection with a short read timeout
        ser = serial.Serial(port, baudrate, timeout=0.1)
        print(f"Connected to {port} at {baudrate} baud")

        if interactive_mode:
            print("Interactive mode: You can send data to the device")
        else:
            print("Monitor-only mode: Receiving data only")

        print(f"Monitoring for {monitor_timeout}s (Press Ctrl+C to exit early)" if monitor_timeout != float('inf')
              else "Monitoring indefinitely (Press Ctrl+C to exit)")
        print("-" * 50)

        # Create stop event for coordinating threads
        stop_event = threading.Event()

        # Start monitoring thread
        monitor_thread = threading.Thread(target=monitor_serial, args=(ser, stop_event))
        monitor_thread.daemon = True
        monitor_thread.start()

        # Start input handling thread if in interactive mode
        input_thread = None
        if interactive_mode:
            input_thread = threading.Thread(target=handle_user_input, args=(ser, stop_event, interactive_mode))
            input_thread.daemon = True
            input_thread.start()

        start_time = time.time()

        try:
            while not stop_event.is_set():
                # Check if monitor timeout has been reached
                elapsed_time = time.time() - start_time
                if elapsed_time >= monitor_timeout:
                    print(f"\nMonitoring timeout ({monitor_timeout}s) reached. Closing connection.")
                    break

                time.sleep(0.1)

        except KeyboardInterrupt:
            print("\nExiting...")

        # Signal threads to stop
        stop_event.set()

        # Wait for threads to finish (with timeout)
        if monitor_thread.is_alive():
            monitor_thread.join(timeout=1.0)
        if input_thread and input_thread.is_alive():
            input_thread.join(timeout=1.0)

    except serial.SerialException as e:
        print(f"Error opening serial port {port}: {e}")
        print("Make sure your RP2040 is connected and the port is correct.")
        return 1

    except Exception as e:
        print(f"Unexpected error: {e}")
        return 1

    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Serial connection closed.")

    return 0

if __name__ == "__main__":
    sys.exit(main())
