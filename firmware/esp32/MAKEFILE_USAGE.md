# ESP32 Makefile Usage Guide

This directory includes a professional Makefile for deploying MicroPython code to ESP32 devices.

## Prerequisites

- `mpremote` installed: `pip install mpremote`
- ESP32 board connected via USB

## Quick Start

```bash
# Deploy everything (recommended)
make deploy

# Deploy to a specific port
make deploy PORT=/dev/ttyUSB0
```

## Available Commands

### Deployment

```bash
# Full deployment: install libraries + upload files + reset
make deploy

# Deploy to specific port (macOS example)
make deploy PORT=/dev/tty.usbserial-0001

# Deploy to specific port (Linux example)
make deploy PORT=/dev/ttyUSB0

# Only install required libraries
make install-libs

# Only upload .py and .txt files (skip library installation)
make upload-files
```

### Device Management

```bash
# Reset the ESP32
make reset

# List files on the device
make ls

# Connect to REPL console
make repl

# Remove all .py and .txt files from device
make clean-device
```

### Help

```bash
# Show all available commands
make help
```

## How It Works

The Makefile automates the deployment process:

1. **Port Detection**: Auto-detects ESP32 port or uses `PORT=` parameter
2. **Library Installation**: Installs required packages (e.g., ssd1306 for OLED)
3. **File Upload**: Uploads all `.py` and `.txt` files from `esp32/` directory
4. **Device Reset**: Resets ESP32 to run the new code
5. **Instructions**: Shows how to connect to REPL for monitoring

## Examples

### Basic Deployment

```bash
cd esp32
make deploy
```

### Deploy to Specific Port

```bash
# macOS
make deploy PORT=/dev/tty.usbserial-0001

# Linux
make deploy PORT=/dev/ttyUSB0

# Windows (in Git Bash or WSL)
make deploy PORT=COM3
```

### Development Workflow

```bash
# 1. Make changes to .py files
vim main.py

# 2. Quick upload (skip library install)
make upload-files

# 3. Connect to see output
make repl
```

### Troubleshooting

```bash
# Check what's on the device
make ls

# Clean everything and start fresh
make clean-device
make deploy

# Reset if device is unresponsive
make reset
```

## Comparison: Makefile vs deploy.sh

### Makefile Advantages:

✅ **Modular targets**: Run individual steps (install-libs, upload-files, etc.)
✅ **Standard tool**: Widely known, no extra dependencies
✅ **Make features**: Dependency tracking, parallel execution (if needed)
✅ **Professional**: Standard approach for build automation
✅ **Cleaner syntax**: More readable than bash for this use case
✅ **Built-in help**: `make help` shows all commands

### Bash Script Advantages:

✅ **Simpler for beginners**: More familiar to some users
✅ **More verbose output**: Easier to follow step-by-step
✅ **No make required**: Works on systems without make (rare)

## Migration from deploy.sh

Both tools do the same thing. You can:

**Option 1: Use Makefile** (recommended)
```bash
make deploy
```

**Option 2: Keep using deploy.sh**
```bash
./deploy.sh
```

**Option 3: Use both**
- Use `make deploy` for daily development
- Keep `deploy.sh` as backup or for CI/CD

## Tips

1. **Tab Completion**: Most shells support tab completion for make targets
   ```bash
   make dep<TAB>  # Completes to 'make deploy'
   ```

2. **Default Target**: Running just `make` shows help
   ```bash
   make  # Same as 'make help'
   ```

3. **Port Persistence**: Set PORT in environment to avoid repeating
   ```bash
   export PORT=/dev/ttyUSB0
   make deploy  # Uses PORT from environment
   ```

4. **Parallel Development**: Each developer can use their own PORT
   ```bash
   # Developer 1
   make deploy PORT=/dev/ttyUSB0

   # Developer 2 (different board)
   make deploy PORT=/dev/ttyUSB1
   ```

## Customization

Edit the Makefile to add custom targets:

```makefile
# Example: Add a backup target
backup:
	@mkdir -p backups
	@mpremote $(PORT_ARG) cp :main.py backups/main.py.bak
	@echo "Backup created"
```

## Color Output

The Makefile uses ANSI colors for better readability:
- **Blue**: Section headers
- **Green**: Success messages
- **Yellow**: Warnings
- **Red**: Errors

If colors don't work in your terminal, edit the Makefile and comment out the color definitions.

## See Also

- [MicroPython mpremote documentation](https://docs.micropython.org/en/latest/reference/mpremote.html)
- [GNU Make manual](https://www.gnu.org/software/make/manual/)
- Original deployment: `deploy.sh`
