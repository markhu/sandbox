# QT2040 Trinkey CircuitPython Project

## 🚀 Quick Start - READ THIS FIRST!

### Essential Files to Check
- **DEPLOYMENT_NOTES.md** - Complete deployment guide and library management
- **deploy.sh** - Deployment script for pushing code to device

### Current Project Status
- ✅ Device: QT2040 Trinkey connected at `/Volumes/CIRCUITPY`
- ✅ Libraries: Required CircuitPython libraries installed
- ✅ Active Script: `lava-domino.py` - Random Double-16 Domino Pattern Display

## 📁 Project Files

### Main Scripts
- **lava-domino.py** - Random double-16 domino pattern generator for I2C 13x9 matrix
- **is31fl3741-lava-lamp.py** - Rainbow lava lamp effect
- **i2c-lava-lamp.py** - I2C lava lamp effect
- **neo-lava.py** - NeoPixel lava effect
- **neo-ambient-lava.py** - Ambient NeoPixel lava effect
- **code.py** - Base/template file
- **blinker.py** - Simple blinker example
- **serial_monitor.py** - Serial output monitoring

### Deployment & Documentation
- **deploy.sh** - Automated deployment script
- **DEPLOYMENT_NOTES.md** - Comprehensive deployment and library guide

### Libraries
- **adafruit-circuitpython-bundle-10.x-mpy-20250924/** - CircuitPython library bundle

## ⚡ Quick Commands

```bash
# Deploy any script to device
./deploy.sh [script-name.py]

# Deploy current domino script
./deploy.sh lava-domino.py

# Check device connection
ls -la /Volumes/CIRCUITPY/

# Check library status
ls -la /Volumes/CIRCUITPY/lib/
```

## 🔧 Hardware Setup
- **Device**: QT2040 Trinkey
- **I2C Matrix**: 13x9 LED matrix (IS31FL3741 controller)
- **Status LED**: Built-in NeoPixel
- **Mount Point**: `/Volumes/CIRCUITPY`

## 📚 For Detailed Information
See **DEPLOYMENT_NOTES.md** for complete deployment procedures, library management, and troubleshooting.

---
*Last Updated: 2025-09-24 - Random Double-16 Domino Pattern Generator deployed*
