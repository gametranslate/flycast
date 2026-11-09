# Clarity desktop frontend for Flycast

Clarity is a native C++/Dear ImGui frontend, enabled by default on desktop
Windows, Linux, macOS and Android (standalone builds, not Libretro).
The startup library, settings, controller mapping, pause menu, cheats and
achievement list share the same Light/Night appearance. On desktop Windows,
all shared file/folder pickers use the standard Windows Common Item Dialog.
Linux uses the desktop portal and macOS uses Cocoa panels via a pinned
Native File Dialog Extended dependency. A missing Linux portal falls back to
the built-in Clarity file browser. Android uses Storage Access Framework and
retains document URIs; Java selection results are applied on the UI thread.
Loading, onboarding and connection dialogs use the same theme. No web runtime
is required. Emulation option defaults are unchanged; Clarity provides its own
desktop window, appearance and keyboard shortcut defaults.

## Building

Use the normal Flycast build instructions with initialized submodules.
`-DFLYCAST_CLARITY=ON` enables the frontend; `OFF` builds the upstream frontend.
The local Windows package uses Visual Studio 2019 x64, CMake 3.31 and
`USE_DX9=OFF`, `USE_BREAKPAD=OFF`, `USE_LUA=OFF`, `USE_DISCORD=OFF`.
DirectX 11, OpenGL, Vulkan and RetroAchievements are included. These are local
build choices, not changes to repository defaults. Build-dependent controls
are shown only when their feature is available.

## Interface and settings

- English and Night are the initial preferences. English, Vietnamese, German,
  French, Spanish, Simplified Chinese, Traditional Chinese and Korean are
  available. Language and theme are global `[clarity]` options in `emu.cfg`,
  independent of the BIOS language and the retained legacy UI preferences.
- Desktop builds start with a centered 1280×900 logical-pixel window, reduced to fit
  the monitor work area at the current DPI. The old 640×480 default upgrades
  once; other saved sizes, maximized/fullscreen state and later resizes are kept.
- Default keyboard shortcuts are F1 for Save State, F2 for Load State and F3 for
  Next Save State Slot. Saved mappings and an explicitly selected Light theme
  are preserved; Reset mapping restores these shortcuts.
- The library has game search, platform filters, grid/list views, folder setup
  and direct access to Button mapping, Settings and About. Drop a supported game
  file onto the library to play, or a folder to add it. Add game folder and all
  Browse actions open the Windows dialog; there is no Open game button. Drops
  are ignored while busy or in settings. The disc-change screen accepts disc
  images and retains Choose a disc.
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
- The Tab quick menu is a centered panel with a primary Resume action, tool
  tiles, a dedicated save-state preview/slot area and a separate Close Game
  action. Its content scrolls in small windows. Save/load restrictions and
  game/disc callbacks retain upstream behavior.
- Light mode uses white content surfaces, darker text and visible control
  borders. Hardcore and dependent texture-dump options align with their parents.
- Icons accompany navigation, categories and major actions. The sidebar
  adapts to translated labels, and becomes an icon rail with tooltips in
  compact windows. Navigation and content scroll independently.
- Android settings use a labeled category selector instead of an icon-only
  sidebar, larger touch padding/scrollbars and system folder selection. The
  library guides touch users to select a folder instead of dragging files.

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
resource/build registration, SDL drop routing and a small Windows startup hook
for native dialogs. This is not a conflict-free overlay.

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
`[qa-preserve] unknown = keep-me`. The harness captures 114 actual DX11 frames
covering locale/theme variants of the library, mapping and About; all settings
pages; network variants, pause, cheats and the bottom of Advanced. Additional
quick-menu captures cover all eight locales in both themes, at top and bottom
scroll positions with a long title. Drop tests use an artificial disc fixture
and intercept launch routing; they do not boot or insert a real disc.
At widths of at least 1000 it also drives real ImGui widgets: navigation, DSP
save/reload, synthetic controller A mapping/detection/persistence/disconnect and
return to the library. Narrow runs check rendering only. It does not inject OS
input. **Turn the smoke option OFF for user builds.**

For optional interactive native-dialog testing in a smoke build, set
`[qa-preserve] native-dialog = yes` in a fresh isolated config and create
`Folder Việt Đức` beside its executable. The test opens Add game folder;
select that exact folder, then cancel the cheat-file dialog that follows.
It writes `native-dialog-pass.txt` and exits. Selecting another folder fails
this test and exits: this is test-harness behavior, not a normal user build.

Native dialogs run in a short-lived child invocation of the same executable,
before emulator initialization. This keeps Explorer extensions outside Flycast's
fail-fast exception handler. Only the result pipe is inherited, UTF-8 paths are
returned to the UI thread, and cancellation leaves options unchanged. The main
window and gamepad navigation are disabled until completion. The shell dialog
has no cross-process HWND owner, avoiding shared-input-queue/DXGI hangs. Errors
restore the main window and are displayed there; rejected selections can retry.
The helper belongs to a job that ends on parent shutdown, so no dialog process
is left behind. The engine's exception handling and emulation behavior are unchanged.

See `tests/VALIDATION.md` for the results and remaining hardware/gameplay checks.

## Multiplatform builds

`.github/workflows/clarity-platforms.yml` builds Linux x64 AppImage, macOS
universal (Intel + Apple Silicon) app ZIP, and an Android preview APK. Linux
and macOS run the opt-in OpenGL capture suite before rebuilding with the test
harness OFF for packaging. QA images/logs are separate workflow artifacts.

Linux native-picker build dependencies include dbus-1, wayland-client and
wayland-scanner. The desktop requires an xdg-desktop-portal provider for native
dialogs. macOS uses AppKit. CMake fetches NFDe v1.4.1 at commit
`65b2f070a0fcfee0d453a64e770a217ab0b6ff4f`; its license is included in packages.
Set `FETCHCONTENT_SOURCE_DIR_CLARITY_NFD` to a local copy for offline builds.

Android uses `-Pclarity=true` (default) with application ID
`us.gametranslate.flycast`, so it can coexist with upstream Flycast. Use
`-Pclarity=false` to build the original package/UI. `-PclarityPreview=true`
signs a testing APK with the existing debug keystore; it is explicitly a
preview, not a production-signed Play Store release. Normal release signing
continues to require a private release keystore. No signing keys are uploaded
by this workflow. Android physical touch/controller and SAF checks still need
an Android device; desktop CI is not proof of mobile runtime behavior.
