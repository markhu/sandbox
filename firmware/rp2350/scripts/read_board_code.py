#!/usr/bin/env python3
"""
Read code from CircuitPython board via serial REPL
"""
import serial
import time
import sys

def read_board_code(port='/dev/cu.usbmodem214301', baudrate=115200):
    """Read the code.py file from the board"""
    try:
        ser = serial.Serial(port, baudrate, timeout=2)
        time.sleep(0.5)

        # Send Ctrl+C several times to interrupt the running program
        print("Stopping running program...")
        for _ in range(5):
            ser.write(b'\x03')
            time.sleep(0.1)

        # Clear buffer
        time.sleep(0.5)
        ser.read(ser.in_waiting or 10000)
        time.sleep(0.3)

        # Now enter commands to read the file
        print("Reading file list...")
        ser.write(b'import os\r\n')
        time.sleep(0.2)
        ser.write(b'print(os.listdir("/"))\r\n')
        time.sleep(0.5)

        output = ser.read(ser.in_waiting or 5000).decode('utf-8', errors='ignore')
        print("Files on board:")
        print(output)

        # Try to read code.py
        print("\nReading code.py...")
        ser.write(b'try:\r\n')
        time.sleep(0.1)
        ser.write(b'    with open("/code.py", "r") as f:\r\n')
        time.sleep(0.1)
        ser.write(b'        print("\\n=== START CODE.PY ===")\r\n')
        time.sleep(0.1)
        ser.write(b'        print(f.read())\r\n')
        time.sleep(0.1)
        ser.write(b'        print("\\n=== END CODE.PY ===")\r\n')
        time.sleep(0.1)
        ser.write(b'except Exception as e:\r\n')
        time.sleep(0.1)
        ser.write(b'    print(f"Error reading code.py: {e}")\r\n')
        time.sleep(0.1)
        ser.write(b'\r\n')  # Exit the try block
        time.sleep(1.5)

        # Read the output
        code_output = ser.read(ser.in_waiting or 20000).decode('utf-8', errors='ignore')
        print(code_output)

        # Also try main.py
        print("\nChecking for main.py...")
        ser.write(b'try:\r\n')
        time.sleep(0.1)
        ser.write(b'    with open("/main.py", "r") as f:\r\n')
        time.sleep(0.1)
        ser.write(b'        print("\\n=== START MAIN.PY ===")\r\n')
        time.sleep(0.1)
        ser.write(b'        print(f.read())\r\n')
        time.sleep(0.1)
        ser.write(b'        print("\\n=== END MAIN.PY ===")\r\n')
        time.sleep(0.1)
        ser.write(b'except Exception as e:\r\n')
        time.sleep(0.1)
        ser.write(b'    print(f"No main.py found")\r\n')
        time.sleep(0.1)
        ser.write(b'\r\n')
        time.sleep(1.5)

        main_output = ser.read(ser.in_waiting or 20000).decode('utf-8', errors='ignore')
        print(main_output)

        ser.close()

    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        return 1

    return 0

if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else '/dev/cu.usbmodem214301'
    sys.exit(read_board_code(port))
