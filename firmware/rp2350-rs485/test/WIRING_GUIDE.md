# RS485 Wiring Guide for RP2350

## Quick Reference

### RP2350 to RS485 Module Connections

```

RP2350 Pico 2          RS485 Module
┌─────────────┐        ┌──────────┐
│             │        │          │
│    GP0 (TX) ├────────┤ DI       │
│             │        │          │
│    GP1 (RX) ├────────┤ RO       │
│             │        │          │
│    GP2 (RTS)├────────┤ DE       │
│             │        │          │
│    GP2 (RTS)├────────┤ RE       │
│             │        │          │
│    3.3V     ├────────┤ VCC      │
│             │        │          │
│    GND      ├────────┤ GND      │
│             │        │          │
└─────────────┘        └──────────┘
                           │  │
                           A  B
                           │  │
                    To RS485 Bus
```

## RS485 Bus Topology

### Two Device Setup

```blockdiagram
Device 1                          Device 2
┌────────┐                        ┌────────┐
│ RS485  │    Twisted Pair        │ RS485  │
│        │                        │        │
│ A ─────┼────────────────────────┼───── A │
│        │                        │        │
│ B ─────┼────────────────────────┼───── B │
│        │                        │        │
└────────┘                        └────────┘
    ║                                 ║
   120Ω                              120Ω
Terminator                       Terminator
```

### Multi-Device Setup (Up to 32 devices)

```diagram
   120Ω                                                    120Ω
    ║                                                       ║
Device 1        Device 2        Device 3        ...    Device N
┌────────┐     ┌────────┐     ┌────────┐             ┌────────┐
│        │     │        │     │        │             │        │
│ A ─────┼─────┼────────┼─────┼────────┼─── ... ─────┼───── A │
│        │     │        │     │        │             │        │
│ B ─────┼─────┼────────┼─────┼────────┼─── ... ─────┼───── B │
│        │     │        │     │        │             │        │
└────────┘     └────────┘     └────────┘             └────────┘
```

## Hardware Components

### Required Components

1. **RP2350 Board** (Raspberry Pi Pico 2)
2. **RS485 Transceiver Module** (one of):
   - MAX485 module
   - MAX3485 module
   - SN75176 module
   - Similar RS485/RS422 transceiver

3. **Termination Resistors**
   - 120Ω resistors (2 required, one at each end of bus)
   - Should match the characteristic impedance of the cable

4. **Twisted Pair Cable**
   - Cat5/Cat6 Ethernet cable works well
   - Use one twisted pair for A/B signals
   - Keep unused pairs grounded or leave disconnected

### Recommended Modules

- **HW-0519** (MAX485 based) - Common and inexpensive
- **XY-017** (MAX485 based)
- **Any MAX3485-based module** - Better noise immunity

## Detailed Wiring Steps

### Step 1: Connect Power

```text
RP2350 3.3V  →  RS485 Module VCC
RP2350 GND   →  RS485 Module GND
```

**Note:** Most RS485 modules work with both 3.3V and 5V. Check your module's datasheet.

### Step 2: Connect UART Signals

```text
RP2350 GP0 (UART0 TX)  →  RS485 Module DI (Driver Input)
RP2350 GP1 (UART0 RX)  →  RS485 Module RO (Receiver Output)
```

### Step 3: Connect Direction Control

```txt
RP2350 GP2 (RTS)  →  RS485 Module DE (Driver Enable)
RP2350 GP2 (RTS)  →  RS485 Module RE (Receiver Enable)
```

**Important:** DE and RE must be tied together and controlled by the same GPIO pin.

### Step 4: Connect to RS485 Bus

```txt
RS485 Module A  →  Twisted Pair Wire 1 (e.g., Orange in Cat5)
RS485 Module B  →  Twisted Pair Wire 2 (e.g., Orange/White in Cat5)
```

### Step 5: Add Termination

Install 120Ω resistor between A and B at **both ends** of the bus only.

## Pin Customization

To use different GPIO pins, modify these definitions in [`src/main.cpp`](../src/main.cpp:57):

```cpp
#define RS485_TX_PIN 0    // Change to your desired TX pin
#define RS485_RX_PIN 1    // Change to your desired RX pin
#define RS485_DE_PIN 2    // Change to your desired DE/RE control pin
```

### Available UART Pins on RP2350

**UART0:**

- TX: GP0, GP12, GP16, GP28
- RX: GP1, GP13, GP17, GP29

**UART1:**

- TX: GP4, GP8, GP20, GP24
- RX: GP5, GP9, GP21, GP25

## Troubleshooting

### No Communication

- [ ] Check all power connections (3.3V and GND)
- [ ] Verify A connects to A, B connects to B (not crossed)
- [ ] Ensure DE and RE are tied together
- [ ] Check termination resistors are installed
- [ ] Verify baud rate matches on all devices

### Intermittent Communication

- [ ] Add or check 120Ω termination resistors
- [ ] Use twisted pair cable
- [ ] Reduce cable length
- [ ] Check for loose connections
- [ ] Ensure proper grounding

### One-Way Communication Only

- [ ] Verify DE/RE control pin is connected
- [ ] Check GPIO pin number in code matches hardware
- [ ] Test DE/RE pin with LED to verify it's toggling

## Testing

### LED Test for DE/RE Pin

Add an LED with resistor to GP2 to visually confirm direction switching:

```
GP2 → 330Ω Resistor → LED Anode → LED Cathode → GND
```

LED should blink when transmitting.

### Loopback Test

For initial testing without a second device:
1. Connect A to A and B to B on the same module (short circuit)
2. The device should receive its own transmissions
3. Check serial monitor for "Received: Hello from RP2350!..."

## Safety Notes

- ⚠️ Do not hot-plug RS485 connections while powered
- ⚠️ Ensure voltage levels are compatible (3.3V vs 5V)
- ⚠️ Use proper ESD protection when handling boards
- ⚠️ Double-check polarity before powering on

## Additional Resources

- [RS485 Standard Overview](https://en.wikipedia.org/wiki/RS-485)
- [RP2350 Pinout](https://datasheets.raspberrypi.com/pico/Pico-2-Pinout.pdf)
- [MAX485 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX1487-MAX491.pdf)
