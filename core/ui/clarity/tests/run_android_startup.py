"""Launch a clean preview APK on the disposable CI Android emulator."""
from pathlib import Path
import subprocess
import time
import re
import struct
import os
import sys
import xml.etree.ElementTree as ET

output = Path('android-evidence')
output.mkdir(exist_ok=True)
package = 'us.gametranslate.flycast'
capture_only = '--capture-only' in sys.argv

def adb(*args, binary=False):
    result = subprocess.run(['adb', *args], check=True, capture_output=True, timeout=60)
    return result.stdout if binary else result.stdout.decode(errors='replace')

def capture(name):
    png = adb('exec-out', 'screencap', '-p', binary=True)
    assert png.startswith(b'\x89PNG') and len(png) > 5000
    (output / name).write_bytes(png)
    return struct.unpack('>II', png[16:24])

sdk = Path(os.environ.get('ANDROID_HOME') or os.environ['ANDROID_SDK_ROOT'])
signer = sorted((sdk / 'build-tools').glob('*/apksigner'))[-1]
signature = subprocess.run([str(signer), 'verify', '--verbose', '--print-certs',
                            'apk/Flycast-Clarity-Android-preview.apk'],
                           check=True, capture_output=True, text=True, timeout=60)
(output / 'signature.txt').write_text(signature.stdout)
adb('install', '-g', 'apk/Flycast-Clarity-Android-preview.apk')
(output / 'package.txt').write_text(adb('shell', 'dumpsys', 'package', package))
adb('logcat', '-c')
try:
    for index in range(2):
        adb('shell', 'am', 'start', '-W', '-n', package + '/com.flycast.emulator.NativeGLActivity')
        for _ in range(15):
            time.sleep(1)
            assert adb('shell', 'pidof', package).strip(), 'App process exited'
        # A new Android user sees a system fullscreen tutorial over the app.
        # Dismiss only its observed accessibility node, not a guessed coordinate.
        adb('shell', 'uiautomator', 'dump', '/sdcard/clarity-window.xml')
        window = adb('exec-out', 'cat', '/sdcard/clarity-window.xml')
        (output / f'window-{index}.xml').write_text(window)
        for node in ET.fromstring(window).iter('node'):
            if node.get('text') == 'Got it' and node.get('package') in ('android', 'com.android.systemui'):
                x1, y1, x2, y2 = map(int, re.findall(r'\d+', node.get('bounds')))
                adb('shell', 'input', 'tap', str((x1+x2)//2), str((y1+y2)//2))
                time.sleep(1)
        size = capture('startup.png' if index == 0 else 'relaunch.png')
        # These controls were visually located in the Pixel 2 CI capture.
        assert size == (1920, 1080), 'Review UI coordinates for this display first'
        if capture_only:
            pass
        elif index == 0:
            # Keep down/up apart for the software renderer's frame polling.
            adb('shell', 'input', 'swipe', '225', '390', '225', '390', '350')  # Add game folder
            time.sleep(3)
            capture('folder-dialog.png')
            picker = adb('shell', 'dumpsys', 'activity', 'activities')
            (output / 'folder-activity.txt').write_text(picker)
            assert any('documentsui' in line for line in picker.splitlines() if 'ResumedActivity' in line), 'System document picker did not open'
            adb('shell', 'input', 'keyevent', 'KEYCODE_BACK')
            time.sleep(2)
            capture('folder-cancelled.png')
        else:
            adb('shell', 'input', 'swipe', '225', '572', '225', '572', '350')  # Button mapping
            time.sleep(2)
            capture('button-mapping.png')
            # The ImGui content uses its visible scrollbar, not Android's
            # native swipe-to-scroll gesture. Coordinates are from the capture.
            adb('shell', 'input', 'swipe', '1860', '410', '1860', '560', '600')
            time.sleep(2)
            capture('button-mapping-scrolled.png')
            assert (output / 'button-mapping.png').read_bytes() != (output / 'button-mapping-scrolled.png').read_bytes(), 'Mapping scrollbar did not change the capture'
        activity = adb('shell', 'dumpsys', 'activity', 'activities')
        (output / f'activity-{index}.txt').write_text(activity)
        assert any(package in line for line in activity.splitlines() if 'mResumedActivity' in line or 'topResumedActivity' in line), 'Flycast is not foreground'
        adb('shell', 'am', 'force-stop', package)
finally:
    log = adb('logcat', '-d')
    (output / 'logcat.txt').write_text(log)
assert 'FATAL EXCEPTION' not in log and 'Fatal signal' not in log, 'Crash recorded'
if capture_only:
    summary = 'Signature verified; APK installed; two launches captured without fatal crash. Controls NOT TESTED: review screenshots before choosing coordinates.'
else:
    summary = 'Signature verified; APK installed; two launches stayed alive; native DocumentsUI folder picker opened and cancelled; mapping navigation tapped and captured; no fatal crash in logcat.'
(output / 'PASS.txt').write_text('PASS: ' + summary + ' Screenshots require visual review. No game, physical controller, or completed SAF folder selection was tested.\n')
