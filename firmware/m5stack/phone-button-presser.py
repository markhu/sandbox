#!/usr/bin/env python3
"""adb-screenshot-annotate.py - Capture Android screenshots with metadata annotation."""

import argparse
import hashlib
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
import tempfile
import socket
import sys
import os
from PIL import Image, ImageDraw, ImageFont
from datetime import datetime
import time


# Known Rain Bird RB2 activities
APP_PACKAGE = 'com.rainbird.rainbird2dev'

KNOWN_ACTIVITIES = {
    'home': {
        'name': 'Rain Bird Home',
        'activity': f'{APP_PACKAGE}/com.rainbird.rb2.home.view.HomeActivity',
    },
    'splash': {
        'name': 'Splash',
        'activity': f'{APP_PACKAGE}/com.rainbird.rb2.home.view.SplashActivity',
    },
    # NOTE: there is deliberately no 'settings' entry here. RB2 is
    # architected as effectively a single/few-Activity app with internal
    # (Compose Navigation-style) routing: when an Activity's task is
    # already running, `am start -n <Activity>` doesn't reset it, it just
    # redelivers the Intent to whatever's already on top via
    # onNewIntent() (the "Activity not started, intent has been delivered
    # to currently running top-most instance" warning you may see from
    # adb). A `--go settings` slug was tried and removed after testing
    # showed it unreliable: fresh from Home it did not navigate anywhere
    # at all, and there's no registered deep link for it either (only
    # 'home' and 'about' are, per CommonUtils/CommonConstants.py) to fall
    # back on. Only add a slug back for a screen once you've confirmed
    # either its Activity path deterministically navigates there, or (as
    # with 'about' below) a real deep link exists for it.
    'about': {
        'name': 'About',
        # NOT a directly launchable Activity: "About" is a bottom sheet
        # hosted inside a shared host Activity (with other screens like
        # App Settings and Legal), so `am start -n` on that Activity just
        # opens whichever of those screens was last showing rather than
        # specifically "About" - the component name alone is ambiguous
        # between them. The app instead exposes an actual navigation deep
        # link for this (see MobileApp_Automation/conftest.py's
        # launch_Deep_Link() / CommonUtils/CommonConstants.py's
        # aboutLink), which is used here instead of an Activity path.
        'deeplink': 'rainbird://navigation?about',
    },
}


def resolve_activity(activity_arg):
    """Resolve a --go argument (slug, 1-based number, full Activity path,
    or full deep-link URI) to a launch target.

    Returns a (target_type, target_value) tuple where target_type is
    'activity' (launched via `am start -n`) or 'deeplink' (launched via
    `am start -a android.intent.action.VIEW -d <uri>`), or None if
    unresolved.
    """
    if not activity_arg:
        return None
    
    # A literal deep-link URI passed directly (e.g. "rainbird://navigation?about").
    if '://' in activity_arg:
        return ('deeplink', activity_arg)
    
    # A literal full Activity path passed directly (has /). Note this only
    # works for screens that are each their own distinct Activity; several
    # RB2 screens (e.g. About, Legal) share a host Activity with other
    # screens and can't be reached this way - use a deep link (see 'about'
    # above) or a slug/number for those instead.
    if '/' in activity_arg:
        return ('activity', activity_arg)
    
    # Try numeric index (1-based)
    try:
        idx = int(activity_arg) - 1
        activities_list = list(KNOWN_ACTIVITIES.keys())
        if 0 <= idx < len(activities_list):
            activity_arg = activities_list[idx]
    except ValueError:
        pass
    
    # Slug lookup
    entry = KNOWN_ACTIVITIES.get(activity_arg.lower())
    if not entry:
        return None
    if 'deeplink' in entry:
        return ('deeplink', entry['deeplink'])
    return ('activity', entry['activity'])


def get_connected_devices(detailed=False):
    """Get list of connected Android devices."""
    try:
        cmd = 'adb devices -l'

        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=10)

        if result.returncode != 0:
            return []

        devices = []
        if detailed:
          for line in result.stdout.split('\n'):
            if 'device' in line and 'List of devices' not in line:
                parts = line.split()
                if parts:
                    serial = parts[0]
                    info = {}
                    for part in parts[2:]:
                        if ':' in part:
                            key, _, value = part.partition(':')
                            info[key] = value
                    devices.append({'serial': serial, 'info': info})
        else:
          for line in result.stdout.split('\n'):
            if 'device' in line and 'List of devices' not in line:
                parts = line.split()
                if parts:
                    devices.append(parts[0])

        return devices
    except Exception as e:
        print(f"Warning: Failed to get devices: {e}", file=sys.stderr)
        return []


def sleep_device(device):
    """Put the device screen to sleep."""
    try:
        subprocess.run(f'adb -s {device} shell input keyevent 26',
                      shell=True, capture_output=True, timeout=5)
        print(f"Device {device} screen put to sleep", file=sys.stderr)
    except Exception as e:
        print(f"Warning: Failed to sleep device: {e}", file=sys.stderr)


def wake_device(device):
    """Wake the device using idempotent method (WAKEUP keyevent, not toggle)."""
    try:
        # Use KEYCODE_WAKEUP (224) which is idempotent - wakes without sleeping
        subprocess.run(f'adb -s {device} shell input keyevent 224',
                      shell=True, capture_output=True, timeout=5)
        time.sleep(0.5)
    except Exception as e:
        print(f"Warning: Failed to wake device: {e}", file=sys.stderr)


def extract_screen_name_from_xml(xml_path):
    """Extract screen name from uiautomator XML."""
    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()
        
        # Empty XML means UIAutomator failed/timed out
        if len(list(root)) == 0:
            return None
        
        for node in root.iter('node'):
            if node.get('resource-id', '').endswith('toolbarTitle'):
                text = node.get('text', '')
                if text:
                    return text
        
        for node in root.iter('node'):
            if 'TextView' in node.get('class', ''):
                text = node.get('text', '')
                # Skip common non-screen-name texts
                if text and len(text) > 1 and text not in ['Calendar', 'Today', 'Tomorrow', 'Weather']:
                    return text
        
        return None
    except Exception as e:
        print(f"Warning: XML parsing failed: {e}", file=sys.stderr)
        return None


