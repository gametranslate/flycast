# Windows validation, 2026-11-08

Base: `7adbf3caef251b153e348df3f469a0f35878339d` (`gametranslate/flycast`).
Local branch: `codex/ui-redesign`.
Toolchain: CMake 3.31.10, Visual Studio 2019 MSVC 14.29, Windows SDK 10.0.19041.
Build configuration: Release x64; DX11/OpenGL/Vulkan enabled; DX9, Lua and Breakpad
disabled for this local package.

| Check | Result |
| --- | --- |
| Original upstream code builds on this toolchain | PASS |
| Clarity Windows Release build | PASS |
| Staged editing, Undo, unchanged values, late read-only lock | PASS, `draft_test.cpp` |
| Actual Option/Settings global and per-game persistence semantics | PASS, `option_scope_test.cpp` |
| Compatibility override stays protected before game profile creation | PASS |
| Game A options do not leak into game B | PASS |
| Clarity language remains global and initially English | PASS |
| Translation table | PASS, 108 entries × 8 languages, reproducible generated table |
| Supplied GameTranslate image | PASS, embedded file SHA-256 matches original |
| Native DX11 rendering | PASS, 22 captures at 1120×860 |
| Compact window | PASS, 22 captures at 640×480; vertical content scrolls |
| Native text inspection | English, Vietnamese, German, Traditional Chinese, Korean inspected; no missing glyphs observed |
| Native widgets: presets, theme change with pending edits, Undo and Apply | PASS, ImGui IO test stream |
| Pending navigation: Keep editing, Discard, Apply & continue | PASS |
| Classic Settings round trips | PASS |
| Applied resolution is persisted; unrecognized config entry survives | PASS |
| OS mouse/window capture automation | Unavailable: capture timed out; used direct renderer captures and internal widget tests |
| Physical controller disconnect, dead zone and remapping | NOT RUN; only detected keyboard enumeration was rendered |
| In-game performance, rendering and Maple/VMU reconnect | NOT RUN; no game content/hardware used |
| Other operating systems and final `FLYCAST_CLARITY=OFF` configuration | NOT RUN |

The source's normal build warnings remain (including MSVC's ignored `/s` linker
option); no errors occurred in the successful builds. The UI capture test is
opt-in and must be OFF for the packaged user executable. It exercises real
ImGui widgets and DX11 drawing, but does not replace a gameplay playtest.

The main library screen and Classic Settings retain upstream layout and
translation coverage. The new Clarity Settings surface supports all eight
requested languages. Upstream's folder picker retains the Classic locale.
