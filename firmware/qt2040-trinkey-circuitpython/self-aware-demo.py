#!/usr/bin/env python3
"""
Self-Aware Script Demo
Reads deployment metadata from settings.toml to identify itself
"""

import os
import time
import board
import digitalio

print("=" * 60)
print("Self-Aware Script Demo")
print("=" * 60)

# Read deployment metadata from settings.toml
deployed_script = os.getenv("DEPLOYED_SCRIPT", "unknown")
deployed_date = os.getenv("DEPLOYED_DATE", "unknown")
deployed_size = os.getenv("DEPLOYED_SIZE", "unknown")
deployed_checksum = os.getenv("DEPLOYED_CHECKSUM", "unknown")

print(f"I am: {deployed_script}")
print(f"Deployed on: {deployed_date}")
print(f"My size: {deployed_size} bytes")
print(f"My checksum: {deployed_checksum[:16]}...")
print("=" * 60)
print()

# Simple LED blink to show we're running
led = None
if hasattr(board, 'LED_G'):
    led = digitalio.DigitalInOut(board.LED_G)
    led.direction = digitalio.Direction.OUTPUT
    print("Using LED_G (RP2350)")
elif hasattr(board, 'LED'):
    led = digitalio.DigitalInOut(board.LED)
    led.direction = digitalio.Direction.OUTPUT
    print("Using LED (generic)")

if led:
    for i in range(5):
        print(f"Blink {i+1}/5")
        led.value = True
        time.sleep(0.5)
        led.value = False
        time.sleep(0.5)
    print("\nDemo complete!")
else:
    print("No LED found, but metadata works!")
    time.sleep(5)