def extract_top_line_from_page_source(xml_path):
    """Return the text= attribute of the node that is visually topmost on
    screen in the uiautomator page-source XML. Document/tree order does not
    correspond to visual order, so this ranks nodes by the top-y coordinate
    of their `bounds` attribute (e.g. "[0,72][1080,168]") rather than by
    traversal order.

    Nodes with zero-area bounds (e.g. "[0,0][0,0]") are excluded: these are
    off-screen/not-rendered elements (RecyclerView items outside the
    viewport, hidden views, etc.) that would otherwise incorrectly win by
    virtue of having top=0."""
    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()

        best_text = None
        best_top = None
        for node in root.iter('node'):
            text = node.get('text', '').strip()
            if not text:
                continue
            bounds = node.get('bounds', '')
            match = re.match(r'\[(\d+),(\d+)\]\[(\d+),(\d+)\]', bounds)
            if not match:
                continue
            left, top, right, bottom = map(int, match.groups())
            # Skip zero-area (off-screen/unrendered) nodes.
            if right <= left or bottom <= top:
                continue
            if best_top is None or top < best_top:
                best_top = top
                best_text = text

        return best_text
    except Exception as e:
        print(f"Warning: XML parsing failed: {e}", file=sys.stderr)
        return None


def extract_top_line_via_ocr(image_path, status_bar_height=80, min_confidence=60):
    """Fallback for screens where uiautomator can't produce a page-source
    dump at all (e.g. an actively-rendering camera preview for QR
    scanning, which never reaches the accessibility "idle" state
    uiautomator dump requires). Runs OCR directly on the screenshot pixels
    via the `tesseract` CLI and returns the topmost, highest-confidence
    line of recognized text.

    The `tesseract` binary is invoked directly via subprocess (not via the
    pytesseract Python binding) to avoid requiring a pip install into this
    externally-managed Python environment.

    status_bar_height: pixel rows to skip from the top, to avoid the
    unreliable/garbled OCR of the system status bar (clock, icons, etc.)
    winning simply because it's higher on screen.
    min_confidence: tesseract confidence (0-100) a line's words must
    average to be considered; filters out noise/garbage recognition.

    Returns None if tesseract isn't installed, OCR fails, or no
    sufficiently confident text is found below the status bar.
    """
    if shutil.which('tesseract') is None:
        return None
    
    try:
        result = subprocess.run(
            ['tesseract', image_path, 'stdout', '--psm', '6', 'tsv'],
            capture_output=True, text=True, timeout=15
        )
        if result.returncode != 0:
            return None
        
        # Parse the TSV output, grouping words back into lines using
        # (block_num, par_num, line_num), and rank lines by their top
        # y-coordinate (visual order) rather than TSV row order.
        lines = {}  # (block, par, line) -> {'top': int, 'words': [(conf, text)]}
        rows = result.stdout.strip().split('\n')
        if len(rows) < 2:
            return None
        header = rows[0].split('\t')
        col = {name: i for i, name in enumerate(header)}
        
        for row in rows[1:]:
            fields = row.split('\t')
            if len(fields) != len(header):
                continue
            try:
                level = int(fields[col['level']])
                top = int(fields[col['top']])
                conf = float(fields[col['conf']])
                text = fields[col['text']].strip()
            except (ValueError, KeyError):
                continue
            # level 5 = word-level rows in tesseract TSV output.
            if level != 5 or not text:
                continue
            if top < status_bar_height:
                continue
            # Single-character tokens are almost always OCR misreads of
            # icon glyphs (back/close/hamburger buttons, etc.) rather than
            # real words, aside from the standalone words "a"/"I". Drop
            # them so they don't get glued onto the start of a heading.
            if len(text) == 1 and text not in ('a', 'A', 'I'):
                continue
            key = (fields[col['block_num']], fields[col['par_num']], fields[col['line_num']])
            entry = lines.setdefault(key, {'top': top, 'words': []})
            entry['top'] = min(entry['top'], top)
            entry['words'].append((conf, text))
        
        if not lines:
            return None
        
        # Pick the visually topmost line whose average word confidence
        # clears the threshold, to skip garbled/low-confidence noise.
        for key, entry in sorted(lines.items(), key=lambda kv: kv[1]['top']):
            confidences = [c for c, _ in entry['words'] if c >= 0]
            if not confidences:
                continue
            avg_conf = sum(confidences) / len(confidences)
            if avg_conf >= min_confidence:
                return ' '.join(t for _, t in entry['words'])
        
        return None
    except Exception as e:
        print(f"Warning: OCR fallback failed: {e}", file=sys.stderr)
        return None


def hash_activity_suffix(class_name, length=8):
    """Hash the stable, meaningful class-name portion of an Activity
    identifier (e.g. "home.view.HomeActivity"), returning a short hex
    digest suitable for use as a filename/headline suffix.

    Deliberately hashes only the class_name (as returned by
    split_activity_display), not the full Activity string, because the app
    ID prefix differs between builds (e.g. "rainbird2dev" in dev vs
    "rainbird2" in prod) while the class name stays identical. Hashing the
    full string would produce different hashes for the same screen across
    builds."""
    if not class_name:
        return ''
    digest = hashlib.sha1(class_name.encode('utf-8')).hexdigest()
    return digest[:length]


def split_activity_display(activity):
    """Split an Activity identifier like
    "com.rainbird.rainbird2dev/com.rainbird.rb2.provisioning.bleseries.view.BleSeriesSelectActivity"
    into (prefix, class_name):
      prefix     = "rainbird2dev/com.rainbird.rb2" (fixed app/package
                   boilerplate, anchored at the literal "com.rainbird.rb2"
                   package)
      class_name = "provisioning.bleseries.view.BleSeriesSelectActivity"
                   (everything after the anchor - the accurate,
                   human-meaningful part identifying the actual screen)

    The split point is a fixed string anchor, not a segment-count heuristic
    (e.g. "last 3 dot segments"), since a count-based heuristic incorrectly
    swallows leading class_name segments (like "provisioning") into the
    prefix whenever the class path is deeper than expected.

    Falls back to (activity, '') if the shape isn't recognized.
    """
    if not activity or '/' not in activity:
        return activity or '', ''
    
    app_id, _, class_part = activity.partition('/')
    
    # Trim the common package boilerplate for brevity on the app id.
    common_prefix = 'com.rainbird.'
    if app_id.startswith(common_prefix):
        app_id = app_id[len(common_prefix):]
    
    # Fixed anchor marking the boundary between app-specific boilerplate
    # and the meaningful, screen-identifying class name.
    anchor = 'com.rainbird.rb2'
    if class_part.startswith(anchor + '.'):
        prefix = f'{app_id}/{anchor}'
        class_name = class_part[len(anchor) + 1:]  # strip anchor + leading dot
    else:
        # Anchor not found; nothing to trim from the class name.
        prefix = app_id
        class_name = class_part
    
    return prefix, class_name


