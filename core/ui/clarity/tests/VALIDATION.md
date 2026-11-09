# Windows validation, 2026-11-08

Base: `7adbf3caef251b153e348df3f469a0f35878339d` (`gametranslate/flycast`).
Local branch: `codex/ui-redesign`; local master remains at the base above.
Toolchain: CMake 3.31.10, VS 2019 MSVC 14.29, Windows SDK 10.0.19041.
Release x64, DirectX 11/OpenGL/Vulkan and RetroAchievements enabled.
DX9, Lua, Discord and Breakpad disabled for this package.

| Check | Result |
| --- | --- |
| Expanded Clarity frontend Release build | PASS |
| `FLYCAST_CLARITY=OFF` Release build | PASS; original frontend compiles |
| Final Release with `FLYCAST_CLARITY_SMOKE_TEST=OFF` | PASS; smoke source/definitions absent from project and report/capture markers absent from executable |
| Final user-build interaction | User subsequently reported the normal Release works; this is user feedback, not a completed automated native-dialog suite |
| Real Option/Settings storage regression | PASS: globals, game A/B isolation, compatibility locks, explicit profile creation/removal, global language |
| Source option coverage | PASS: 118 upstream references accounted for; UITheme/UILanguage replaced by Clarity controls, Android SAF outside Windows scope |
| Translation table and printf placeholders | PASS: 704 entries × 8 locales; generated strings/search reproducible |
| Original logo | PASS; SHA-256 `b04253e0603917ac37cc00beb56b4fa491329d169fb9edd6fe42e217175fac71` matches supplied file |
| Native DX11 render at 1280×900 | PASS: 114 cases |
| Native DX11 render at 640×480 | PASS: same 114 cases; rendering only |
| Real ImGui widget navigation | PASS: library → Settings → Audio → mapping → About → library |
| Audio change and persistence | PASS: DSP toggle, save and reload |
| Controller mapping interaction | PASS: synthetic device, A mapping through input detection, mapping-file reload, unregister/disconnect cleanup |
| Opening settings preserves values | PASS: resolution 480, buffer 987 samples retained; unknown config entry preserved |
| Folder drop/routing | PASS: Unicode folder, deduplication, persistence, game/disc routing and busy-state guard; artificial game launch intercepted |
| Native folder dialog | Common Item Dialog opens; user-selected folders reached ContentPath and were persisted. The interactive Unicode/cancel harness has not completed; arbitrary folders intentionally fail that harness and exit. |
| Native integration fix | Shell extensions isolated from engine exception handling; no cross-process HWND ownership; main-window and gamepad input blocked during selection; helper job terminated on shutdown |
| Visual review | Light workspace, Achievements alignment, Advanced/Dump alignment, library, mapping, Graphics, network variants, pause, cheats and About sampled; French sidebar and compact icon rail corrected; English/Vietnamese/French/CJK/Korean samples inspected across development runs |
| Whitespace check | PASS: `git diff --check` |
| Physical controllers, rumble, reconnect, real gameplay and performance | NOT RUN |
| In-game Maple/VMU reconnect and save/load with real game content | NOT RUN; shared upstream lifecycle retained |
| Real network sessions and RetroAchievements authentication/game list | NOT RUN; network variants and account-page layout rendered |
| Other operating systems | See the multiplatform follow-up below |

The 114-case suite comprises library (8 locales × 2 themes), eleven settings
pages (English × 2 themes), controller mapping (8 × 2), About (8 × 2), GGPO,
Naomi and Battle Cable (each × 2), pause (× 2), cheats (× 2), Advanced bottom (× 2), and 32 extra quick-menu
frames (8 locales × 2 themes × top/bottom scroll positions, long title).
Other settings pages have eight-column translations but were not each captured
in every locale. Compact windows use scrollable content/navigation; the footer
stays separate from content. Compact runs skip synthetic widget interaction.

