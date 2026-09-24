# AGENTS.md - firmware/m5stack

Project-specific instructions for working in this directory (M5StickC Plus
ClickLogger firmware + supporting tools).

## Use `serial_cmd.py` for serial commands

Don't hand-write `pyserial` snippets for STATUS/DUMP/CLEAR/THRESH/etc. Use:

```bash
python3 serial_cmd.py STATUS DUMP
python3 serial_cmd.py "THRESH 4500" HALLON CLEAR
python3 serial_cmd.py --wait 8 SERVE       # slow commands need more settle time
python3 serial_cmd.py --listen 20          # just listen, no commands sent
```

It auto-detects the board's port and never touches `esptool`/`chip_id`, so it
never resets the board (safe mid-data-collection).

## Do NOT use `health_check.py` during active testing

It calls `esptool chip_id`, which hard-resets the board. Fine for a one-time
connectivity check, but never during a data-collection run.

## Settings are RAM-only, reset on every reboot/reflash

`THRESH_ON`/`THRESH_OFF`, `SNIPON`/`SNIPOFF`, `HALLON`/`HALLOFF` are not
persisted. After any `pio run -t upload`, re-apply via `serial_cmd.py`
before resuming testing.

## Read HANDOFF.md first

`HANDOFF.md` in this directory has the full project narrative: current
physical hardware setup (which solenoid, mounting position), what's been
tried and ruled out, what's still open, infrastructure details (push
receiver, WiFi network, adb device IDs). Read it before assuming anything
about project state - it's kept up to date across sessions specifically so
context isn't lost.