def wrap_dotted_text(text, font, max_width, draw):
    """Wrap text at dot ('.') boundaries only, keeping each dot attached to
    the start of the following segment (Java package-path style), rather
    than breaking mid-word by estimated character width. Segments are
    greedily packed onto each line using real pixel-width measurement
    (draw.textlength) so multiple short segments may share a line when
    they fit."""
    if not text:
        return [text]
    
    segments = re.findall(r'\.[^.]*|[^.]+', text)
    if not segments:
        return [text]
    
    lines = []
    current_line = ""
    for segment in segments:
        test_line = current_line + segment
        if not current_line or draw.textlength(test_line, font=font) <= max_width:
            current_line = test_line
        else:
            lines.append(current_line)
            current_line = segment
    if current_line:
        lines.append(current_line)
    
    return lines


def get_current_activity(device):
    """Get the currently focused (resumed) activity via dumpsys.

    Uses `dumpsys activity activities` and its `ResumedActivity:` line,
    which has stayed a stable, singular indicator of the foreground
    Activity across Android versions. The previously-used `dumpsys window
    windows` / `mFocusedApp=` approach broke on newer Android releases
    (observed on Android 13+ devices, and confirmed on an Android 16
    device) where `mFocusedApp` no longer appears in that dump at all,
    having been replaced by a differently-formatted `topApp=` field.

    Also drops the requirement that the class name end in the literal
    word "Activity" (e.g. system components like
    "com.android.settings/.Settings" don't), which the old regex silently
    excluded.
    """
    try:
        cmd = f'timeout 5 adb -s {device} shell dumpsys activity activities 2>&1'
        
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=10)
        
        if result.returncode != 0:
            return None
        
        content = result.stdout
        
        # Extract the component (package/Class) from the ResumedActivity
        # line, e.g.:
        #   ResumedActivity: ActivityRecord{87575955 u0 com.foo/.BarActivity t5225}
        # Matching up to the first whitespace or stray "}" (some Android
        # versions emit an extra "}" immediately after the component name
        # before the task id) avoids capturing trailing junk.
        match = re.search(r'ResumedActivity: ActivityRecord\{\S+ u\d+ ([^\s}]+)', content)
        if match:
            return match.group(1)
        
        return None
    except Exception as e:
        return None


def fingerprint_screen_from_dumpsys(device):
    """Identify screen by examining dumpsys activity info and activity name."""
    try:
        cmd = f'timeout 5 adb -s {device} shell dumpsys activity activities 2>&1'
        
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=10)
        
        if result.returncode != 0:
            return None
        
        content = result.stdout
        
        # Extract the component from the ResumedActivity line (see
        # get_current_activity for why this replaced the old
        # mFocusedApp-based approach).
        match = re.search(r'ResumedActivity: ActivityRecord\{\S+ u\d+ ([^\s}]+)', content)
        if not match:
            return None
        focused_activity = match.group(1)
        
        if 'com.rainbird.rainbird2dev' in focused_activity:
            if 'HomeActivity' in focused_activity:
                return "Rain Bird Home"
            elif 'SettingsActivity' in focused_activity or 'SettingsContainerActivity' in focused_activity:
                return "App Settings"
            elif 'AboutActivity' in focused_activity:
                return "About"
        
        return None
    except Exception as e:
        print(f"Warning: Dumpsys fingerprint detection failed: {e}", file=sys.stderr)
        return None


def close_modal_if_present(device, xml_path):
    """Check for and close modals/dialogs by clicking Close button or OK button."""
    try:
        with open(xml_path, 'r') as f:
            content = f.read()
        
        # Check for close button (X)
        if 'ivClose' in content and 'content-desc="Close"' in content:
            print("Modal detected with Close button, closing it...", file=sys.stderr)
            subprocess.run(f'adb -s {device} shell input tap 84 276',
                          shell=True, capture_output=True, timeout=5)
            time.sleep(1)
            return True
        
        # Check for OK button
        if 'btnMigratingDevicesOk' in content or ('text="OK"' in content and 'clickable="true"' in content):
            print("Modal detected with OK button, clicking it...", file=sys.stderr)
            subprocess.run(f'adb -s {device} shell input tap 540 1755',
                          shell=True, capture_output=True, timeout=5)
            time.sleep(1)
            return True
        
        return False
    except Exception as e:
        print(f"Warning: Failed to detect/close modal: {e}", file=sys.stderr)
        return False


def has_back_button(xml_path):
    """Check if the current screen has a back button in the toolbar."""
    try:
        with open(xml_path, 'r') as f:
            content = f.read()
        # Look for the back button indicator in toolbar
        return 'content-desc="Back"' in content or 'content-desc="Navigate up"' in content
    except:
        return False


def launch_activity(device, activity):
    """Launch a specific activity on the device."""
    try:
        subprocess.run(f'adb -s {device} shell am start -n {activity}',
                      shell=True, capture_output=True, timeout=5)
        time.sleep(2)
    except Exception as e:
        print(f"Warning: Failed to launch activity: {e}", file=sys.stderr)


def launch_deeplink(device, uri, package=APP_PACKAGE):
    """Launch a navigation deep link (e.g. "rainbird://navigation?about")
    via an explicit VIEW intent. `package` scopes the intent to a single
    app so Android doesn't pop up a disambiguation ResolverActivity when
    multiple installed builds (dev/staging/prod) register the same URI
    scheme."""
    try:
        subprocess.run(f'adb -s {device} shell am start -a android.intent.action.VIEW -d "{uri}" -p {package}',
                      shell=True, capture_output=True, timeout=5)
        time.sleep(2)
    except Exception as e:
        print(f"Warning: Failed to launch deep link: {e}", file=sys.stderr)


