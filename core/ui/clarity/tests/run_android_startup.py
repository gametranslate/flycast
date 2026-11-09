"""Launch a clean preview APK on the disposable CI Android emulator."""
from pathlib import Path
import subprocess
import time

output = Path('android-evidence')
output.mkdir(exist_ok=True)
package = 'us.gametranslate.flycast'

def adb(*args, binary=False):
    result = subprocess.run(['adb', *args], check=True, capture_output=True, timeout=60)
    return result.stdout if binary else result.stdout.decode(errors='replace')

def capture(name):
    png = adb('exec-out', 'screencap', '-p', binary=True)
    assert png.startswith(b'\x89PNG') and len(png) > 5000
    (output / name).write_bytes(png)

adb('install', '-g', 'apk/Flycast-Clarity-Android-preview.apk')
adb('logcat', '-c')
for index in range(2):
    adb('shell', 'am', 'start', '-W', '-n', package + '/com.flycast.emulator.NativeGLActivity')
    for _ in range(15):
        time.sleep(1)
        assert adb('shell', 'pidof', package).strip(), 'App process exited'
    capture('startup.png' if index == 0 else 'relaunch.png')
    activity = adb('shell', 'dumpsys', 'activity', 'activities')
    (output / f'activity-{index}.txt').write_text(activity)
    assert any(package in line for line in activity.splitlines() if 'mResumedActivity' in line or 'topResumedActivity' in line), 'Flycast is not foreground'
    adb('shell', 'am', 'force-stop', package)

log = adb('logcat', '-d')
(output / 'logcat.txt').write_text(log)
assert 'FATAL EXCEPTION' not in log and 'Fatal signal' not in log, 'Crash recorded'
(output / 'PASS.txt').write_text('PASS: APK installed, app stayed alive and foreground for two 15-second launches, screenshots captured, no fatal crash in logcat. No game, touch navigation, controller, or SAF selection was tested.\n')
