# ESP32 MicroPython Deployment Makefile
# Usage:
#   make deploy          - Deploy all files to ESP32
#   make deploy PORT=/dev/ttyUSB0  - Deploy to specific port
#   make ble-name        - Upload ble_name.txt only
#   make test-ble        - Run BLE diagnostic test
#   make install-libs    - Install required libraries only
#   make upload-files    - Upload files only (no library install)
#   make reset           - Reset the ESP32
#   make repl            - Connect to REPL
#   make ls              - List files on ESP32
#   make clean-device    - Remove all .py and .txt files from device
#   make help            - Show this help message

.PHONY: help deploy install-libs upload-files reset repl ls clean-device check-mpremote test-ble ble-name

# Configuration
PORT ?=
SCRIPT_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
PORT_ARG := $(if $(PORT),connect $(PORT),)

# File patterns to upload
PY_FILES := $(wildcard $(SCRIPT_DIR)*.py)
TXT_FILES := $(wildcard $(SCRIPT_DIR)*.txt)

# Colors for output (optional, comment out if not supported)
BLUE := \033[0;34m
GREEN := \033[0;32m
YELLOW := \033[0;33m
RED := \033[0;31m
NC := \033[0m # No Color
help:
	@echo "$(BLUE)ESP32 MicroPython Deployment$(NC)"
	@echo ""
	@echo "$(GREEN)Available targets:$(NC)"
	@echo "  make deploy          - Full deployment (install libs + upload files + reset)"
	@echo "  make deploy PORT=/dev/ttyUSB0  - Deploy to specific port"
	@echo "  make ble-name        - Upload ble_name.txt only (update BLE name)"
	@echo "  make test-ble        - Run BLE diagnostic test on device"
	@echo "  make install-libs    - Install required libraries only"
	@echo "  make upload-files    - Upload .py and .txt files only"
	@echo "  make reset           - Reset the ESP32"
	@echo "  make repl            - Connect to REPL console"
	@echo "  make ls              - List files on ESP32"
	@echo "  make clean-device    - Remove all .py and .txt files from device"
	@echo "  make help            - Show this help message"
	@echo ""
	@echo "$(YELLOW)Examples:$(NC)"
	@echo "  make deploy                    # Auto-detect port"
	@echo "  make deploy PORT=/dev/ttyUSB0  # Specific port"
	@echo "  make ble-name                  # Update BLE name from ble_name.txt"
	@echo "  make test-ble                  # Test BLE functionality"
	@echo "  make repl                      # Connect to REPL"
	@echo "  make repl                      # Connect to REPL"

check-mpremote:
	@which mpremote > /dev/null 2>&1 || \
		(echo "$(RED)ERROR: mpremote is not installed$(NC)" && \
		 echo "Install with: pip install mpremote" && \
		 exit 1)

detect-port: check-mpremote
	@echo "$(BLUE)Detecting ESP32...$(NC)"
	@if [ -n "$(PORT)" ]; then \
		echo "$(GREEN)✓ Using specified port: $(PORT)$(NC)"; \
	else \
		DEVS=$$(mpremote devs 2>&1 | grep -E '/dev/|COM' | head -n 1 | awk '{print $$1}'); \
		if [ -n "$$DEVS" ]; then \
			echo "$(GREEN)✓ Found ESP32 at: $$DEVS$(NC)"; \
		else \
			echo "$(YELLOW)⚠ No port detected, using mpremote auto-detection$(NC)"; \
		fi \
	fi
	@echo ""

install-libs: detect-port
	@echo "$(BLUE)Installing required libraries...$(NC)"
	@echo "  Installing ssd1306 library for OLED display..."
	@mpremote $(PORT_ARG) mip install ssd1306 || \
		(echo "$(YELLOW)WARNING: Failed to install ssd1306 library$(NC)" && \
		 echo "OLED display functionality may not work")
	@echo ""

upload-files: detect-port
	@echo "$(BLUE)Uploading Python files...$(NC)"
	@for file in $(PY_FILES); do \
		filename=$$(basename $$file); \
		echo "  Uploading: $$filename"; \
		mpremote $(PORT_ARG) cp "$$file" ":$$filename" || exit 1; \
	done
	@echo ""
	@echo "$(BLUE)Uploading text files...$(NC)"
	@for file in $(TXT_FILES); do \
		filename=$$(basename $$file); \
		echo "  Uploading: $$filename"; \
		mpremote $(PORT_ARG) cp "$$file" ":$$filename" || exit 1; \
	done
	@echo ""

deploy: install-libs upload-files
	@echo "$(GREEN)================================================$(NC)"
	@echo "$(GREEN)✓ Deployment complete!$(NC)"
	@echo "$(GREEN)================================================$(NC)"
	@echo ""
	@echo "Uploaded files:"
	@mpremote $(PORT_ARG) ls
	@echo ""
	@$(MAKE) reset
	@echo ""
	@echo "To see the output, run:"
	@if [ -n "$(PORT)" ]; then \
		echo "  make repl PORT=$(PORT)"; \
		echo "  OR: mpremote connect $(PORT) repl"; \
		echo "  OR: screen $(PORT) 115200"; \
	else \
		echo "  make repl"; \
		echo "  OR: mpremote repl"; \
	fi
	@echo ""
	@echo "$(YELLOW)Troubleshooting:$(NC)"
	@echo "  If BLE is not working, run: make test-ble"
	@echo "  See BLE_TROUBLESHOOTING.md for detailed help"
	@echo ""
	@echo "Note: The ESP32 has been reset and will auto-start"
	@echo "      Check the qas-TIMESTAMP.log file for boot diagnostics"

ble-name: check-mpremote
	@echo "$(BLUE)Uploading ble_name.txt...$(NC)"
	@if [ ! -f "ble_name.txt" ]; then \
		echo "$(RED)ERROR: ble_name.txt not found!$(NC)"; \
		echo "Create it first with your desired BLE name, e.g.:"; \
		echo "  echo 'MY-ESP32-123' > ble_name.txt"; \
		exit 1; \
	fi
	@echo "Current content of ble_name.txt:"
	@cat ble_name.txt
	@echo ""
	@mpremote $(PORT_ARG) cp ble_name.txt :ble_name.txt
	@echo "$(GREEN)✓ ble_name.txt uploaded successfully$(NC)"
	@echo ""
	@echo "$(YELLOW)Note: The BLE name will update automatically within 5 seconds$(NC)"
	@echo "      Or reset the device for immediate effect: make reset"

test-ble: check-mpremote
	@echo "$(BLUE)Running BLE diagnostic test...$(NC)"
	@echo "$(YELLOW)This will run the test_ble.py script on your ESP32$(NC)"
	@echo ""
	@echo "If test_ble.py is not on the device, uploading it first..."
	@mpremote $(PORT_ARG) cp test_ble.py :test_ble.py 2>/dev/null || true
	@echo ""
	@echo "$(GREEN)Starting BLE test (will run for 30 seconds)...$(NC)"
	@echo "$(YELLOW)Open a BLE scanner app on your phone and look for 'ESP32-TEST'$(NC)"
	@echo ""
	@mpremote $(PORT_ARG) exec "import test_ble"
	@echo ""
	@echo "$(GREEN)Test complete!$(NC)"
	@echo "See BLE_TROUBLESHOOTING.md for help if the test failed"

reset: check-mpremote
	@echo "$(BLUE)Resetting ESP32...$(NC)"
	@mpremote $(PORT_ARG) reset

repl: check-mpremote
	@echo "$(BLUE)Connecting to REPL...$(NC)"
	@echo "Press Ctrl+] to exit"
	@echo ""
	@mpremote $(PORT_ARG) repl

ls: check-mpremote
	@mpremote $(PORT_ARG) ls

clean-device: check-mpremote
	@echo "$(RED)Removing all .py and .txt files from ESP32...$(NC)"
	@echo "Are you sure? (Press Ctrl+C to cancel)"
	@read -p "Press Enter to continue..."
	@mpremote $(PORT_ARG) exec "import os; [os.remove(f) for f in os.listdir() if f.endswith('.py') or f.endswith('.txt')]" || true
	@echo "$(GREEN)✓ Cleanup complete$(NC)"
	@$(MAKE) ls