def dump_uiautomator_xml(device, remote_path='/sdcard/layout.xml', retries=1, retry_delay=0.5):
    """Run `uiautomator dump` and return True only if it actually succeeded.

    `uiautomator dump` can fail fast (e.g. "ERROR: could not get idle
    state" when an animation, bottom sheet, or spinner is active) without
    timing out or returning a non-zero exit code from the wrapping `adb
    shell` invocation. If a stale XML from a previous successful dump is
    still sitting on the device's /sdcard, a naive pull-after-dump would
    silently succeed with outdated data instead of surfacing the failure.
    To avoid that, the stale remote file is removed first, so a failed
    dump results in a missing file rather than a stale one, and the dump
    output is inspected for success/failure rather than assumed.

    Only a single retry is attempted (by default): some failures are
    truly persistent for the duration of the screen (e.g. an actively
    rendering camera preview during QR-code scanning never reaches
    accessibility idle state, so no amount of retrying will help) rather
    than transient. Callers should fall back to OCR
    (extract_top_line_via_ocr) on the screenshot when this returns False,
    rather than retrying further.
    """
    def adb_shell(cmd, timeout=10):
        full_cmd = f'adb -s {device} shell {cmd}'
        return subprocess.run(full_cmd, shell=True, capture_output=True, text=True, timeout=timeout)
    
    # Remove any stale dump from a previous capture so a failed dump can't
    # be mistaken for a fresh, successful one.
    try:
        adb_shell(f'rm -f {remote_path}', timeout=5)
    except Exception:
        pass
    
    for attempt in range(retries + 1):
        try:
            result = adb_shell(f'timeout 8 uiautomator dump {remote_path}', timeout=10)
            output = (result.stdout or '') + (result.stderr or '')
            if 'dumped to' in output.lower():
                return True
            if attempt < retries:
                print(f"Warning: uiautomator dump failed ({output.strip() or 'no output'}), "
                      f"retrying...", file=sys.stderr)
                time.sleep(retry_delay)
        except subprocess.TimeoutExpired:
            if attempt < retries:
                print("Warning: uiautomator dump timed out, retrying...", file=sys.stderr)
    
    return False


def capture_direct(device):
    """Capture screenshot and XML directly."""
    tmpdir = tempfile.mkdtemp()
    
    # Wake device first
    wake_device(device)
    
    subprocess.run(f'adb -s {device} shell screencap /sdcard/screenshot.png',
                  shell=True, check=True, capture_output=True)
    subprocess.run(f'adb -s {device} pull /sdcard/screenshot.png {tmpdir}/screenshot.png',
                  shell=True, check=True, capture_output=True)
    
    # Try to get XML, but don't fail if the dump fails or times out (e.g.
    # active animations/bottom sheets on the device, or older Android
    # devices without full uiautomator support).
    if dump_uiautomator_xml(device):
        subprocess.run(f'adb -s {device} pull /sdcard/layout.xml {tmpdir}/layout.xml',
                      shell=True, capture_output=True, timeout=5)
    else:
        print("Warning: UIAutomator dump failed, skipping XML extraction "
              "(page-source-derived screen name/headline will fall back)", file=sys.stderr)
        # Create empty XML so extraction gracefully falls back
        with open(f'{tmpdir}/layout.xml', 'w') as f:
            f.write('<hierarchy />')
    
    model = subprocess.run(f'adb -s {device} shell getprop ro.product.model',
                          shell=True, capture_output=True, text=True).stdout.strip()
    device_id = subprocess.run(f'adb -s {device} shell getprop ro.product.device',
                              shell=True, capture_output=True, text=True).stdout.strip()
    timestamp = subprocess.run(f"adb -s {device} shell 'date \"+%Y-%m-%d %H:%M:%S\"'",
                              shell=True, capture_output=True, text=True).stdout.strip()
    
    return model, device_id, timestamp, tmpdir



def calculate_entropy(image_path):
    """Calculate Shannon entropy of image data (fast check for blank/locked screens)."""
    try:
        from PIL import Image
        img = Image.open(image_path)
        pixels = list(img.getdata())
        
        # Count frequency of pixel values
        freq = {}
        for p in pixels:
            freq[p] = freq.get(p, 0) + 1

        # Calculate entropy
        import math
        total = len(pixels)
        entropy = 0.0
        for f in freq.values():
            if f > 0:
                p = f / total
                entropy -= p * math.log2(p)
        return entropy
    except Exception as e:
        return -1.0


def check_blank_or_locked_screen(screenshot_path, threshold=0.13):
    """Check if screenshot appears to be blank or locked (low entropy).

    threshold is deliberately very low: legitimate, mostly-white/sparse
    screens (e.g. the About screen, which is nearly all whitespace with a
    few lines of text) have been observed with entropy as low as ~0.2 while
    a truly blank/solid-color screen (locked, all-black, etc.) has entropy at or near 0.
    """
    entropy = calculate_entropy(screenshot_path)
    if entropy < 0:
        return False, entropy

    if entropy < threshold:
        return True, entropy
    return False, entropy


def detect_terminal_graphics_support():
    """Detect if terminal supports inline image display (Kitty, iTerm2, WezTerm, Ghostty, etc.)."""
    term = os.environ.get('TERM', '').lower()
    term_program = os.environ.get('TERM_PROGRAM', '').lower()

    # Known terminals with inline graphics support
    supported_terms = [ 'kitty', 'iterm2', 'wezterm' ]
    
    # Check TERM variable first
    for term_name in supported_terms:
        if term_name in term:
            return term
    
    # Check TERM_PROGRAM (mainly for macOS)
    if 'iterm' in term_program:
        return 'iterm2'
    if 'wezterm' in term_program:
        return 'wezterm'
    if 'ghostty' in term_program:
        # Ghostty implements the Kitty graphics protocol, so reuse the
        # existing 'kitty' code path (kitten icat) rather than a new type.
        return 'kitty'
    
    # WezTerm also sets TERM=wezterm
    if 'wezterm' in term:
        return 'wezterm'
    
    return None


