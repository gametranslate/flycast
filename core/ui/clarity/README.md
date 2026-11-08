# Clarity settings for Flycast

Clarity is a native C++/Dear ImGui settings screen, enabled by default for desktop
Windows builds. It does not replace the emulation core. The browser concept is a
visual reference; the native screen reads the current Flycast configuration.

## Building

Use the normal Flycast build instructions and initialized submodules. Add
`-DFLYCAST_CLARITY=ON` to enable the screen, or `OFF` to compile only Classic UI.
The current Windows development build uses Visual Studio 2019 x64, CMake 3.31,
and `USE_DX9=OFF`, `USE_BREAKPAD=OFF`, `USE_LUA=OFF`. DirectX 11, OpenGL and Vulkan
remain available. These flags are local build choices, not changes to defaults.

## Behavior

- English and Light are the initial Clarity preferences. Eight languages and
  Light/Night are saved globally in the `[clarity]` section of `emu.cfg`.
  They do not change Dreamcast BIOS language or existing Classic UI preferences.
- Graphics, audio, input assignments/dead zones, and game folders are staged.
  Apply commits changed fields; Undo reloads the live configuration. Closing or
  entering Classic with pending changes offers Apply, Discard, and Keep editing.
- Opening Settings does not select a preset. Original/Balanced/Sharp change only
  internal resolution to 480/960/1440 when clicked. They are not performance scores.
- A running game's existing profile is respected. Customize this game stages
  profile creation until Apply. Remove profiles in Classic Settings; close the
  game before editing global defaults. Folder paths and host controller mappings
  are shared, as in upstream. VMU display and save-state options follow game scope.
- Read-only compatibility options remain locked. Selecting another renderer
  starts with its non-OIT mode; per-pixel mode is gated on the applied context's
  capabilities. Framebuffer emulation retains the user's resolution/widescreen
  preferences, which the renderer ignores while accurate framebuffer is enabled.
- Audio buffer samples are not rounded unless the slider changes. Apply updates
  volume's logarithmic gain. Existing Done/save, Maple reconnect, VMU reset and
  DreamPicoPort/DreamPotato lifecycle behavior is shared with Classic.
- Device drafts use weak references. Disconnected devices are dropped; a newly
  connected device is read without overwriting other pending edits.
- Classic Settings retains all other options, including button mapping, Maple/VMU
  device types, networking, BIOS paths, achievements, and diagnostics. It retains
  its original immediate-edit/save behavior and upstream translation coverage.
  Clarity's eight-language coverage applies to the new screen, not every legacy
  dialog. The upstream folder picker also uses the Classic locale.

## Keeping upstream merges manageable

Keep `master` tracking upstream code and work on `codex/ui-redesign`. New code,
translations, tests, and the supplied GameTranslate image live in this directory.
Integration is limited to `core/ui/CMakeLists.txt`, `gui.cpp` (session entry),
`settings.cpp` (screen routing and shared lifecycle), and `resources.cmake`.
There are no emulation-core or existing option-default changes.

When syncing, fetch upstream and merge `upstream/master` into the UI branch.
Review these four integration points, especially upstream changes to the Done
path and renderer/settings behavior. Build both Clarity ON and OFF and run the
checks below. Isolation reduces conflicts; it cannot guarantee conflict-free
updates when upstream changes the integration APIs.

## Translations and verification

Edit `strings.json` (column order: en, vi, de, fr, es, zh_CN, zh_TW, ko), then run
`python core/ui/clarity/generate_strings.py` and commit both the JSON and generated
`strings.inc`. No Python is required to build Flycast.

`tests/draft_test.cpp` is a standalone C++17 regression test for deferred writes,
Undo, read-only values, unrelated external updates, and no-op commits.
`tests/option_scope_test.cpp` uses the real upstream Option/Settings classes with
an in-memory storage backend to check game isolation, inherited values, protected
overrides, and global language persistence. Compile with assertions enabled
(`cl /EHsc /std:c++17 /utf-8 /Icore /Icore/deps/nowide/include ...` or equivalent).

Native checks should cover English first launch, all eight languages, both themes,
Apply/Undo and reload, pending navigation, a narrow window, folder selection,
device disconnect/reconnect, per-game isolation and Classic round trips. Actual
gameplay, physical controller mapping and in-game Maple/VMU reconnection require
appropriate game content and hardware; a successful UI build is not a playtest.

For deterministic Windows visual checks, build with
`-DFLYCAST_CLARITY_SMOKE_TEST=ON`. Run that test executable from an isolated folder
with its own `emu.cfg`, DirectX 11 (`pvr.rend = 2`), initial resolution 480, and the
desired `[window]` size. Add `[qa-preserve]` with `unknown = keep-me` to the test
configuration to check preservation of unrecognized entries.
It opens Settings, renders 22 locale/theme/category cases, writes PNGs directly
from the DX11 backbuffer into the working directory and exits. It does not need
desktop capture or synthesize OS input. At widths of at least 1000 pixels it also
drives the real ImGui widgets and checks presets, theme changes with pending edits,
Undo, Apply, pending-navigation decisions, Classic round trips, and persistence.
This does not test gameplay. Turn this option OFF for the executable for users.

The unmodified `gametranslate.png` is the logo supplied by the fork owner. The
lower-left link opens `https://gametranslate.us` only when activated.
