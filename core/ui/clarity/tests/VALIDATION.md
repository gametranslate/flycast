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
| Isolated user-build launch | PASS; stayed running with package config; hidden test process then terminated by harness, so this was not a graceful-close test |
| Real Option/Settings storage regression | PASS: globals, game A/B isolation, compatibility locks, explicit profile creation/removal, global language |
| Source option coverage | PASS: 118 upstream references accounted for; UITheme/UILanguage replaced by Clarity controls, Android SAF outside Windows scope |
| Translation table and printf placeholders | PASS: 693 entries × 8 locales; generated strings/search reproducible |
| Original logo | PASS; SHA-256 `b04253e0603917ac37cc00beb56b4fa491329d169fb9edd6fe42e217175fac71` matches supplied file |
| Native DX11 render at 1280×900 | PASS: 82 cases |
| Native DX11 render at 640×480 | PASS: same 82 cases; rendering only |
| Real ImGui widget navigation | PASS: library → Settings → Audio → mapping → About → library |
| Audio change and persistence | PASS: DSP toggle, save and reload |
| Controller mapping interaction | PASS: synthetic device, A mapping through input detection, mapping-file reload, unregister/disconnect cleanup |
| Opening settings preserves values | PASS: resolution 480, buffer 987 samples retained; unknown config entry preserved |
| Picker without extension filter | PASS: text fixture `000-picker-fixture.elf` appears in actual file list in both themes/sizes |
| Visual review | Library, mapping, Graphics, network variants, pause, cheats, picker and About sampled; French sidebar and compact icon rail corrected; English/Vietnamese/French/CJK/Korean samples inspected across development runs |
| Whitespace check | PASS: `git diff --check` |
| Physical controllers, rumble, reconnect, real gameplay and performance | NOT RUN |
| In-game Maple/VMU reconnect and save/load with real game content | NOT RUN; shared upstream lifecycle retained |
| Real network sessions and RetroAchievements authentication/game list | NOT RUN; network variants and account-page layout rendered |
| Other operating systems | NOT RUN; Windows is the current target |

The 82-case suite comprises library (8 locales × 2 themes), eleven settings
pages (English × 2 themes), controller mapping (8 × 2), About (8 × 2), GGPO,
Naomi and Battle Cable (each × 2), pause (× 2), cheats (× 2) and picker (× 2).
Other settings pages have eight-column translations but were not each captured
in every locale. Compact windows use scrollable content/navigation; the footer
stays separate from content. Compact runs skip synthetic widget interaction.

Evidence is in `.work/full-icons/` and `.work/compact-icons/` with
`clarity-smoke-pass.txt` and `clarity-native-00.png` through `81.png`.
The package contains representative screenshots and copies of both reports.
`.work/clarity-off-build.log` and `.work/full-release-build.log` record the
builds. Normal upstream warnings remain, including ignored MSVC `/s`.

These tests use actual Dear ImGui/DX11 rendering and input-detection code with
synthetic input, not OS screenshot automation or hardware/gameplay playtesting.
Opening external About/branding links was checked in source; no GitHub star,
login, online game connection, push or PR was performed by the tests.