def get_terminal_size():
    """Return (lines, cols) of the controlling terminal, or (None, None)
    if it can't be determined."""
    try:
        result = subprocess.run(['stty', 'size'], capture_output=True, text=True, timeout=2)
        if result.returncode == 0:
            lines, cols = map(int, result.stdout.strip().split())
            return lines, cols
    except Exception:
        pass
    return None, None


def scroll_terminal_to_blank(lines):
    """Print `lines` blank lines to push all current on-screen content
    (including any previously displayed image) up into the terminal's
    scroll-back history, leaving a clean, blank viewport to draw into.
    Without this, inline images placed at an absolute position (e.g. Kitty's
    --place @0x0) simply overwrite the top of the screen instead of scrolling
    the old content into history."""
    if lines:
        sys.stdout.write('\n' * lines)
        sys.stdout.flush()


def display_image_inline(image_path, terminal_type, reserved_lines=0):
    """Display image inline using terminal-specific command.

    reserved_lines: number of terminal rows to leave below the image, sized
    to fit the caller's textual metadata block that gets printed after the
    image. This is also used as the scroll amount so old content (including
    any previously displayed image) is pushed into scroll-back rather than
    being overwritten.
    """
    try:
        term_lines, term_cols = get_terminal_size()
        scroll_terminal_to_blank(term_lines)
        if "kitty" in terminal_type:
            # Kitty icat with fit-to-height: display image sized to leave
            # `reserved_lines` rows free at the bottom for the metadata text
            # printed by the caller after this function returns.
            try:
                if term_lines:
                    available_lines = max(1, term_lines - reserved_lines)
                    # Note: --place positions cursor at top-left of image, so we move it below after
                    subprocess.run(['kitten', 'icat', '--align=left', '--place', f'{term_cols}x{available_lines}@0x0', '--scale-up', image_path], check=False)
                    # Move cursor just below the placed image so the metadata
                    # text the caller prints next appears directly beneath it.
                    sys.stdout.write(f'\033[{available_lines + 1}H')
                    sys.stdout.flush()
                else:
                    # Fallback if stty fails
                    subprocess.run(['kitten', 'icat', '--align=left', image_path], check=False)
            except Exception:
                # Fallback on any exception
                subprocess.run(['kitten', 'icat', '--align=left', image_path], check=False)
        elif terminal_type == 'iterm2':
            # iTerm2 inline image protocol
            with open(image_path, 'rb') as f:
                data = f.read()
            import base64
            b64_data = base64.b64encode(data).decode('ascii')
            sys.stdout.write(f'\033]1337;File=name={os.path.basename(image_path).encode().hex()};inline=1:{b64_data}\007')
            sys.stdout.flush()
        elif terminal_type == 'wezterm':
            # WezTerm uses OSC image protocol (similar to iTerm2)
            with open(image_path, 'rb') as f:
                data = f.read()
            import base64
            b64_data = base64.b64encode(data).decode('ascii')
            sys.stdout.write(f'\033]1337;File=name={os.path.basename(image_path).encode().hex()};inline=1:{b64_data}\007')
            sys.stdout.flush()
    except Exception as e:
        print(f"Warning: Failed to display image inline: {e}", file=sys.stderr)


def wrap_text_smart(text, font, max_width):
    """Wrap text intelligently to fit within max_width, breaking at dots or slashes."""
    if not text:
        return [text]
    
    lines = []
    current_line = ""
    
    # Split on common separators but keep them with the text
    # Break on: . / or space
    parts = []
    current = ""
    for char in text:
        current += char
        if char in './ ':
            parts.append(current)
            current = ""
    if current:
        parts.append(current)
    
    # Now build lines trying to fit within max_width
    for part in parts:
        test_line = current_line + part
        # Estimate width (rough approximation - 0.6 * font size per character)
        estimated_width = len(test_line) * font.size * 0.6
        
        if estimated_width <= max_width:
            current_line = test_line
        else:
            if current_line:
                lines.append(current_line)
            current_line = part
    
    if current_line:
        lines.append(current_line)
    
    return lines if lines else [text]


def display_screenshot_by_index(device=None, index=0, description=""):
    """Helper to display a screenshot by index with error handling."""
    screenshot = find_latest_screenshot(device, index)
    if not screenshot:
        print(f"Error: No annotated screenshots found in /tmp", file=sys.stderr)
        sys.exit(1)
    
    terminal_type = detect_terminal_graphics_support()
    if terminal_type:
        print(f"Displaying {description}: {screenshot} (TERM:{terminal_type})", file=sys.stderr)
        display_image_inline(screenshot, terminal_type)
    else:
        print("Warning: Terminal does not support inline graphics display", file=sys.stderr)
        print(screenshot)


def find_latest_screenshot(device=None, index=0):
    """Find annotated screenshot by index.
    
    Negative indices: relative to newest (-1=previous, -2=older, etc.)
    Positive indices: absolute from oldest (0=oldest, 1=next oldest, etc.)
    """
    try:
        pattern = f'{device}_*_ent*.png' if device else '*_ent*.png'
        result = subprocess.run(f'ls -t /tmp/{pattern} 2>/dev/null',
                              shell=True, capture_output=True, text=True, timeout=5)
        files = [f for f in result.stdout.strip().split('\n') if f and os.path.exists(f)]
        
        if not files:
            return None
        
        # Negative indices: relative to newest (most recent is at index -1)
        # Positive indices: absolute from oldest (oldest is at index 0)
        if index < 0:
            # -1 is newest (latest), -2 is previous, etc.
            # files are sorted newest-first, so -1 maps to index 0
            actual_index = -1 - index  # -1->0, -2->1, -3->2, etc.
        else:
            # 0 is oldest, 1 is next oldest, etc.
            # files are sorted newest-first, so reverse
            actual_index = len(files) - 1 - index

        if 0 <= actual_index < len(files):
            return files[actual_index]
        return None
    except Exception as e:
        print(f"Warning: Failed to find screenshot: {e}", file=sys.stderr)
        return None


