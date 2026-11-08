"""Regenerate the native translation table. No build-time Python dependency."""
import json
from pathlib import Path

root = Path(__file__).resolve().parent
rows = json.loads((root / "strings.json").read_text(encoding="utf-8"))
assert all(len(values) == 8 and all(values) for values in rows.values())
(root / "strings.inc").write_text(
    "// Generated from strings.json by generate_strings.py.\n"
    + "".join("{" + json.dumps(key) + ", {"
              + ", ".join(json.dumps(value, ensure_ascii=False) for value in values)
              + "}},\n" for key, values in rows.items()), encoding="utf-8")
