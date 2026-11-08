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
| Final user-build interaction | NOT RUN after smoke was disabled: Computer Use reported a physical Escape stop at launch; final Release compiled and contains no smoke report/capture markers |
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
| Other operating systems | NOT RUN; Windows is the current target |

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
