---
name: data-logging
description: Use when running a hardware data-collection session that involves a serial-attached microcontroller board PLUS an Android app driven via adb (e.g. triggering a real-world device/actuator through its companion app while logging sensor data off a dev board's serial/HTTP interface). Covers reliable adb UI automation, per-board serial/IP identity gotchas, and verifying logged data against the real on-device source of truth.
---

# Data logging: adb-driven app + serial-attached board

General lessons from running physical data-collection sessions where an Android
app (e.g. a companion app for some real-world device) must be driven via `adb`
to trigger events, while a microcontroller dev board logs sensor data via
serial and/or its own HTTP server. None of this is project-specific — reapply
wherever this same shape of task comes up.

## adb UI automation: tap coordinates

**Never eyeball tap coordinates from a screenshot.** `adb exec-out screencap -p`
often returns a PNG scaled DOWN from the device's real resolution (e.g. a
900px-wide PNG on a 1080px-wide phone). Tapping at pixel positions read off
that image will be off by the scale factor and silently hit the wrong
element — this produced several wrong-app/wrong-device taps in practice
before switching approach.

**Always get real bounds via `uiautomator dump` first**, then tap the center
of the specific element you want, using the coordinates it reports directly
(uiautomator bounds are already in real device-pixel space, matching what
`adb shell input tap` expects — no scaling needed):

```bash
adb -s <serial> shell uiautomator dump /sdcard/ui.xml
adb -s <serial> shell cat /sdcard/ui.xml > /tmp/ui.xml
grep -o 'text="[^"]*"\|content-desc="[^"]*"\|bounds="[^"]*"' /tmp/ui.xml | paste - - -
```

Look for `content-desc` or `text` matching the button/row you want, take its
`bounds="[x1,y1][x2,y2]"`, and tap the center: `((x1+x2)/2, (y1+y2)/2)`.
Custom-rendered apps (Flutter, etc.) often lack resource-ids but usually still
expose `content-desc` on interactive elements — good enough to key off.

Still verify with a screenshot after each tap when navigating anything
non-trivial (multi-step flows, dialogs, screens that auto-navigate away on
completion). Don't chain multiple blind taps in one script until the flow is
confirmed step-by-step at least once — screen state (offline banners, sync
spinners, sort order changes, auto-navigation after a running task finishes)
shifts element positions between runs in ways a fixed script won't anticipate.

## Multiple similar devices/rows in a list

If the app lists multiple similar-looking devices/entries (e.g. several
controllers under one account), don't assume list order is stable — sort
order can depend on connection state, last-synced time, low-battery/offline
flags, etc., and can silently reorder between your screenshots. Re-check with
`uiautomator dump` (matching on the specific name/content-desc string) before
every tap rather than reusing a remembered row position.

## BLE / wireless companion devices going offline

If the target device shows offline/"No Bluetooth" in the app and the
manual-trigger button is greyed out, retrying in software may not help —
some devices need a physical wake (a tap/nudge/power-cycle) before the phone
can reconnect. Don't assume this is a scripting bug; check the app's device
list for an offline/no-signal indicator first, and ask the user to physically
intervene if so.

## Per-board serial port / IP identity

A microcontroller board's `/dev/cu.usbserial-XXXXXXXX` suffix and its
DHCP-assigned IP address are tied to that specific physical board (or that
specific boot), not to the project. Symptoms this bites you on:

- **Different board plugged in** (new unit, or same model after a fresh
  provisioning) → old hardcoded `upload_port`/serial-suffix config
  (`platformio.ini`, helper scripts' `--serial` defaults) silently fails or
  targets nothing. Identify the new port before uploading, e.g.:
  ```bash
  ls /dev/cu.*
  system_profiler SPUSBDataType 2>/dev/null | grep -B10 -A2 "Serial Number"
  ```
  Match on the manufacturer/product string you expect (e.g. an FTDI chip's
  "M5stack"/"Hades2001" strings), then update the hardcoded port/suffix.

- **Same board, fresh reboot/reflash** → its DHCP-assigned IP can change
  between boots even though the serial port stays the same. If a helper
  command (e.g. `SERVE`) prints the current IP, use THAT value for any
  subsequent `curl`, don't reuse an IP from a previous boot.

## Verify data via the authoritative source, not an intermediate transport

If a serial command's read-back of a log looks truncated or corrupted, don't
assume real data loss before checking an alternate, more authoritative path
(e.g. the board's own HTTP file server, if it has one) — serial reads can be
artifacts of read-timing (buffer not fully flushed yet) rather than the
actual on-flash/on-device state. Confirmed pattern: a `DUMP`-over-serial
command returned a file that looked cut off mid-line, but the same file
fetched via `curl` from the board's own webserver was complete. Re-verify
before reporting/treating something as a bug.

## Long real-world gaps between tool calls

If a session pauses for user input/clarification and picks back up much
later, real-world clock time may have advanced far more than apparent
conversation-turn count suggests (minutes to hours). If the logging device
timestamps events with gap durations, a large unexpected gap value is a good
signal to check for this rather than assuming corrupted data. Also re-check
whether anything with its own independent schedule (e.g. a real-world
device's own scheduled program, unrelated automation) fired autonomously
during that gap, producing extra logged events you didn't intend to trigger.
