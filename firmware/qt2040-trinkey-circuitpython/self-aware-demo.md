# Self-Aware Script Demo

## Description

Demonstrates how CircuitPython scripts can read their own deployment metadata from `settings.toml`. The script identifies itself by reading environment variables populated from the settings file, then blinks an LED to show it's running.

## Hardware Requirements

- Board: Any CircuitPython board (tested on RP2350 Pimoroni Tiny2350)
- Peripherals: None (uses built-in LED)

## Features

- Reads `DEPLOYED_SCRIPT`, `DEPLOYED_DATE`, `DEPLOYED_SIZE`, `DEPLOYED_CHECKSUM` from environment
- Auto-detects LED pin (LED_G for RP2350, LED for generic boards)
- Blinks LED 5 times to show execution

## Notes

This demonstrates the "self-aware script" pattern where code can identify its own provenance at runtime.

<!-- Deployment details auto-updated below by deploy-circuitpy.sh -->


## Latest Deployment

**Deployed to:** CircuitPython Device (CIRCUITPY)  
**Original Filename:** `qt2040-trinkey-circuitpython/self-aware-demo.py`  
**File Size:** 1325 bytes  
**Date File Updated:** 2025-12-11 16:42:36 PST  
**Date Deployed:** 2025-12-11 16:47:34 PST  
**SHA256 Checksum:** `05200eb36e753bbdcffc9f5587f332c89d7d8fba6da94743ccfb8ed4b9b47267`

### Verification

```bash
# Check filesize
ls -l /Volumes/CIRCUITPY/code.py | awk '{print $5}'
# Expected: 1325

# Check checksum
shasum -a 256 /Volumes/CIRCUITPY/code.py
# Expected: 05200eb36e753bbdcffc9f5587f332c89d7d8fba6da94743ccfb8ed4b9b47267
```

---

## Latest Deployment

**Deployed to:** CircuitPython Device (CIRCUITPY)  
**Original Filename:** `qt2040-trinkey-circuitpython/self-aware-demo.py`  
**File Size:** 1325 bytes  
**Date File Updated:** 2025-12-11 16:42:36 PST  
**Date Deployed:** 2025-12-11 16:48:05 PST  
**SHA256 Checksum:** `05200eb36e753bbdcffc9f5587f332c89d7d8fba6da94743ccfb8ed4b9b47267`

### Verification

```bash
# Check filesize
ls -l /Volumes/CIRCUITPY/code.py | awk '{print $5}'
# Expected: 1325

# Check checksum
shasum -a 256 /Volumes/CIRCUITPY/code.py
# Expected: 05200eb36e753bbdcffc9f5587f332c89d7d8fba6da94743ccfb8ed4b9b47267
```
