# Clarity desktop frontend for Flycast

Clarity is a native C++/Dear ImGui frontend, enabled by default on Windows.
The startup library, settings, controller mapping, pause menu, file/folder
picker, cheats and achievement list share the same Light/Night appearance.
Loading, onboarding and connection dialogs use the same theme. No web runtime
is required, and the emulator core and existing option defaults are unchanged.

## Building

Use the normal Flycast build instructions with initialized submodules.
`-DFLYCAST_CLARITY=ON` enables the frontend; `OFF` builds the upstream frontend.
The local Windows package uses Visual Studio 2019 x64, CMake 3.31 and
`USE_DX9=OFF`, `USE_BREAKPAD=OFF`, `USE_LUA=OFF`, `USE_DISCORD=OFF`.
DirectX 11, OpenGL, Vulkan and RetroAchievements are included. These are local
build choices, not changes to repository defaults. Build-dependent controls
are shown only when their feature is available.

## Interface and settings

- English and Light are the initial preferences. English, Vietnamese, German,
  French, Spanish, Simplified Chinese, Traditional Chinese and Korean are
  available. Language and theme are global `[clarity]` options in `emu.cfg`,
  independent of the BIOS language and the retained legacy UI preferences.
- The library has game search, platform filters, grid/list views, folder setup
  and direct access to Button mapping, Settings and About.
- Settings has eleven release pages: Console & appearance, Button mapping,
  Ports & VMU, Graphics, Audio, Library & saves, Folders & BIOS, Network,
  Achievements, Advanced and About. Diagnostics appears in debug/profiling
  builds. Search finds categories and setting labels. There is no Classic
  destination in the Clarity frontend.
- Buttons, direction/axis mappings, triggers, system controls and emulator
  shortcuts are accessible in the mapping page. It supports device/port
  selection, Dreamcast/Arcade layouts, per-game mappings, live input feedback,
  rumble/stick settings, reset confirmation and disconnect cleanup.
- Edits apply immediately, as in upstream. Save changes persists them without
  leaving; navigation and Save & return use the shared settings finish path.
  Some options take effect when restarting the game or renderer. This replaces
  the former four-page staged Apply/Undo implementation.
- Opening settings does not choose a preset or round audio buffer samples.
  Read-only compatibility options remain disabled. Per-game profiles use the
  original Option/Settings storage; creation is explicit and deletion requires
  confirmation. Host input mappings and folders retain upstream scope.
- Saving uses the shared audio gain, UI scaling, Maple reconnect, VMU reset
  and DreamPotato lifecycle. Controller mapping files use upstream input
  detection and persistence rather than a separate mapping format.
- Icons accompany navigation, categories and major actions. The sidebar
  adapts to translated labels, and becomes an icon rail with tooltips in
  compact windows. Navigation and content scroll independently.

About credits flyinghead and Flycast contributors, links to the original
repository, author, license, reicast and dependencies, and invites the user
to star **flyinghead/flycast**. Clarity/GameTranslate contributions are credited
separately. Links only open when activated; no star is submitted automatically.
The supplied `gametranslate.png` is embedded unchanged in the lower-left
branding, linking to https://gametranslate.us.

## Keeping upstream updates manageable

Keep the UI work on `codex/ui-redesign` and keep `master` separate. Most new code
lives here, but integration also touches `core/ui` routing, shared widgets,
settings lifecycle, mapping, cheats, achievements, `core/oslib/i18n.cpp` and
resource/build registration. This is not a conflict-free overlay.

For an upstream update, fetch upstream and merge `upstream/master` into a
temporary integration branch based on the UI branch. Review changed UI APIs,
settings and input lifecycle, translation/font behavior and build registration.
Resolve changes, build Clarity ON and OFF, run the coverage/scope/native checks
below, then merge the tested integration back into the UI branch. Keep backend
changes separate from presentation changes to make conflicts easier to review.

## Translations and validation

`strings.json` contains eight-column native translations, ordered
en, vi, de, fr, es, zh_CN, zh_TW, ko. New authored strings also live in
`translations.tsv` (pipe-delimited, `\n` for embedded newlines).
Run `python core/ui/clarity/generate_catalogs.py` to merge these strings,
generate English aliases for shared dialogs and regenerate the search index
and `strings.inc`. Commit generated files. No Python is required to build.
Shared upstream dialogs consult the same translations. Backend-provided device
names and service/game content retain their original text.

`python core/ui/clarity/tests/coverage_test.py` detects missing option references,
checks all eight columns and compares printf placeholders. It is a source
coverage check, not proof that every setting was exercised in a game.
`tests/option_scope_test.cpp` tests the actual upstream Option/Settings classes
with in-memory storage: global/game isolation, protected overrides, profile
removal and global language persistence. Compile it as C++17 with assertions
enabled, include paths `core` and `core/deps/nowide/include`.

For deterministic Windows rendering checks, enable
`-DFLYCAST_CLARITY_SMOKE_TEST=ON`. Run the executable in an isolated folder with
its own `emu.cfg`, `pvr.rend = 2`, `rend.Resolution = 480`,
`aica.BufferSize = 987`, a `[window]` width/height and
`[qa-preserve] unknown = keep-me`. The harness captures 82 actual DX11 frames
covering locale/theme variants of the library, mapping and About; all settings
pages; network variants, pause, cheats and the shared picker. It creates a text
fixture named `000-picker-fixture.elf` solely to verify unfiltered file listing.
At widths of at least 1000 it also drives real ImGui widgets: navigation, DSP
save/reload, synthetic controller A mapping/detection/persistence/disconnect and
return to the library. Narrow runs check rendering only. It does not inject OS
input. **Turn the smoke option OFF for user builds.**

See `tests/VALIDATION.md` for the results and remaining hardware/gameplay checks.