Evidence is in `.work/polish-final-full/` and `.work/polish-final-compact/` with
`clarity-smoke-pass.txt` and `clarity-native-00.png` through `113.png`.
The package contains representative screenshots and copies of both reports.
`.work/polish-off-build.log` and `.work/polish-release-build.log` record the
builds. Normal upstream warnings remain, including ignored MSVC `/s`.

These tests use actual Dear ImGui/DX11 rendering and input-detection code with
synthetic input, not OS screenshot automation or hardware/gameplay playtesting.
Opening external About/branding links was checked in source; no GitHub star,
login, online game connection, push or PR was performed by the tests.

The package is assembled from a clean allowlist of files, not the previous
portable directory used for gameplay. Personal saves, BIOS, mappings, folder
paths and generated caches are excluded. Source patch reconstruction and ZIP
checksums are verified by `.work/package-polished.py`.

## Startup defaults update

- Windows Clarity now starts at 1280×900 logical pixels, limited by the monitor
  work area and DPI. Only the old 640×480 default (or invalid dimensions) is
  migrated, once; other saved geometry and fullscreen/maximized state remain.
  The initialized size is saved before the marker so interrupted shutdown cannot
  restore the old default on the next launch.
- Fresh appearance is Night; saved Light preferences remain effective. Default
  keyboard mapping assigns F1 Save State, F2 Load State and F3 Next Save State
  Slot, through the existing emulator actions. Existing mapping files are kept.
- A normal Release launched from an isolated DX11 config with no window/theme
  settings; its saved config after exit recorded 1280×900 and Night=yes. The user
  interacted with this QA instance. Computer Use capture failed with
  `window capture timed out`; no screenshot or F1/F2 gameplay claim is made.
- Release rebuilt with smoke testing OFF after these changes. Review confirmed
  the keyboard scancodes match both SDL and Windows Raw Input translation.
  Low-resolution/high-DPI displays, saved-geometry migration and in-game hotkey
  behavior were reviewed in source but not exercised in this follow-up.
- The web-download ZIP is a separate, clean binary-only package containing
  `flycast.exe`, `emu.cfg` (English/Night/DX11) and `LICENSE`. Source, reports,
  screenshots and personal data are excluded; previous portable folders are
  untouched. ZIP integrity and executable identity are checked when packaging.

## Multiplatform follow-up (2026-11-09)

- Linux CI run `42588184564` passed 114 OpenGL captures at 1280x900 and 114
  at 640x480, the full-window widget/mapping/persistence suite and drop-routing
  fixtures. It rebuilt with the harness OFF and generated an AppImage.
- An isolated Ubuntu 24.04/WSL AddressSanitizer build exposed an existing font
  ownership mismatch: embedded resources allocated with `new[]` were handed
  to ImGui, whose allocator releases with `free`. Embedded font buffers now
  use ImGui's allocator. Both 114-frame runs, widget checks and normal shutdown
  pass AddressSanitizer after the fix (`detect_leaks=0` for graphics-driver
  allocations; allocator mismatch and use-after-free checks remain enabled).
- The smoke harness now exits through `dc_exit()` and the normal main-loop
  teardown, instead of bypassing Flycast cleanup with `std::exit()`.
- macOS universal compilation and actual OpenGL captures were observed. The
  runner limits its display to 1024x768, yielding a 1024x642 full-case window;
  the test now scrolls to the A binding before attempting to map it. Final
  package verification is pending a run containing the font fix.
- Android APK installation and two startup/relaunch checks passed on Android
  15 x86_64 emulation. The first input attempt showed that instantaneous ADB
  taps can be missed by the software renderer's frame polling; the integration
  script now holds touch for 350 ms. Final native-picker/mapping screenshots
  and a rebuilt APK containing the font fix are pending.
- Real gameplay, save-state hotkeys with a game, physical controllers, online
  services and completed native-folder selection on the new platforms remain
  unverified. CI render captures are not gameplay tests.
