#!/bin/bash

# RP2350 RS485 Build & Upload Script
# This script helps streamline building and uploading firmware to the Pico 2 W

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project configuration
PROJECT_NAME="RP2350 RS485"
ENV_NAME="rp2350"

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo ""
    echo "Commands:"
    echo "  build       - Build the project"
    echo "  upload      - Upload firmware to device"
    echo "  monitor     - Open serial monitor"
    echo "  clean       - Clean build files"
    echo "  all         - Build and upload (default)"
    echo "  help        - Show this help message"
    echo ""
    echo "Options:"
    echo "  --verbose   - Enable verbose output"
    echo "  --port PORT - Specify upload port (auto-detect if not provided)"
    echo ""
    echo "Examples:"
    echo "  $0                    # Build and upload"
    echo "  $0 build              # Just build"
    echo "  $0 upload --port /dev/cu.usbmodem14201"
    echo "  $0 monitor            # Open serial monitor"
}

# Function to check if PlatformIO is installed
check_platformio() {
    if ! command -v pio &> /dev/null; then
        print_error "PlatformIO CLI not found!"
        print_status "Please install PlatformIO:"
        print_status "  Option 1: pip install platformio"
        print_status "  Option 2: curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o get-platformio.py && python3 get-platformio.py"
        exit 1
    fi
}

# Function to build the project
build_project() {
    print_status "Building $PROJECT_NAME..."

    if [ "$VERBOSE" = true ]; then
        pio run -e $ENV_NAME --verbose
    else
        pio run -e $ENV_NAME
    fi

    if [ $? -eq 0 ]; then
        print_success "Build completed successfully!"

        # Show build info
        FIRMWARE_PATH=".pio/build/$ENV_NAME/firmware.uf2"
        if [ -f "$FIRMWARE_PATH" ]; then
            FIRMWARE_SIZE=$(ls -lh "$FIRMWARE_PATH" | awk '{print $5}')
            print_status "Firmware size: $FIRMWARE_SIZE"
            print_status "Firmware location: $FIRMWARE_PATH"
        fi
    else
        print_error "Build failed!"
        exit 1
    fi
}

# Function to upload firmware
upload_firmware() {
    print_status "Uploading firmware to Pico 2 W..."

    # Check if a specific port was provided
    if [ -n "$UPLOAD_PORT" ]; then
        print_status "Using specified port: $UPLOAD_PORT"
        UPLOAD_CMD="pio run -e $ENV_NAME --target upload --upload-port $UPLOAD_PORT"
    else
        print_status "Auto-detecting upload port..."
        UPLOAD_CMD="pio run -e $ENV_NAME --target upload"
    fi

    # Instructions for BOOTSEL mode
    print_warning "Make sure your Pico 2 W is in BOOTSEL mode:"
    print_status "1. Hold the BOOTSEL button while connecting USB"
    print_status "2. Or hold BOOTSEL and press RESET if already connected"
    print_status "3. The Pico should appear as a USB mass storage device"

    # Wait for user confirmation
    read -p "Press Enter when your Pico 2 W is in BOOTSEL mode and ready for upload..."

    if [ "$VERBOSE" = true ]; then
        $UPLOAD_CMD --verbose
    else
        $UPLOAD_CMD
    fi

    if [ $? -eq 0 ]; then
        print_success "Upload completed successfully!"
        print_status "Your Pico 2 W should now be running the new firmware."
    else
        print_error "Upload failed!"
        print_status "Troubleshooting:"
        print_status "- Ensure the Pico is in BOOTSEL mode"
        print_status "- Check USB connection"
        print_status "- Try a different USB cable or port"
        exit 1
    fi
}

# Function to open serial monitor
open_monitor() {
    print_status "Opening serial monitor..."
    print_status "Press Ctrl+C to exit monitor"
    print_warning "Make sure to press RESET on your Pico after upload to see output!"

    # Use platformio's built-in monitor
    pio device monitor --environment $ENV_NAME --baud 115200
}

# Function to clean build files
clean_project() {
    print_status "Cleaning build files..."
    pio run -e $ENV_NAME --target clean

    if [ $? -eq 0 ]; then
        print_success "Clean completed!"
    else
        print_error "Clean failed!"
        exit 1
    fi
}

# Function to show project info
show_project_info() {
    print_status "Project: $PROJECT_NAME"
    print_status "Environment: $ENV_NAME"
    print_status "Platform: Raspberry Pi (RP2350)"
    print_status "Board: Pico 2 W"
    print_status "Framework: Arduino"
    echo ""
}

# Parse command line arguments
COMMAND="all"
VERBOSE=false
UPLOAD_PORT=""

while [[ $# -gt 0 ]]; do
    case $1 in
        build|upload|monitor|clean|all|help)
            COMMAND="$1"
            ;;
        --verbose)
            VERBOSE=true
            ;;
        --port)
            UPLOAD_PORT="$2"
            shift
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
    shift
done

# Main script execution
main() {
    echo "=================================================="
    echo "  RP2350 RS485 Build & Upload Script"
    echo "=================================================="
    echo ""

    show_project_info
    check_platformio

    case $COMMAND in
        "build")
            build_project
            ;;
        "upload")
            upload_firmware
            ;;
        "monitor")
            open_monitor
            ;;
        "clean")
            clean_project
            ;;
        "all")
            build_project
            echo ""
            upload_firmware
            echo ""
            print_status "Would you like to open the serial monitor? (y/n)"
            read -p "Monitor: " -n 1 -r
            echo ""
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                open_monitor
            fi
            ;;
        "help")
            show_usage
            ;;
        *)
            print_error "Unknown command: $COMMAND"
            show_usage
            exit 1
            ;;
    esac
}

# Run main function
main "$@"
