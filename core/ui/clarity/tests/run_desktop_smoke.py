"""Run opt-in OpenGL UI tests in isolated GitHub runner configuration."""
from pathlib import Path
import hashlib
import os
import platform
import shutil
import struct
import subprocess
import sys

exe = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
if platform.system() == "Darwin":
    config = Path.home() / ".flycast"
    if config.exists():
        raise RuntimeError("Refusing to replace an existing Flycast configuration")
    config.mkdir()
else:
    env["XDG_CONFIG_HOME"] = str(output / "config")
    env["XDG_DATA_HOME"] = str(output / "data")
    config = output / "config/flycast"
    config.mkdir(parents=True)

for name, width, height in [("full", 1280, 900), ("compact", 640, 480)]:
    case = output / name
    case.mkdir()
    (config / "emu.cfg").write_text(
        "[config]\npvr.rend = 0\nrend.Resolution = 480\naica.BufferSize = 987\n"
        "[clarity]\nLanguage = en\nNight = yes\nWindowSizeInitialized = yes\n"
        f"[window]\nwidth = {width}\nheight = {height}\nfullscreen = no\nmaximized = no\n"
        "[qa-preserve]\nunknown = keep-me\n", encoding="utf-8")
    command = [str(exe)]
    if platform.system() == "Linux":
        env["LIBGL_ALWAYS_SOFTWARE"] = "1"
        command = ["xvfb-run", "-a", "-s", "-screen 0 1600x1200x24"] + command
    with (case / "run.log").open("w") as log:
        result = subprocess.run(command, cwd=case, env=env, stdout=log,
                                stderr=subprocess.STDOUT, timeout=240)
    for capture in config.glob("clarity-native-*.png"):
        shutil.move(str(capture), case / capture.name)
    if (config / "clarity-smoke-fail.txt").exists():
        shutil.copy2(config / "clarity-smoke-fail.txt", case)
    if result.returncode:
        print((case / "run.log").read_text(errors="replace")[-12000:])
    assert result.returncode == 0, (name, result.returncode)
    assert (case / "clarity-smoke-pass.txt").exists(), name
    captures = list(case.glob("clarity-native-*.png"))
    assert len(captures) == 114, (name, len(captures))
    hashes = set()
    for capture in captures:
        png = capture.read_bytes()
        w, h = struct.unpack(">II", png[16:24])
        assert w >= 600 and h >= 450 and len(png) > 2000, (capture, w, h)
        hashes.add(hashlib.sha256(png).digest())
    assert len(hashes) > 30, "Captures did not change across UI states"
    print(f"PASS {name}: 114 OpenGL frames, {len(hashes)} distinct captures")

if platform.system() == "Darwin":
    # The directory was created by this script after an explicit absence check.
    shutil.move(str(config), output / "config")
