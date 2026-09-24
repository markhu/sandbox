# Open questions - M5StickS3 dark-screen bring-up

Captured mid-investigation. See BRINGUP_NOTES.md for the fuller technical
trail; these are the specific things I'm stuck on / most want answered.

1. **Why did the "settle delay + retry M5.begin() up to 5x" fix not help at
   all?** If the root cause were a PMIC-not-ready-yet race on fresh
   power-up, at least one of 5 attempts spaced 250ms apart should have
   succeeded. Zero change in behavior suggests the I2C probe (or the
   GPIO47/48 pull-up precheck that gates it) is *deterministically*
   failing, not flaky-timing failing. What's actually different between
   this unit's bus state and what M5GFX's autodetect expects?

2. **Why is serial output completely uncapturable in this sandbox?**
   Neither pyserial (multiple DTR/RTS strategies), raw `cat`, nor
   `stty`+`cat` ever returned a single byte - not even the ROM boot banner
   in most attempts, despite esptool successfully talking to the same port
   moments earlier for flashing/chip_id. Is this a USB-passthrough
   limitation of the sandbox (bulk/interrupt CDC data endpoints not
   forwarded, only control transfers used by esptool's slip protocol), or
   something host-driver-specific? Without resolving this, all further
   firmware-side debugging is flying blind on visual-only feedback.

3. **Does the factory UiFlow2 firmware actually prove the LCD hardware is
   good, or could *it* also be relying on a different, more tolerant
   init path** (e.g. MicroPython/UIFlow's board bring-up code may retry
   differently, use different pins, or not use the M5PM1 autodetect logic
   in M5GFX at all)? I inferred "hardware is fine" from the USB descriptor
   name alone - I never actually watched the factory firmware render
   anything before we overwrote it. That inference could be wrong.

4. **Is `M5Unified@0.2.23`'s `board_M5StickS3` support actually mature/
   tested,** given the registry metadata showed it published very
   recently? Is there a changelog, GitHub issue, or newer patch version
   that specifically addresses a StickS3 autodetect failure like this one?

5. **My own repeated DTR/RTS manipulation earlier put the chip into
   download-mode (`waiting for download`) several times.** I *believe*
   the final `esptool --after hard_reset` calls cleanly returned it to
   normal boot before each subsequent flash, but I never got independent
   confirmation of that (see question 2) - could the board actually still
   be stuck in a bootloader/download state right now rather than running
   application code at all, making "screen dark" a red herring for a
   totally different problem (app never runs) rather than a display-init
   bug?

6. **Is there a hardware `RST`/`EN` button distinct from the side G0
   button** that would let me force a clean, unambiguous power-on reset
   independent of all this USB-line-level guessing - and would that be a
   more reliable way to get a "ground truth" boot than anything I've tried
   over the serial/USB connection?
