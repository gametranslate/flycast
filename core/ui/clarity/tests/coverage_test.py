"""Catch settings omitted by a future upstream UI update and invalid locales."""
import json, re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ui=root.parent
def read(path): return path.read_text(encoding="utf-8")
def options(text):
    return set(re.findall(r"config::([A-Z][A-Za-z0-9_]*)",text))-{"Option","Settings"}
expected=set()
for path in ui.glob("settings*.cpp"): expected |= options(read(path))
native=read(root/"settings_pages.cpp")+read(root/"controller_panel.inc")+read(root/"clarity.cpp")
shared=read(ui/"settings.cpp").split("static bool beginTabItem")[0]
shared+=read(ui/"settings_about.cpp")
shared+=read(ui/"settings_general.cpp").split("void gui_settings_achievements()")[1]
controls=read(ui/"settings_controls.cpp")
shared+=controls[controls.index("static void gamepadSettingsPopup"):controls.index("void gui_settings_controls")]
replaced={"UILanguage","UITheme"} # Clarity's eight locales and Light/Night preferences.
platform_specific={"UseSafFilePicker"} # Android-only storage API, not in the Windows build.
missing=expected-options(native+shared)-replaced-platform_specific
assert not missing, "Settings missing from Clarity: "+", ".join(sorted(missing))
rows=json.loads(read(root/"strings.json"))
fmt=re.compile(r"%(?:[-+0-9.#]*)([sdiufc])")
for key,values in rows.items():
    assert len(values)==8 and all(values), key
    for value in values: assert fmt.findall(value)==fmt.findall(values[0]), ("format",key,value)
assert rows["language"][0]=="Language"
for key in ["Source credits","Star Flycast on GitHub","Button mapping","Folders & BIOS","Map button"]:
    assert key in rows
print(f"PASS: {len(expected)} upstream option references accounted for; {len(rows)} strings x 8 locales with matching format placeholders.")
