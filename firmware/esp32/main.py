"""
ESP32 Main Boot Script

This script runs automatically when the ESP32 boots and starts:
1. BLE Provisioning service - listens for serial commands to set BLE name
2. OLED Display service - monitors and displays the BLE name on OLED

Both services run in parallel using MicroPython's _thread module.
"""

import sys
import time
import _thread

# Flag to control graceful shutdown
running = True

# Logging setup
LOG_FILE = None

def get_timestamp():
    """Generate timestamp string for logging"""
    try:
        t = time.localtime()
        return "{:04d}{:02d}{:02d}-{:02d}{:02d}{:02d}".format(
            t[0], t[1], t[2], t[3], t[4], t[5]
        )
    except:
        return str(time.ticks_ms())

def setup_logging():
    """Setup logging to file with timestamp"""
    global LOG_FILE
    timestamp = get_timestamp()
    log_filename = f"qas-{timestamp}.log"
    try:
        LOG_FILE = open(log_filename, 'w')
        log(f"=== ESP32 Boot Log - {timestamp} ===")
        log(f"Log file: {log_filename}")
        return log_filename
    except Exception as e:
        print(f"[ERROR] Could not create log file: {e}")
        return None

def log(message):
    """Log message to both console and file"""
    print(message)
    if LOG_FILE:
        try:
            LOG_FILE.write(message + '\n')
            LOG_FILE.flush()
        except:
            pass

def close_log():
    """Close log file"""
    global LOG_FILE
    if LOG_FILE:
        try:
            log("=== End of log ===")
            LOG_FILE.close()
        except:
            pass

def run_ble_provisioning():
    """Run BLE provisioning in a separate thread"""
    try:
        log("[MAIN] Starting BLE provisioning service...")
        import ble_provisioning
        # Call the main() function to start BLE advertising
        ble_provisioning.main()

    except Exception as e:
        log(f"[ERROR] BLE provisioning failed: {e}")
        import sys
        sys.print_exception(e)

def run_oled_display():
    """Run OLED display in a separate thread"""
    try:
        # Try to read BLE name from file
        ble_name = "Unknown"
        try:
            with open('ble_name.txt', 'r') as f:
                ble_name = f.read().strip()
            log(f"[OLED] Successfully read BLE name: {ble_name}")
        except Exception as e:
            log(f"[OLED] Could not read ble_name.txt: {e}")

        log(f"[MAIN] Starting OLED display service... (BLE: {ble_name})")

        # Check I2C before importing oled_display
        try:
            from machine import I2C, Pin
            i2c = I2C(0, scl=Pin(22), sda=Pin(21))
            devices = i2c.scan()
            log(f"[OLED] I2C scan found {len(devices)} device(s): {[hex(d) for d in devices]}")
            if not devices:
                log("[OLED] WARNING: No I2C devices found!")
        except Exception as e:
            log(f"[OLED] I2C check failed: {e}")

        import oled_display
        log("[OLED] oled_display module imported successfully")
        # Call the main() function to start the OLED display
        oled_display.main()

    except Exception as e:
        log(f"[ERROR] OLED display failed: {e}")
        import sys
        sys.print_exception(e)

def main():
    """Main entry point - start both services"""
    log("=" * 50)
    log("ESP32 Auto-Start System")
    log("=" * 50)

    # Log system information
    try:
        import machine
        log(f"[INFO] Reset cause: {machine.reset_cause()}")
        log(f"[INFO] Frequency: {machine.freq() / 1000000} MHz")
        log(f"[INFO] Free memory: {gc.mem_free()} bytes")
    except Exception as e:
        log(f"[INFO] Could not get system info: {e}")

    log("[INFO] Starting services...")
    log("")

    # Start OLED display in a separate thread
    try:
        _thread.start_new_thread(run_oled_display, ())
        log("[MAIN] ✓ OLED display thread started")
        time.sleep(1)  # Give OLED time to initialize
    except Exception as e:
        log(f"[WARNING] Could not start OLED display: {e}")
        log("[INFO] Continuing without OLED display...")

    # Run BLE provisioning in the main thread
    # This allows us to receive serial input in the main thread
    try:
        log("[MAIN] ✓ Starting BLE provisioning in main thread")
        log("")
        run_ble_provisioning()
    except KeyboardInterrupt:
        log("\n[MAIN] Shutdown requested...")
        global running
        running = False
    except Exception as e:
        log(f"[ERROR] Main thread error: {e}")
        import sys
        sys.print_exception(e)
    finally:
        close_log()

if __name__ == "__main__":
    # Small delay to allow boot messages to complete
    time.sleep(2)

    # Setup logging first
    import gc
    log_file = setup_logging()
    if log_file:
        log(f"[INFO] Logging to: {log_file}")

    main()
