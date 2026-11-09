// SPDX-License-Identifier: GPL-2.0-or-later
#include "clarity.h"
#include "ui/settings.h"
#include "ui/gui.h"
#include "ui/mainui.h"
#include "ui/IconsFontAwesome6.h"
#include "wsi/context.h"
#include "audio/audiostream.h"
#include "network/ice.h"
#include "imgui_stdlib.h"
#include "version.h"
#include <algorithm>
#include <initializer_list>
#include <functional>

namespace clarity {
namespace {
template<typename T> struct Choice { T value; const char* text; };
template<class Option>
void choice(const char* label, Option& option, std::initializer_list<Choice<std::decay_t<decltype(option.get())>>> choices, const char* help = nullptr, bool disabled = false) {
    const char* preview = "?";
    for (auto c : choices) if (option.get() == c.value) preview = tr(c.text);
    beginRow(tr(label), tr(help), disabled || option.isReadOnly());
    if (ImGui::BeginCombo("##value", preview)) {
        for (auto c : choices) {
            if (ImGui::Selectable(tr(c.text), option.get() == c.value)) option = c.value;
            if (option.get() == c.value) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    endRow();
}
template<class Option>
void textOption(const char* label, Option& option, const char* help = nullptr, ImGuiInputTextFlags flags = 0) {
    beginRow(tr(label), tr(help), option.isReadOnly());
    ImGui::InputText("##value", &option.get(), flags);
    endRow();
}
RenderType base(RenderType r) {
    switch (r) {
    case RenderType::OpenGL_OIT: return RenderType::OpenGL;
    case RenderType::Vulkan_OIT: return RenderType::Vulkan;
    case RenderType::DirectX11_OIT: return RenderType::DirectX11;
    default: return r;
    }
}
RenderType oit(RenderType r) {
    switch (base(r)) {
    case RenderType::OpenGL: return RenderType::OpenGL_OIT;
    case RenderType::Vulkan: return RenderType::Vulkan_OIT;
    case RenderType::DirectX11: return RenderType::DirectX11_OIT;
    default: return r;
    }
}
const char* rendererName(RenderType r) {
    switch (base(r)) {
    case RenderType::OpenGL: return "OpenGL";
    case RenderType::Vulkan: return "Vulkan";
    case RenderType::DirectX9: return "DirectX 9";
    case RenderType::DirectX11: return "DirectX 11";
    default: return "?";
    }
}
std::function<void(const std::string&)> folderSelected;
void folderPicker() {
    select_file_popup("Select folder###clarity-folder", [](bool cancelled, std::string path) {
        if (!cancelled && folderSelected) folderSelected(path);
        folderSelected = {};
        return true;
    });
}
bool folderRequested = false;
template<class Option>
void pathList(const char* label, Option& option, const char* help, bool content = false) {
    ImGui::PushID(label);
    ImGui::BeginChild("paths", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    ImGui::TextColored(accent(), ICON_FA_FOLDER_OPEN);
    ImGui::SameLine(); ImGui::TextWrapped("%s", tr(label));
    muted(tr(help));
    ImGui::BeginDisabled(option.isReadOnly());
    auto& paths = option.get();
    int remove = -1;
    for (size_t i = 0; i < paths.size(); ++i) {
        ImGui::PushID((int)i);
        if (ImGui::Button(ICON_FA_TRASH_CAN)) remove = (int)i;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Remove"));
        ImGui::SameLine(); ImGui::TextWrapped("%s", paths[i].c_str());
        ImGui::PopID();
    }
    if (remove >= 0) {
        if (content) scanner.stop();
        paths.erase(paths.begin() + remove);
        if (content) scanner.refresh();
    }
    if (button("Browse...")) {
        folderSelected = [&option, content](const std::string& path) {
            if (option.isReadOnly()) return;
            auto& paths = option.get();
            if (std::find(paths.begin(), paths.end(), path) != paths.end()) return;
            if (content) scanner.stop();
            paths.push_back(path);
            if (content) scanner.refresh();
        };
        folderRequested = true;
    }
    ImGui::EndDisabled();
    if (paths.empty()) muted(tr("Using the default folder."));
    ImGui::EndChild();
    ImGui::PopID();
}
template<class Option>
void singlePath(const char* label, Option& option, const char* help) {
    ImGui::PushID(label);
    ImGui::BeginChild("path", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    ImGui::TextColored(accent(), ICON_FA_FOLDER_OPEN);
    ImGui::SameLine(); ImGui::TextWrapped("%s", tr(label));
    muted(tr(help));
    ImGui::BeginDisabled(option.isReadOnly());
    if (option.get().empty()) muted(tr("Using the default folder."));
    else ImGui::TextWrapped("%s", option.get().c_str());
    if (button("Browse...")) {
        folderSelected = [&option](const std::string& path) { if (!option.isReadOnly()) option = path; };
        folderRequested = true;
    }
    ImGui::SameLine();
    if (button("Use Default")) option.get().clear();
    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::PopID();
}
void finishFolders() {
    // Open on this stack, after leaving the card that requested the picker.
    if (folderRequested) { ImGui::OpenPopup("Select folder###clarity-folder"); folderRequested = false; }
    folderPicker();
}
}

void generalPage() {
#ifdef __ANDROID__
    OptionCheckbox(tr("Use SAF File Picker"), config::UseSafFilePicker);
#endif
    section(tr("Appearance"));
    if (OptionSlider(tr("UI Scaling"), config::UIScaling, 50, 200, tr("Adjust the size of UI elements and fonts."), "%d%%"))
        uiUserScaleUpdated = true;
    if (uiUserScaleUpdated) muted(tr("The new scale is applied when you save."));
    muted(tr("Choose your language and Light or Night mode in the top bar."));
    section("Console");
    {
        DisabledScope scope(settings.platform.isArcade());
        const char* languages[] = {tr("Japanese"), tr("English"), tr("German"), tr("French"), tr("Spanish"), tr("Italian"), tr("Default")};
        OptionComboBox(tr("Dreamcast Language"), config::Language, languages, std::size(languages), tr("The language as configured in the Dreamcast BIOS"));
        const char* broadcast[] = {"NTSC", "PAL", "PAL/M", "PAL/N", tr("Default")};
        OptionComboBox(tr("Broadcast"), config::Broadcast, broadcast, std::size(broadcast), tr("TV broadcasting standard for non-VGA modes"));
    }
    const char* consoleRegion[] = {tr("Japan"), tr("USA"), tr("Europe"), tr("Default")};
    const char* arcadeRegion[] = {tr("Japan"), tr("USA"), tr("Export"), tr("Korea")};
    OptionComboBox(tr("Region"), config::Region, settings.platform.isArcade() ? arcadeRegion : consoleRegion, 4, tr("BIOS region"));
    choice("Cable", config::Cable, {{0, "VGA"}, {2, "RGB Component"}, {3, "TV Composite"}}, "Video connection type", settings.platform.isArcade());
    OptionCheckbox(tr("Naomi Free Play"), config::ForceFreePlay, tr("Configure Naomi games in Free Play mode."));
#if USE_DISCORD
    OptionCheckbox(tr("Discord Presence"), config::DiscordPresence, tr("Show which game you are playing on Discord"));
#endif
}

void graphicsPage() {
    section("Picture quality");
    beginRow(tr("resolution"), tr("resolutionHelp"), config::RenderResolution.isReadOnly() || config::EmulateFramebuffer);
    std::string selected = std::to_string(config::RenderResolution.get()) + "p";
    if (ImGui::BeginCombo("##resolution", selected.c_str())) {
        for (int value : {240,480,720,960,1200,1440,1920,2160,2400,2880,3360,3840,4320}) {
            std::string label = std::to_string(value) + "p";
            if (ImGui::Selectable(label.c_str(), config::RenderResolution == value)) config::RenderResolution = value;
        }
        ImGui::EndCombo();
    }
    endRow();
    OptionCheckbox(tr("Integer Scaling"), config::IntegerScale, tr("Scales the output by the maximum integer multiple allowed by the display resolution."));
    OptionCheckbox(tr("Linear Interpolation"), config::LinearInterpolation, tr("Scales the output with linear interpolation. Will use nearest neighbor interpolation otherwise. Disable with integer scaling."));
    OptionCheckbox(tr("VSync"), config::VSync, tr("Synchronizes the frame rate with the screen refresh rate. Recommended"));
#ifdef __ANDROID__
    { DisabledScope scope(!config::VSync); OptionCheckbox(tr("Frame Pacing (Experimental)"), config::FramePacing); }
#else
    if (isVulkan(config::RendererType)) {
        DisabledScope scope(!config::VSync);
        OptionCheckbox(tr("Duplicate frames"), config::DupeFrames, tr("Duplicate frames on high refresh rate monitors (120 Hz and higher)"));
    }
#endif
    section("Aspect Ratio");
    {
        DisabledScope scope(config::EmulateFramebuffer);
        OptionCheckbox(tr("Widescreen"), config::Widescreen, tr("Draw geometry outside of the normal 4:3 aspect ratio. May produce graphical glitches in the revealed areas.\nAspect Fit and shows the full 16:9 content."));
        { DisabledScope scope(!config::Widescreen || config::IntegerScale);
          OptionCheckbox(tr("Super Widescreen"), config::SuperWidescreen, tr("Use the full width of the screen or window when its aspect ratio is greater than 16:9.\nAspect Fill and remove black bars. Not compatible with integer scaling.")); }
    }
    OptionCheckbox(tr("Widescreen Game Cheats"), config::WidescreenGameHacks, tr("Modify the game so that it displays in 16:9 anamorphic format and use horizontal screen stretching. Only some games are supported."));
    OptionSlider(tr("Horizontal Stretching"), config::ScreenStretching, 100, 250, tr("Stretch the screen horizontally"), "%d%%");
    OptionCheckbox(tr("Rotate Screen 90°"), config::Rotate90, tr("Rotate the screen 90° counterclockwise"));

    section("Graphics API");
    RenderType previousApi = base(config::RendererType);
    beginRow(tr("renderer"), tr("rendererHelp"), config::RendererType.isReadOnly());
    if (ImGui::BeginCombo("##api", rendererName(config::RendererType))) {
        const RenderType apis[] = {
#ifdef USE_OPENGL
            RenderType::OpenGL,
#endif
#ifdef USE_VULKAN
            RenderType::Vulkan,
#endif
#ifdef USE_DX11
            RenderType::DirectX11,
#endif
#ifdef USE_DX9
            RenderType::DirectX9,
#endif
        };
        for (auto api : apis) {
            ImGui::BeginDisabled(api == RenderType::DirectX9 && settings.platform.isNaomi2());
            if (ImGui::Selectable(rendererName(api), base(config::RendererType) == api) && base(config::RendererType) != api)
                config::RendererType = api;
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    endRow();
    beginRow(tr("Transparent Sorting"), tr("sortingHelp"), config::RendererType.isReadOnly() || config::PerStripSorting.isReadOnly());
    bool pixel = base(config::RendererType) != config::RendererType;
    const char* sorting[] = {tr("Per Triangle"), tr("Per Strip"), tr("Per Pixel")};
    int sort = pixel ? 2 : config::PerStripSorting ? 1 : 0;
    if (ImGui::BeginCombo("##sort", sorting[sort])) {
        for (int i = 0; i < 3; ++i) {
            DisabledScope lock(i == 2 && (previousApi != base(config::RendererType) || base(config::RendererType) == RenderType::DirectX9 || !GraphicsContext::Instance()->hasPerPixel()));
            if (ImGui::Selectable(sorting[i], sort == i)) {
                config::RendererType = i == 2 ? oit(config::RendererType) : base(config::RendererType);
                if (i != 2) config::PerStripSorting = i == 1;
            }
        }
        ImGui::EndCombo();
    }
    endRow();
    if (pixel) {
        choice("Pixel Buffer Size", config::PixelBufferSize,
            {{int64_t(512_MB), "512 MB"}, {int64_t(1_GB), "1 GB"}, {int64_t(2_GB), "2 GB"}, {int64_t(4_GB), "4 GB"}},
            "The size of the pixel buffer. May need to be increased when upscaling by a large factor.");
        OptionSlider(tr("Maximum Layers"), config::PerPixelLayers, 8, 128, tr("Maximum number of transparent layers. May need to be increased for some complex scenes. Decreasing it may improve performance."));
    }
    section("Textures & effects");
    choice("Anisotropic Filtering", config::AnisotropicFiltering, {{1, "Disabled"}, {2, "2x"}, {4, "4x"}, {8, "8x"}, {16, "16x"}});
    choice("Texture Filtering", config::TextureFiltering, {{0, "Default"}, {1, "Force Nearest-Neighbor"}, {2, "Force Linear"}});
    { DisabledScope lock(game_started);
      OptionCheckbox(tr("Load Custom Textures"), config::CustomTextures, tr("Load custom/high-res textures from data/textures/<game id>"));
      DisabledScope noTextures(!config::CustomTextures);
      OptionCheckbox(tr("Preload Custom Textures"), config::PreloadCustomTextures, tr("Preload custom textures at game start. May improve performance but increases memory usage")); }
    OptionCheckbox(tr("Shadows"), config::ModifierVolumes, tr("Enable modifier volumes, usually used for shadows"));
    OptionCheckbox(tr("Fog"), config::Fog, tr("Enable fog effects"));
    section("Performance & compatibility");
    choice("Automatic Frame Skipping", config::AutoSkipFrame, {{0,"Disabled"}, {1,"Normal"}, {2,"Maximum"}});
    OptionSlider(tr("Frame Skipping"), config::SkipFrame, 0, 6, tr("Number of frames to skip between two actually rendered frames"));
    OptionCheckbox(tr("Full Framebuffer Emulation"), config::EmulateFramebuffer, tr("Fully accurate VRAM framebuffer emulation. Helps games that directly access the framebuffer for special effects. Very slow and incompatible with upscaling and wide screen."));
    OptionCheckbox(tr("Delay Frame Swapping"), config::DelayFrameSwapping, tr("Useful to avoid flashing screen or glitchy videos. Not recommended on slow platforms"));
    OptionCheckbox(tr("Fix Upscale Bleeding Edge"), config::FixUpscaleBleedingEdge, tr("Helps with texture bleeding case when upscaling. Disabling it can help if pixels are warping when upscaling in 2D games (MVC2, CVS, KOF, etc.)"));
    OptionCheckbox(tr("Native Depth Interpolation"), config::NativeDepthInterpolation, tr("Helps with texture corruption and depth issues on AMD GPUs. Can also help Intel GPUs in some cases."));
    OptionCheckbox(tr("Copy Rendered Textures to VRAM"), config::RenderToTextureBuffer, tr("Copy rendered-to textures back to VRAM. Slower but accurate"));
    OptionCheckbox(tr("Show FPS Counter"), config::ShowFPS, tr("Show on-screen frame/sec counter"));
    OptionCheckbox(tr("Show VMU In-game"), config::FloatVMUs, tr("Show the VMU LCD screens while in-game"));
#ifdef VIDEO_ROUTING
    section("Video routing");
    { DisabledScope scope(base(config::RendererType) != RenderType::OpenGL && base(config::RendererType) != RenderType::DirectX11);
      OptionCheckbox(tr("Send video content to another program"), config::VideoRouting, tr("e.g. Route GPU texture to OBS Studio directly instead of using CPU intensive Display/Window Capture"));
      DisabledScope scope2(!config::VideoRouting);
      OptionCheckbox(tr("Scale down before sending"), config::VideoRoutingScale, tr("Could increase performance when sharing a smaller texture, YMMV"));
      DisabledScope scope3(!config::VideoRoutingScale);
      beginRow(tr("Output vertical resolution"));
      int res = config::VideoRoutingVRes;
      if (ImGui::InputInt("##height", &res) && res > 0) config::VideoRoutingVRes = res;
      endRow();
      ImGui::Text(tr("Output texture size: %d x %d"), config::VideoRoutingScale ? config::VideoRoutingVRes * settings.display.width / std::max(settings.display.height, 1) : settings.display.width,
          config::VideoRoutingScale ? config::VideoRoutingVRes.get() : settings.display.height);
    }
#endif
}

void audioPage() {
    section("Sound");
    if (OptionSlider(tr("Volume Level"), config::AudioVolume, 0, 100, tr("Adjust the emulator's audio level"), "%d%%"))
        config::AudioVolume.calcDbPower();
    OptionCheckbox(tr("Enable DSP"), config::DSPEnabled, tr("Enable the Dreamcast Digital Sound Processor. Only recommended on fast platforms"));
    OptionCheckbox(tr("Enable VMU Sounds"), config::VmuSound, tr("Play VMU beeps when enabled."));
    section("Audio Driver");
    beginRow(tr("Audio Driver"), tr("The audio driver to use"), config::AudioBackend.isReadOnly());
    if (ImGui::BeginCombo("##backend", config::AudioBackend.get().c_str())) {
        if (ImGui::Selectable(tr("auto - Automatic driver selection"), config::AudioBackend.get() == "auto")) config::AudioBackend = "auto";
        for (u32 i = 0; i < AudioBackend::getCount(); ++i) {
            auto backend = AudioBackend::getBackend(i);
            if (ImGui::Selectable((backend->slug + " - " + backend->getName()).c_str(), config::AudioBackend.get() == backend->slug)) config::AudioBackend = backend->slug;
        }
        ImGui::EndCombo();
    }
    endRow();
#ifdef __ANDROID__
    if (config::AudioBackend.get() == "auto" || config::AudioBackend.get() == "android")
        OptionCheckbox(tr("Automatic Latency"), config::AutoLatency);
#endif
    if (!config::AutoLatency || (config::AudioBackend.get() != "auto" && config::AudioBackend.get() != "android")) {
        beginRow(tr("Latency"), tr("Sets the maximum audio latency. Not supported by all audio drivers."), config::AudioBufferSize.isReadOnly());
        int ms = (int)std::round(config::AudioBufferSize * 1000.f / 44100.f);
        if (ImGui::SliderInt("##latency", &ms, 12, 512, "%d ms")) config::AudioBufferSize = (int)std::round(ms * 44100.f / 1000.f);
        endRow();
    }
    auto backend = AudioBackend::getBackend(config::AudioBackend);
    if (backend) {
        int count = 0;
        const auto* options = backend->getOptions(&count);
        for (int i = 0; i < count; ++i) {
            const auto& opt = options[i];
            std::string value = config::loadStr(backend->slug, opt.name);
            beginRow(opt.caption.c_str());
            if (opt.type == AudioBackend::Option::integer) {
                int n = opt.minValue;
                try { n = std::stoi(value); } catch (...) {}
                if (ImGui::SliderInt("##value", &n, opt.minValue, opt.maxValue)) config::saveStr(backend->slug, opt.name, std::to_string(n));
            } else if (opt.type == AudioBackend::Option::checkbox) {
                bool checked = value == "1";
                if (ImGui::Checkbox("##value", &checked)) config::saveStr(backend->slug, opt.name, checked ? "1" : "0");
            } else if (opt.type == AudioBackend::Option::list && ImGui::BeginCombo("##value", value.c_str())) {
                for (const auto& entry : opt.values)
                    if (ImGui::Selectable(entry.c_str(), entry == value)) config::saveStr(backend->slug, opt.name, entry);
                ImGui::EndCombo();
            }
            endRow();
        }
    }
}

void libraryPage() {
    section("Game library");
    pathList("Content Location", config::ContentPath, "The folders where your games are stored", true);
    if (button("Rescan Content")) scanner.refresh();
    OptionCheckbox(tr("Box Art Game List"), config::BoxartDisplayMode, tr("Display game cover art in the game list."));
    OptionCheckbox(tr("Fetch Box Art"), config::FetchBoxart, tr("Fetch cover images from TheGamesDB.net."));
    if (OptionCheckbox(tr("Hide Legacy Naomi Roms"), config::HideLegacyNaomiRoms, tr("Hide .bin, .dat and .lst files from the content browser"))) scanner.refresh();
    section("Automatic save states");
    OptionCheckbox(tr("Auto-save on exit"), config::AutoSaveState, tr("Save the state of the game when stopping"));
    OptionCheckbox(tr("Auto-load on launch"), config::AutoLoadState, tr("Load the last saved state of the game when starting"));
    beginRow(tr("Save state slot"), tr("Select the slot used by Save State and Load State."), config::SavestateSlot.isReadOnly());
    std::string slot = std::to_string(config::SavestateSlot.get() + 1);
    if (ImGui::BeginCombo("##slot", slot.c_str())) {
        for (int i=0; i<10; ++i)
            if (ImGui::Selectable(std::to_string(i+1).c_str(), config::SavestateSlot == i)) config::SavestateSlot = i;
        ImGui::EndCombo();
    }
    endRow();
    finishFolders();
}
void pathsPage() {
    section("Folders & BIOS");
    beginRow(tr("Home Folder"), tr("The folder where Flycast saves configuration files and VMUs. BIOS files should be in a subfolder named \"data\""));
    ImGui::TextWrapped("%s", get_writable_config_path("").c_str()); endRow();
    pathList("Content Location", config::ContentPath, "The folders where your games are stored", true);
    pathList("BIOS Folders", config::BiosPath, "Folders containing BIOS files (e.g. dc_boot.bin or dc_bios.bin) and arcade BIOS");
    singlePath("VMU Folder", config::VMUPath, "Folder where VMU (.bin) saves are stored");
    pathList("Savestate Folders", config::SavestatePath, "Folders for save states. First path is used for new states; all are searched when loading");
    singlePath("Game Save Folder", config::SavePath, "Folder for game save data (e.g. arcade NVRAM)");
    pathList("Texture Pack Folders", config::TexturePath, "Folders containing textures/<gameId> or <gameId> under a textures subfolder");
    singlePath("Texture Dump Folder", config::TextureDumpPath, "Folder where texture dumps are saved. Game-specific subfolders will be created automatically");
    singlePath("Box Art Folder", config::BoxartPath, "Folder containing box art images (png/jpg). If empty, Flycast will use the default Home Folder/boxart for downloads and generated art");
    pathList("Controller Mapping Folders", config::MappingsPath, "Folders containing controller mapping files (.cfg). The emulator also looks in Home Folder/mappings. Per-game mappings are suffixed with _<gameId>.cfg");
    pathList("Cheat Folders", config::CheatPath, "Folders containing cheat files (.cht/.txt) named with the game ID. Flycast will auto-load matching files if present");
    finishFolders();
}

void networkPage() {
    section("Network Type");
    int type = config::GGPOEnable ? 1 : config::NetworkEnable ? 2 : config::BattleCableEnable ? 3 : 0;
    const char* types[] = {tr("Native"), "GGPO", "Naomi", tr("Battle Cable")};
    beginRow(tr("Network Type"), nullptr, game_started || config::GGPOEnable.isReadOnly() || config::NetworkEnable.isReadOnly() || config::BattleCableEnable.isReadOnly());
    if (ImGui::BeginCombo("##network", types[type])) {
        for (int i = 0; i < 4; ++i) if (ImGui::Selectable(types[i], i == type)) {
            type = i; config::GGPOEnable = i == 1; config::NetworkEnable = i == 2; config::BattleCableEnable = i == 3;
        }
        ImGui::EndCombo();
    }
    endRow();
    if (type == 1) {
        OptionCheckbox(tr("Play as Player 1"), config::ActAsServer, tr("Deselect to play as player 2"));
        textOption("Peer", config::NetworkServer, "Your peer IP address and optional port", ImGuiInputTextFlags_CharsNoBlank);
        OptionSlider(tr("Frame Delay"), config::GGPODelay, 0, 20, tr("Sets Frame Delay, advisable for sessions with ping >100 ms"));
        choice("Left Thumbstick", config::GGPOAnalogAxes, {{0,"Disabled"}, {1,"Horizontal"}, {2,"Full"}});
        OptionCheckbox(tr("Enable Chat"), config::GGPOChat, tr("Open the chat window when a chat message is received"));
        if (config::GGPOChat) {
            OptionCheckbox(tr("Enable Chat Window Timeout"), config::GGPOChatTimeoutToggle, tr("Automatically close chat window after 20 seconds"));
            if (config::GGPOChatTimeoutToggle) {
                beginRow(tr("Chat Window Timeout (seconds)"), nullptr, config::GGPOChatTimeout.isReadOnly());
                int timeout = config::GGPOChatTimeout;
                if (ImGui::InputInt("##timeout", &timeout) && timeout >= 0) config::GGPOChatTimeout = timeout;
                endRow();
            }
        }
        OptionCheckbox(tr("Network Statistics"), config::NetworkStats, tr("Display network statistics on screen"));
    } else if (type == 2) {
        int role = config::ActAsServer ? 0 : config::NaomiSatellite ? 2 : 1;
        const char* roles[] = {tr("Master"), tr("Slave"), tr("Satellite")};
        beginRow(tr("Network Role"), nullptr, config::ActAsServer.isReadOnly() || config::NaomiSatellite.isReadOnly());
        if (ImGui::BeginCombo("##role", roles[role])) {
            for (int i = 0; i < 3; ++i) if (ImGui::Selectable(roles[i], i == role)) {
                config::ActAsServer = i == 0; config::NaomiSatellite = i == 2;
            }
            ImGui::EndCombo();
        }
        endRow();
        if (!config::ActAsServer) textOption("Server", config::NetworkServer, "The server to connect to. Leave blank to find a server automatically on the default port", ImGuiInputTextFlags_CharsNoBlank);
    } else if (type == 3) {
#ifdef USE_ICE
        section("Match Code");
        static std::string matchCode;
        auto state = ice::getState();
        beginRow(tr("Code"), tr("Choose a unique word or number and share it with your opponent."));
        ImGui::InputText("##code", &matchCode, state == ice::Offline ? ImGuiInputTextFlags_CharsNoBlank : ImGuiInputTextFlags_ReadOnly);
        endRow();
        if (state == ice::Offline) {
            DisabledScope lock(matchCode.empty());
            if (button("Connect")) ice::init(matchCode, true);
        } else if (button("Disconnect")) {
            try { ice::term(); } catch (...) {}
        }
        muted(ice::getStatusText().c_str());
        OptionCheckbox(tr("Network Statistics"), config::NetworkStats, tr("Display network statistics on screen"));
#endif
        section("Manual");
        textOption("Peer", config::NetworkServer, "The peer to connect to. Leave blank to find a player automatically on the default port", ImGuiInputTextFlags_CharsNoBlank);
        OptionCheckbox(tr("Act as Master"), config::ActAsServer, tr("Only used for Maximum Speed. One of the peer must be master."));
    }
    if (type == 2 || type == 3) {
        beginRow(tr("Local Port"), tr("The local UDP port to use"), config::LocalPort.isReadOnly());
        int port = config::LocalPort;
        if (ImGui::InputInt("##port", &port) && port >= 1 && port <= 65535) config::LocalPort = port;
        endRow();
    }
    section("Network Options");
    OptionCheckbox(tr("Enable UPnP"), config::EnableUPnP, tr("Automatically configure your network router for netplay"));
    OptionCheckbox(tr("Broadcast Digital Outputs"), config::NetworkOutput, tr("Broadcast digital outputs and force-feedback state on TCP port 8000. Compatible with the \"-output network\" MAME option. Arcade games only."));
    { DisabledScope lock(game_started); OptionCheckbox(tr("Broadband Adapter Emulation"), config::EmulateBBA, tr("Emulate the Ethernet Broadband Adapter (BBA) instead of the Modem")); }
    OptionCheckbox(tr("Use DCNet"), config::UseDCNet, tr("Use the DCNet cloud service for Dreamcast Internet access."));
    beginRow(tr("ISP User Name"), tr("The ISP user name stored in the console Flash RAM. Used by some online games as the player name. Leave blank to keep the current Flash RAM value."), config::ISPUsername.isReadOnly());
    InputText("##isp", &config::ISPUsername.get(), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter,
        [](ImGuiInputTextCallbackData* data) { return (int)(data->EventChar <= ' ' || data->EventChar > '~'); });
    endRow();
#if !defined(NDEBUG) || defined(DEBUGFAST)
    { DisabledScope lock(config::UseDCNet); textOption("DNS", config::DNS, "DNS server name or IP address", ImGuiInputTextFlags_CharsNoBlank); }
#endif
#ifdef NAOMI_MULTIBOARD
    choice("Multiboard Screens", config::MultiboardSlaves, {{1, "1 (Twin, Satellite)"}, {2, "2+ (Deluxe, Main screen)"}});
#endif
}
void aboutPage() {
    section("Source credits");
    ImGui::PushFont(nullptr, uiLargeFontSize()); ImGui::TextUnformatted("Flycast"); ImGui::PopFont();
    muted(tr("Original emulator by flyinghead and the Flycast contributors. Derived from reicast."));
    ImGui::TextLinkOpenURL("flyinghead / flycast", "https://github.com/flyinghead/flycast");
    ImGui::TextLinkOpenURL(tr("flyinghead on GitHub"), "https://github.com/flyinghead");
    ImGui::TextLinkOpenURL(tr("Contributors"), "https://github.com/flyinghead/flycast/graphs/contributors");
    ImGui::Spacing();
    ImGui::BeginChild("star", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    ImGui::TextColored(accent(), ICON_FA_STAR); ImGui::SameLine();
    ImGui::TextWrapped("%s", tr("Enjoying Flycast? Give flyinghead's original project a star on GitHub."));
    ImGui::TextLinkOpenURL(tr("Star Flycast on GitHub"), "https://github.com/flyinghead/flycast");
    ImGui::EndChild();
    section("Clarity interface");
    muted(tr("Clarity UI and GameTranslate branding are additions in this fork. Emulator source credit belongs to the original authors."));
    ImGui::TextLinkOpenURL("GameTranslate / Flycast", "https://github.com/gametranslate/flycast");
    ImGui::TextLinkOpenURL("gametranslate.us", "https://gametranslate.us");
    section("License & history");
    muted(tr("Flycast is free software under the GNU GPL. This build retains the upstream license and copyright notices."));
    ImGui::TextLinkOpenURL("GNU GPL · LICENSE", "https://github.com/flyinghead/flycast/blob/master/LICENSE");
    ImGui::TextLinkOpenURL("reicast", "https://github.com/skmp/reicast-emulator");
    ImGui::TextLinkOpenURL(tr("Third-party libraries"), "https://github.com/flyinghead/flycast/tree/master/core/deps");
    section("Build & system");
    gui_settings_about();
}
}