def main():
    parser = argparse.ArgumentParser(
        description='Capture Android screenshots with metadata.',
        epilog="Screen-name suffix is the first 8 hex chars of the SHA-1 of the Activity's class name "
               "(e.g. 'home.view.HomeActivity'). To sanity-check it by hand:\n"
               "  echo -n 'home.view.HomeActivity' | shasum -a 1 | cut -c1-8\n"
               "(use sha1sum instead of shasum on Linux)\n"
               "\n"
               "--go/--activity vs --deeplink:\n"
               "  --go/--activity launches an explicit Intent naming an exact Activity component\n"
               "  (slug, number, or full 'pkg/Class' path), e.g. --go home.\n"
               "  --deeplink instead launches an implicit VIEW Intent for a raw URI, which\n"
               "  Android/the app then routes internally - this is required for destinations that\n"
               "  aren't their own Activity (e.g. 'About', which lives inside a shared host\n"
               "  Activity and can't be reached by naming a component at all). Example:\n"
               "    ./adb-screenshot-annotate.py --deeplink 'rainbird://navigation?about'\n"
               "  Known KNOWN_ACTIVITIES slugs backed by a deep link (currently just 'about') can\n"
               "  still be reached via --go/--activity too (e.g. --go about) - resolve_activity()\n"
               "  auto-detects which launch mechanism a given slug/entry needs.\n"
               "\n"
               "Reliability caveat for Activity-path targets (home, splash, or a raw 'pkg/Class'\n"
               "  path): RB2 is largely a single/few-Activity app with internal Compose\n"
               "  Navigation-style routing, so `am start -n <Activity>` only reliably navigates\n"
               "  if that Activity's task isn't already running - otherwise Android just\n"
               "  redelivers the Intent to whatever screen is already on top (the 'Activity not\n"
               "  started, intent has been delivered to currently running top-most instance'\n"
               "  warning) rather than navigating there. A 'settings' slug was tried and removed\n"
               "  after testing showed exactly this: launched fresh from Home, it did not\n"
               "  navigate anywhere, and no deep link exists for it to fall back on either\n"
               "  (only 'home' and 'about' are registered, per CommonUtils/CommonConstants.py).\n"
               "  Prefer a deep link (--deeplink or a KNOWN_ACTIVITIES 'deeplink' entry) for any\n"
               "  screen you need to reach deterministically regardless of current app state.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument('--device', help='Device serial (defaults to only attached device if not specified; first detected if multiple)')
    parser.add_argument('--go', '--activity', dest='activity', help='Activity to launch (slug like "home", number, or full activity path)')
    parser.add_argument('--deeplink', dest='deeplink_uri', metavar='URI', help='Launch a raw navigation deep-link URI directly (e.g. "rainbird://navigation?about"), bypassing slug/Activity resolution. See epilog for the difference vs --go/--activity.')
    parser.add_argument('--output', help='Output file path')
    parser.add_argument('--get-screen-name-only', action='store_true', help='Print screen name and exit')
    parser.add_argument('--width-multiplier', type=float, default=1.0, help='Pillarbox width multiplier')
    parser.add_argument('--list', action='store_true', help='List connected Android devices')
    parser.add_argument('--sleep', '--asleep', dest='sleep_only', action='store_true', help='Put device screen to sleep only (no capture)')
    parser.add_argument('--awake', action='store_true', help='Keep device screen awake (prevent timeout)')
    parser.add_argument('--display', dest='display', action='store_true', default=None, help='Display screenshot inline in terminal (if supported)')
    parser.add_argument('--no-display', dest='display', action='store_false', help='Do not display screenshot inline')
    parser.add_argument('--display-last', action='store_true', help='Display the most recent annotated screenshot (no capture)')
    parser.add_argument('--display-prev', action='store_true', help='Display the previous annotated screenshot (same as --display-num -2)')
    parser.add_argument('--display-num', type=int, help='Display screenshot by index: negative=relative to newest (-1=prev, -2=older), positive=absolute from oldest (0=oldest, 1=next, etc.)')
    parser.set_defaults(display=None)  # Default to auto-detect

    args = parser.parse_args()

    # Handle --list
    if args.list:
        devices = get_connected_devices(detailed=True)
        if devices:
            print("Connected Android devices:")
            for device in devices:
                info = device['info']
                extras = ' '.join(f"{k}:{v}" for k, v in info.items() if k in ('model', 'product', 'device', 'transport_id'))
                print(f"  {device['serial']:<20} {extras}")
        else:
            print("No devices connected")
        return

    # Handle --display-last
    if args.display_last:
           display_screenshot_by_index(args.device, index=-1, description="latest")
           return
    
    # Handle --display-prev (same as --display-num -2)
    if args.display_prev:
        display_screenshot_by_index(args.device, index=-2, description="previous")
        return
    
    # Handle --display-num
    if args.display_num is not None:
          display_screenshot_by_index(args.device, index=args.display_num, description=f"screenshot at index {args.display_num}")
          return
    
    # Handle --sleep-only / --asleep (no capture, just put screen to sleep)
    if args.sleep_only:
         # Auto-select device if not specified: use the only device, or the
         # first detected if multiple are USB-connected.
         if not args.device:
             devices = get_connected_devices()
             if len(devices) == 0:
                 print("Error: No devices connected", file=sys.stderr)
                 sys.exit(1)
             args.device = devices[0]
             if len(devices) == 1:
                 print(f"Auto-selected device: {args.device}", file=sys.stderr)
             else:
                 print(f"Multiple devices connected ({', '.join(devices)}); "
                       f"defaulting to first: {args.device}", file=sys.stderr)
         
         print(f"Putting {args.device} screen to sleep...", file=sys.stderr)
         sleep_device(args.device)
         return
     
     # Auto-select device if not specified: use the only device, or the
     # first detected if multiple are USB-connected.
    if not args.device:
        devices = get_connected_devices()
        if len(devices) == 0:
            print("Error: No devices connected", file=sys.stderr)
            sys.exit(1)
        args.device = devices[0]
        if len(devices) == 1:
            print(f"Auto-selected device: {args.device}", file=sys.stderr)
        else:
            print(f"Multiple devices connected ({', '.join(devices)}); "
                  f"defaulting to first: {args.device}", file=sys.stderr)
    
    try:
        print(f"Capturing from {args.device}...", file=sys.stderr)
        # Keep screen awake if requested
        if args.awake:
            try:
                subprocess.run(f'adb -s {args.device} shell settings put system screen_off_timeout 2147483647',
                              shell=True, capture_output=True, timeout=5)
                print(f"Screen timeout disabled on {args.device}", file=sys.stderr)
            except Exception as e:
                print(f"Warning: Could not disable screen timeout: {e}", file=sys.stderr)

        
        # Resolve activity slug / deep link if provided. --go/--activity and
        # --deeplink are mutually exclusive: the former resolves through
        # KNOWN_ACTIVITIES (or a literal Activity path), the latter always
        # launches a raw URI directly via an implicit VIEW Intent.
        if args.activity and args.deeplink_uri:
            print("Error: --go/--activity and --deeplink are mutually exclusive", file=sys.stderr)
            sys.exit(1)
        
        if args.deeplink_uri:
            print(f"Launching deep link: {args.deeplink_uri}...", file=sys.stderr)
            launch_deeplink(args.device, args.deeplink_uri)
        elif args.activity:
            resolved_target = resolve_activity(args.activity)
            if not resolved_target:
                print(f"Error: Unknown activity '{args.activity}'", file=sys.stderr)
                print(f"Known activities: {', '.join(KNOWN_ACTIVITIES.keys())}", file=sys.stderr)
                sys.exit(1)
            target_type, target_value = resolved_target
            print(f"Launching {args.activity} ({target_type}: {target_value})...", file=sys.stderr)
            if target_type == 'deeplink':
                launch_deeplink(args.device, target_value)
            else:
                print("Note: Activity-path launches are only reliable if the app's "
                      "task isn't already running elsewhere in its nav stack - Android "
                      "may just redeliver the Intent to whatever screen is already on "
                      "top instead of navigating (see --help epilog).", file=sys.stderr)
                launch_activity(args.device, target_value)
                # Try to close any modal that might appear on Home after an
                # Activity-launch (e.g. "Migrating Devices"). Skipped for
                # deep links: this blind coordinate tap was observed to
                # instead hit and dismiss the deep-linked screen's own
                # close button (e.g. About's "X"), undoing the navigation
                # it was meant to help with.
                try:
                    subprocess.run(f'adb -s {args.device} shell input tap 84 276',
                                  shell=True, capture_output=True, timeout=5)
                    time.sleep(1)
                except:
                    pass  # If tap fails, continue
        
        # Capture
        model, device_id, timestamp, tmpdir = capture_direct(args.device)
        
        # Check for blank/locked screen
        screenshot_path = f'{tmpdir}/screenshot.png'
        is_blank, entropy = check_blank_or_locked_screen(screenshot_path)
        if is_blank:
            print(f"Warning: Screenshot appears to be blank or locked (entropy: {entropy:.2f}). "
                  f"Ensure device is unlocked and awake.", file=sys.stderr)
        
        # Try dumpsys fingerprint first (most reliable), then XML, then fallback
        screen_name = fingerprint_screen_from_dumpsys(args.device)
        if not screen_name:
            screen_name = extract_screen_name_from_xml(f'{tmpdir}/layout.xml')
        if not screen_name:
            screen_name = "Screen"
        
        # Compute the headline the same way regardless of --get-screen-name-only,
        # so both code paths always agree on the screen's identified name.
        activity = get_current_activity(args.device) or "unknown"
        activity_prefix, activity_class_name = split_activity_display(activity)
        
        # Headline = top line of the page-source XML text (the visible,
        # on-screen title), suffixed with a hash of the Activity's stable
        # class-name portion. Exception: the Home screen's top line is a
        # graphical logo with no text, so it falls back to the Activity
        # class name instead.
        page_source_top_line = extract_top_line_from_page_source(f'{tmpdir}/layout.xml')
        if not page_source_top_line:
            # No usable page-source (e.g. uiautomator dump failed because
            # an actively-rendering camera preview never reaches idle, as
            # with a QR-code scanning screen). Fall back to OCR directly
            # on the screenshot pixels.
            page_source_top_line = extract_top_line_via_ocr(screenshot_path)
        is_home_screen = 'HomeActivity' in activity_class_name
        if is_home_screen:
            top_line = activity_class_name or page_source_top_line or screen_name
        else:
            top_line = page_source_top_line or activity_class_name or screen_name
        activity_hash = hash_activity_suffix(activity_class_name)
        headline = f'{top_line}-{activity_hash}' if activity_hash else top_line
        
        if args.get_screen_name_only:
            print(headline)
            print(activity)
            # Persist the page-source XML using the same naming scheme as a
            # regular capture, even though no PNG is produced in this mode.
            timestamp_filename = datetime.now().strftime('%Y%m%d_%H%M%S')
            entropy_int = int(entropy * 100)
            page_source_output = f'/tmp/{args.device}_{timestamp_filename}_ent{entropy_int}.xml'
            try:
                shutil.copyfile(f'{tmpdir}/layout.xml', page_source_output)
                print(page_source_output, file=sys.stderr)
            except Exception as e:
                print(f"Warning: Failed to save page-source XML: {e}", file=sys.stderr)
            subprocess.run(f'rm -rf {tmpdir}', shell=True, capture_output=True)
            return
        
        print("Annotating...", file=sys.stderr)
        img = Image.open(screenshot_path)
        width, height = img.size
        pillarbox_width = int(width * args.width_multiplier)
        new_width = width + pillarbox_width
        
        new_img = Image.new('RGB', (new_width, height), color=(20, 20, 30))
        new_img.paste(img, (0, 0))
        
        # Load fonts (cross-platform, with fallback)
        def load_font(size):
            """Load TrueType font with fallback to default."""
            font_paths = [
                "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",  # Linux
                "/System/Library/Fonts/Helvetica.ttc",  # macOS
                "C:\\Windows\\Fonts\\arial.ttf",  # Windows
            ]
            for path in font_paths:
                try:
                    return ImageFont.truetype(path, size)
                except (IOError, OSError):
                    continue
            return ImageFont.load_default()
        
        title_font = load_font(64)
        info_font = load_font(44)
        small_font = load_font(32)
        activity_font = info_font
        
        draw = ImageDraw.Draw(new_img)
        text_color = (200, 200, 220)
        label_color = (150, 150, 180)
        right_x = width + 40
        y = 80
        
        # activity, activity_prefix, activity_class_name, and headline were
        # already computed above (shared with --get-screen-name-only) so
        # both code paths agree on the screen's identified name.
        
        # Calculate entropy and determine the output filename early so the
        # bare filename can be included as a metadata field on the image
        # itself (portability: the annotated PNG identifies itself even if
        # copied elsewhere without its original path).
        entropy = calculate_entropy(screenshot_path)
        hostname = socket.gethostname()
        
        if args.output:
            output = args.output
        else:
            # Generate unique filename with timestamp and entropy score
            timestamp_filename = datetime.now().strftime('%Y%m%d_%H%M%S')
            # Entropy without decimal point (e.g., 7.45 -> 745)
            entropy_int = int(entropy * 100)
            output = f'/tmp/{args.device}_{timestamp_filename}_ent{entropy_int}.png'
        output_filename = os.path.basename(output)
        
        draw.text((right_x - 20, y), headline, fill=text_color, font=title_font)
        draw.text((right_x, y + 100), "Phone:", fill=label_color, font=info_font)
        draw.text((right_x + 160, y + 100), f"{model} ({device_id})", fill=text_color, font=info_font)

        draw.text((right_x, y + 420), "Activity:", fill=label_color, font=info_font)
        # First part of the activity (package/app boilerplate) is shown dimmed
        # on the same line as the label; the accurate, meaningful class name
        # wraps onto the following line(s).
        max_activity_width = pillarbox_width - 80  # 80px margin
        if activity_class_name:
            label_width = draw.textlength("Activity:  ", font=info_font)
            draw.text((right_x + label_width, y + 425), activity_prefix, fill=label_color, font=small_font)
            activity_lines = wrap_dotted_text('.' + activity_class_name, activity_font, max_activity_width, draw)
        else:
            activity_lines = wrap_text_smart(activity, activity_font, max_activity_width)
        activity_y = y + 480
        for line in activity_lines:
            draw.text((right_x, activity_y), line, fill=text_color, font=activity_font)
            activity_y += 60

        draw.text((right_x, y + 690), "Date:", fill=label_color, font=info_font)
        draw.text((right_x + 160, y + 690), f"{timestamp}", fill=text_color, font=info_font)
        draw.text((right_x, y + 750), "Host:", fill=label_color, font=info_font)
        draw.text((right_x + 160, y + 750), f"{hostname}", fill=text_color, font=info_font)

        # Filename in smaller font, since it can be long and we don't want to truncate
        draw.text((right_x, y + 815), "File:", fill=label_color, font=small_font)
        draw.text((right_x + 80, y + 815), f"{output_filename}", fill=text_color, font=small_font)

        # Calculate and display filesize and entropy
        filesize_kb = None
        try:
            filesize = os.path.getsize(screenshot_path)
            filesize_kb = filesize / 1024
            draw.text((right_x, y + 880), "Filesize:", fill=label_color, font=info_font)
            draw.text((right_x + 250, y + 880), f"{filesize_kb:.1f} KB", fill=text_color, font=info_font)
            draw.text((right_x, y + 940), "Entropy:", fill=label_color, font=info_font)
            draw.text((right_x + 250, y + 940), f"{entropy:.2f}", fill=text_color, font=info_font)
        except Exception as e:
            draw.text((right_x, y + 880), f"(metadata error)", fill=label_color, font=info_font)

        # Draw activities list from bottom up, numbered
        activities_list = list(KNOWN_ACTIVITIES.items())
        activities_y = height - 100  # Start from near bottom
        draw.text((right_x, activities_y), "Quick --go", fill=label_color, font=info_font)
        activities_y -= 80

        # Draw activities in reverse (bottom to top)
        for idx, (slug, info) in enumerate(reversed(activities_list), start=1):
            text = f"{len(activities_list) - idx + 1}. {slug}"
            draw.text((right_x, activities_y), text, fill=text_color, font=info_font)
            activities_y -= 70

        new_img.save(output, 'PNG')

        print(output)

        # Persist the uiautomator page-source XML alongside the PNG, using
        # the same basename, so the two can be associated later (e.g. for
        # re-deriving the headline or debugging screen detection).
        page_source_output = os.path.splitext(output)[0] + '.xml'
        try:
            shutil.copyfile(f'{tmpdir}/layout.xml', page_source_output)
        except Exception as e:
            print(f"Warning: Failed to save page-source XML: {e}", file=sys.stderr)

        # Metadata lines printed textually *after* the image (below it) so
        # that scroll-back history reads image-then-metadata, top to bottom,
        # for each capture. The line count is used as the reserved bottom
        # margin when placing the image, so the image is sized to leave
        # exactly enough room for this block underneath it.
        metadata_lines = [
            f"Screen Name: {headline}",
            f"Phone:       {model} ({device_id})",
            f"Activity:    {activity}",
            f"Date:        {timestamp}",
            f"Host:        {hostname}",
            f"File:        {output_filename}",
        ]
        if filesize_kb is not None:
            metadata_lines.append(f"Filesize:    {filesize_kb:.1f} KB")
        metadata_lines.append(f"Entropy:     {entropy:.2f}")
        
        def print_metadata():
            for line in metadata_lines:
                print(line, file=sys.stderr)
        
        # Display image inline if requested or auto-detect
        if args.display is None:
            # Auto-detect: display if terminal supports it
            terminal_type = detect_terminal_graphics_support()
            if terminal_type:
                display_image_inline(output, terminal_type, reserved_lines=len(metadata_lines) + 1)
                print_metadata()
        elif args.display:
            # Explicitly requested display
            terminal_type = detect_terminal_graphics_support()
            if terminal_type:
                display_image_inline(output, terminal_type, reserved_lines=len(metadata_lines) + 1)
                print_metadata()
            else:
                print("Warning: --display requested but terminal does not support inline graphics", file=sys.stderr)
                print_metadata()

        print         (f'rm -rf {tmpdir}',file=sys.stderr)
        subprocess.run(f'rm -rf {tmpdir}', shell=True, capture_output=True)

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
