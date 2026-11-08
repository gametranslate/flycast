// SPDX-License-Identifier: GPL-2.0-or-later
#include "clarity.h"
#include "model.h"
#include "ui/settings.h"
#include "ui/gui.h"
#include "ui/imgui_driver.h"
#include "oslib/resources.h"
#include "wsi/context.h"
#include "ui/IconsFontAwesome6.h"
#include "stb_image.h"

#include <algorithm>
#include <unordered_map>

namespace clarity {
namespace {
// Preferences are deliberately global and independent of the Dreamcast BIOS
// language and legacy UILanguage (which is a per-game option upstream).
config::Option<std::string, false> language("Language", "en", "clarity");
config::Option<bool, false> night("Night", false, "clarity");
constexpr const char* localeIds[] = {"en", "vi", "de", "fr", "es", "zh_CN", "zh_TW", "ko"};
constexpr const char* localeNames[] = {"English", "Tiếng Việt", "Deutsch", "Français", "Español", "简体中文", "繁體中文", "한국어"};
const std::unordered_map<std::string, std::array<const char*, 8>> strings = {
#include "strings.inc"
};
int localeIndex() {
    for (int i = 0; i < 8; ++i) if (language.get() == localeIds[i]) return i;
    return 0;
}
const char* tr(const char* key) {
    auto it = strings.find(key);
    return it == strings.end() ? key : it->second[localeIndex()];
}
Model& model() { static Model value; return value; }
bool classic = false;
bool initialized = false;
bool advanced = false;
int category = 0;
enum class Destination { None, Done, Classic };
Destination pending = Destination::None;
ImGuiTextFilter filter;
int visibleRows = 0;

float px(float n) { return uiScaled(n); }
ImVec4 color(unsigned rgb) { return ImVec4((rgb >> 16) / 255.f, ((rgb >> 8) & 255) / 255.f, (rgb & 255) / 255.f, 1); }
ImVec4 accent() { return color(night ? 0xff9b72 : 0xb8370b); }
ImVec4 tint() { return color(night ? 0x3c2b2a : 0xffeee5); }

struct Theme {
    ImGuiStyle previous = ImGui::GetStyle();
    Theme() {
        auto& s = ImGui::GetStyle();
        if (night) ImGui::StyleColorsDark(); else ImGui::StyleColorsLight();
        s.Colors[ImGuiCol_WindowBg] = color(night ? 0x111827 : 0xf5f6fa);
        s.Colors[ImGuiCol_ChildBg] = color(night ? 0x182233 : 0xffffff);
        s.Colors[ImGuiCol_PopupBg] = s.Colors[ImGuiCol_ChildBg];
        s.Colors[ImGuiCol_Text] = color(night ? 0xe9edf5 : 0x14263e);
        s.Colors[ImGuiCol_TextDisabled] = color(night ? 0xa5b2c6 : 0x54657e);
        s.Colors[ImGuiCol_Border] = color(night ? 0x354258 : 0xd8deea);
        s.Colors[ImGuiCol_FrameBg] = color(night ? 0x253248 : 0xedf0f5);
        s.Colors[ImGuiCol_FrameBgHovered] = tint();
        s.Colors[ImGuiCol_FrameBgActive] = tint();
        s.Colors[ImGuiCol_Button] = s.Colors[ImGuiCol_FrameBg];
        s.Colors[ImGuiCol_ButtonHovered] = tint();
        s.Colors[ImGuiCol_ButtonActive] = tint();
        s.Colors[ImGuiCol_Header] = tint();
        s.Colors[ImGuiCol_HeaderHovered] = tint();
        s.Colors[ImGuiCol_HeaderActive] = tint();
        s.Colors[ImGuiCol_CheckMark] = accent();
        s.Colors[ImGuiCol_SliderGrab] = accent();
        s.Colors[ImGuiCol_SliderGrabActive] = accent();
        s.Colors[ImGuiCol_NavCursor] = accent();
        s.Colors[ImGuiCol_TextLink] = accent();
        s.WindowRounding = 0;
        s.ChildRounding = px(12);
        s.FrameRounding = px(7);
        s.PopupRounding = px(8);
        s.WindowPadding = ScaledVec2(18, 14);
        s.FramePadding = ScaledVec2(12, 8);
        s.ItemSpacing = ScaledVec2(10, 12);
        s.CellPadding = ScaledVec2(14, 14);
    }
    ~Theme() { ImGui::GetStyle() = previous; }
};

void muted(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}
bool button(const char* key, bool selected = false, ImVec2 size = {}) {
    ImGui::PushStyleColor(ImGuiCol_Button, selected ? tint() : ImGui::GetStyleColorVec4(ImGuiCol_Button));
    ImGui::PushStyleColor(ImGuiCol_Text, selected ? accent() : ImGui::GetStyleColorVec4(ImGuiCol_Text));
    bool clicked = ImGui::Button((std::string(tr(key)) + "###" + key).c_str(), size);
#ifdef FLYCAST_CLARITY_SMOKE_TEST
    smokeObserve(key);
#endif
    ImGui::PopStyleColor(2);
    return clicked;
}
bool matches(const char* label, const char* help) {
    return !filter.IsActive() || filter.PassFilter(tr(label)) || (help && filter.PassFilter(tr(help)));
}

// Each row wraps explanatory text and keeps controls in a consistent column.
bool row(const char* label, const char* help, bool locked = false) {
    if (!matches(label, help)) return false;
    ++visibleRows;
    ImGui::PushID(label);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, color(night ? 0x182233 : 0xffffff));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ScaledVec2(16, 4));
    ImGui::BeginChild("card", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    const float available = ImGui::GetContentRegionAvail().x;
    bool narrow = available < px(420);
    ImGui::BeginTable("row", narrow ? 1 : 2, ImGuiTableFlags_SizingStretchProp);
    if (!narrow) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthFixed, std::min(px(210), available * .42f));
    }
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", tr(label));
    if (help) muted(tr(help));
    if (locked) muted(tr("locked"));
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1);
    ImGui::BeginDisabled(locked);
    return true;
}
void endRow() {
    ImGui::EndDisabled();
    ImGui::EndTable();
    ImGui::EndChild();
    ImGui::PopID();
}
void toggle(const char* label, const char* help, bool& value, bool locked) {
    if (!row(label, help, locked)) return;
    ImGui::Checkbox("##value", &value);
    ImGui::SameLine();
    ImGui::TextUnformatted(tr(value ? "on" : "off"));
    endRow();
}

RenderType baseRenderer(RenderType r) {
    switch (r) {
    case RenderType::OpenGL_OIT: return RenderType::OpenGL;
    case RenderType::Vulkan_OIT: return RenderType::Vulkan;
    case RenderType::DirectX11_OIT: return RenderType::DirectX11;
    default: return r;
    }
}
RenderType pixelRenderer(RenderType r) {
    switch (baseRenderer(r)) {
    case RenderType::OpenGL: return RenderType::OpenGL_OIT;
    case RenderType::Vulkan: return RenderType::Vulkan_OIT;
    case RenderType::DirectX11: return RenderType::DirectX11_OIT;
    default: return r;
    }
}
const char* rendererName(RenderType r) {
    switch (baseRenderer(r)) {
    case RenderType::OpenGL: return "OpenGL";
    case RenderType::Vulkan: return "Vulkan";
    case RenderType::DirectX9: return "DirectX 9";
    case RenderType::DirectX11: return "DirectX 11";
    default: return "?";
    }
}

void graphics() {
    auto& m = model();
    if (button("basic", !advanced)) advanced = false;
    ImGui::SameLine();
    if (button("advanced", advanced)) advanced = true;
    if (!advanced) {
        if (!filter.IsActive()) {
            muted(tr("quick"));
            const char* keys[] = {"original", "balanced", "sharp"};
            float w = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3;
            ImGui::BeginDisabled(m.locked(config::RenderResolution) || m.framebuffer);
            for (int i = 0; i < 3; ++i) {
                if (i) ImGui::SameLine();
                std::string label = std::string(tr(keys[i])) + "\n" + std::to_string(i + 1) + "x";
                ImGui::PushStyleColor(ImGuiCol_Button, m.resolution == (i + 1) * 480 ? tint() : color(night ? 0x182233 : 0xffffff));
                if (ImGui::Button(label.c_str(), ImVec2(w, px(64)))) m.resolution = (i + 1) * 480;
#ifdef FLYCAST_CLARITY_SMOKE_TEST
                smokeObserve(keys[i]);
#endif
                ImGui::PopStyleColor();
            }
            ImGui::EndDisabled();
        }
        if (row("resolution", "resolutionHelp", m.locked(config::RenderResolution) || m.framebuffer)) {
            std::string current = std::to_string(m.resolution) + "p";
            if (ImGui::BeginCombo("##resolution", current.c_str())) {
                for (int value : {240, 480, 720, 960, 1200, 1440, 1920, 2160, 2400, 2880, 3360, 3840, 4320}) {
                    std::string name = std::to_string(value) + "p (" + std::to_string(value / 480.f).substr(0, 3) + "x)";
                    if (ImGui::Selectable(name.c_str(), m.resolution == value)) m.resolution = value;
                }
                ImGui::EndCombo();
            }
            endRow();
        }
        if (row("aspect", "aspectHelp", m.locked(config::Widescreen) || m.framebuffer)) {
            if (ImGui::BeginCombo("##aspect", m.widescreen ? "16:9" : "4:3")) {
                if (ImGui::Selectable("4:3", !m.widescreen)) m.widescreen = false;
                if (ImGui::Selectable("16:9", m.widescreen)) m.widescreen = true;
                ImGui::EndCombo();
            }
            endRow();
        }
        toggle("vsync", "vsyncHelp", m.vsync, m.locked(config::VSync));
        muted(tr(m.framebuffer ? "framebufferNative" : m.widescreen ? "wideNote" : "graphicsNote"));
    } else {
        if (row("renderer", "rendererHelp", m.locked(config::RendererType))) {
            if (ImGui::BeginCombo("##renderer", rendererName(m.renderer))) {
                const RenderType backends[] = {
#ifdef USE_DX11
                    RenderType::DirectX11,
#endif
#ifdef USE_VULKAN
                    RenderType::Vulkan,
#endif
#ifdef USE_OPENGL
                    RenderType::OpenGL,
#endif
#ifdef USE_DX9
                    RenderType::DirectX9,
#endif
                };
                for (auto r : backends) {
                    if (ImGui::Selectable(rendererName(r), baseRenderer(m.renderer) == r) && baseRenderer(m.renderer) != r)
                        // A new API starts in a supported non-OIT mode. Capability
                        // checks below use the live context after applying it.
                        m.renderer = r;
                }
                ImGui::EndCombo();
            }
            endRow();
        }
        bool pendingBackend = baseRenderer(m.renderer) != baseRenderer(config::RendererType);
        if (row("sorting", "sortingHelp", m.locked(config::RendererType) || m.locked(config::PerStripSorting))) {
            bool pixel = baseRenderer(m.renderer) != m.renderer;
            const char* selected = pixel ? "pixel" : m.perStrip ? "strip" : "triangle";
            if (ImGui::BeginCombo("##sorting", tr(selected))) {
                for (const char* key : {"triangle", "strip", "pixel"}) {
                    bool wantsPixel = std::string(key) == "pixel";
                    bool unavailable = wantsPixel && (pendingBackend || baseRenderer(m.renderer) == RenderType::DirectX9
                        || !GraphicsContext::Instance()->hasPerPixel());
                    ImGui::BeginDisabled(unavailable);
                    if (ImGui::Selectable(tr(key), std::string(selected) == key)) {
                        m.renderer = wantsPixel ? pixelRenderer(m.renderer) : baseRenderer(m.renderer);
                        if (!wantsPixel) m.perStrip = std::string(key) == "strip";
                    }
                    ImGui::EndDisabled();
                }
                ImGui::EndCombo();
            }
            endRow();
        }
        if (pendingBackend) muted(tr("restartGraphics"));
        toggle("framebuffer", "framebufferHelp", m.framebuffer, m.locked(config::EmulateFramebuffer));
        muted(tr("framebufferNative"));
    }
}

void audio() {
    auto& m = model();
    if (row("volume", "volumeHelp", m.locked(config::AudioVolume))) {
        ImGui::SliderInt("##volume", &m.volume, 0, 100, "%d%%");
        endRow();
    }
    // Match upstream backend eligibility; do not round samples merely by viewing.
    if (!config::AutoLatency || (config::AudioBackend.get() != "auto" && config::AudioBackend.get() != "android")) {
        if (row("manualLatency", "manualLatencyHelp", m.locked(config::AudioBufferSize))) {
            int ms = (int)std::round(m.bufferSize * 1000.f / 44100.f);
            if (ImGui::SliderInt("##buffer", &ms, 12, 512, "%d ms")) m.bufferSize = (int)std::round(ms * 44100.f / 1000.f);
            endRow();
        }
    }
    toggle("dsp", "dspNativeHelp", m.dsp, m.locked(config::DSPEnabled));
    toggle("vmuSound", "vmuSoundHelp", m.vmuSound, m.locked(config::VmuSound));
}

void controls() {
    auto& m = model();
    muted(tr("shared"));
    if (m.devices.empty()) muted(tr("noDevices"));
    for (auto& d : m.devices) {
        auto p = d.device.lock();
        if (!p) continue;
        ImGui::PushID(p.get());
        ImGui::TextWrapped("%s  (%s)", p->name().c_str(), p->api_name().c_str());
        if (row("assignment", nullptr)) {
            const char* ports[] = {tr("disabled"), "A", "B", "C", "D", tr("allPorts")};
            if (ImGui::BeginCombo("##port", ports[std::clamp(d.port + 1, 0, 5)])) {
                for (int i = 0; i < 6; ++i)
                    if (ImGui::Selectable(ports[i], d.port == i - 1)) d.port = i - 1;
                ImGui::EndCombo();
            }
            endRow();
        }
        if (p->has_analog_stick() && p->get_input_mapping() && row("deadzone", "deadzoneHelp")) {
            int percent = (int)std::round(d.deadZone * 100);
            if (ImGui::SliderInt("##deadzone", &percent, 0, 40, "%d%%")) d.deadZone = percent / 100.f;
            endRow();
        }
        ImGui::PopID();
    }
    toggle("vmulcd", "vmulcdHelp", m.vmuLcd, m.locked(config::FloatVMUs));
    muted(tr("portHelp"));
}

void storage() {
    auto& m = model();
    muted(tr("scopeStorage"));
    if (matches("folder", "folderHelp")) {
        ++visibleRows;
        ImGui::TextUnformatted(tr("folder"));
        if (m.paths.empty()) muted(tr("noFolders"));
        ImGui::BeginDisabled(config::ContentPath.isReadOnly());
        for (size_t i = 0; i < m.paths.size(); ++i) {
            ImGui::PushID((int)i);
            ImGui::TextWrapped("%s", m.paths[i].c_str());
            if (button("removeFolder")) m.paths.erase(m.paths.begin() + i--);
            ImGui::PopID();
        }
        std::string picker = std::string(tr("addFolder")) + "###clarity-folder-picker";
        bool open = button("addFolder");
        select_file_popup(picker.c_str(), [](bool cancelled, std::string path) {
            if (!cancelled && !config::ContentPath.isReadOnly()) {
                auto& paths = model().paths;
                if (std::find(paths.begin(), paths.end(), path) == paths.end()) paths.push_back(std::move(path));
            }
            return true;
        });
        if (open) ImGui::OpenPopup(picker.c_str());
        ImGui::EndDisabled();
        ImGui::Separator();
    }
    toggle("autoSave", "saveStateHelp", m.autoSave, m.locked(config::AutoSaveState));
    toggle("autoLoad", "saveStateHelp", m.autoLoad, m.locked(config::AutoLoadState));
}

void branding() {
    const char* name = "clarity/gametranslate.png";
    ImTextureID texture = imguiDriver->getTexture(name);
    if (!texture) {
        size_t size = 0;
        auto bytes = resource::load(name, size);
        if (bytes) {
            int w, h, channels;
            stbi_set_flip_vertically_on_load_thread(0);
            auto* data = stbi_load_from_memory(bytes.get(), (int)size, &w, &h, &channels, STBI_rgb_alpha);
            if (data) {
                texture = imguiDriver->updateTexture(name, data, w, h, false);
                stbi_image_free(data);
            }
        }
    }
    if (texture) ImGui::Image(texture, ScaledVec2(38, 38));
    if (ImGui::GetWindowWidth() > px(170)) {
        if (texture) ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("GameTranslate");
        ImGui::TextLinkOpenURL("gametranslate.us", "https://gametranslate.us");
        ImGui::EndGroup();
    } else {
        ImGui::TextLinkOpenURL(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE "##GameTranslate", "https://gametranslate.us");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("gametranslate.us");
    }
}

void apply(const std::function<void(bool)>& finish) {
    auto& m = model();
    bool pathsChanged = m.paths != config::ContentPath.get() && !config::ContentPath.isReadOnly();
    if (pathsChanged) scanner.stop();
    m.commit();
    finish(false);
    if (pathsChanged) scanner.refresh();
    m.capture();
}
void navigate(Destination to, const std::function<void(bool)>& finish) {
    pending = Destination::None;
    if (to == Destination::Classic) classic = true;
    else if (to == Destination::Done) finish(true);
}
void requestNavigation(Destination to, const std::function<void(bool)>& finish) {
    if (model().count()) {
        pending = to;
    } else navigate(to, finish);
}
}

void beginSession() {
    initialized = false;
    classic = false;
    pending = Destination::None;
}

#ifdef FLYCAST_CLARITY_SMOKE_TEST
void smokeConfigure(int locale, bool dark, int page) {
    language = localeIds[locale];
    night = dark;
    category = page;
    advanced = false;
}
bool smokeCheck(int liveResolution, int draftResolution, bool inClassic) {
    bool ok = config::RenderResolution.get() == liveResolution && model().resolution == draftResolution && classic == inClassic;
    if (!ok) {
        if (auto f = std::fopen("clarity-smoke-values.txt", "w")) {
            std::fprintf(f, "Actual live=%d draft=%d classic=%d; expected=%d/%d/%d\n", config::RenderResolution.get(),
                model().resolution, classic, liveResolution, draftResolution, inClassic);
            std::fclose(f);
        }
    }
    return ok;
}
#endif

bool returnFromClassic() {
    bool clicked = ImGui::Button(tr("backClarity"));
#ifdef FLYCAST_CLARITY_SMOKE_TEST
    smokeObserve("backClarity");
#endif
    if (clicked) {
        classic = false;
        initialized = false;
        return true;
    }
    return false;
}

bool display(const std::function<void(bool)>& finishSettings) {
    if (classic) return false;
    auto& m = model();
    if (!initialized) { m.capture(); initialized = true; }
    m.syncDevices();
    Theme theme;
    fullScreenWindow(false);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Clarity###clarity-settings", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();
    const float width = ImGui::GetContentRegionAvail().x;
    bool compact = width < px(880);
    ImGui::BeginChild("header", ImVec2(0, px(66)), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(accent(), ICON_FA_COMPACT_DISC);
    ImGui::SameLine();
    ImGui::TextUnformatted("flycast");
    if (!compact) { ImGui::SameLine(); muted("C L A R I T Y"); }
    float headerControls = px(compact ? 290 : 385);
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), width - headerControls));
    ImGui::SetNextItemWidth(px(compact ? 125 : 155));
    if (ImGui::BeginCombo("##language", localeNames[localeIndex()])) {
        for (int i = 0; i < 8; ++i) {
            if (ImGui::Selectable(localeNames[i], localeIndex() == i)) {
                language = localeIds[i]; language.save();
                filter.Clear();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (compact) {
        if (ImGui::Button(night ? ICON_FA_MOON : ICON_FA_SUN)) { night = !night; night.save(); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(night ? "night" : "light"));
    } else {
        if (button("light", !night)) { night = false; night.save(); }
        ImGui::SameLine();
        if (button("night", night)) { night = true; night.save(); }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_XMARK "##done")) requestNavigation(Destination::Done, finishSettings);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("done"));
    ImGui::EndChild();

    const char* keys[] = {"video", "audio", "controls", "storage"};
    const char* icons[] = {ICON_FA_DISPLAY, ICON_FA_VOLUME_HIGH, ICON_FA_GAMEPAD, ICON_FA_FOLDER_OPEN};
    float sidebarWidth = compact ? px(68) : px(228);
    if (!compact) for (auto key : keys) sidebarWidth = std::max(sidebarWidth, ImGui::CalcTextSize(tr(key)).x + px(72));
    ImGui::BeginChild("sidebar", ImVec2(sidebarWidth, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (!compact) muted(tr("settings"));
    for (int i = 0; i < 4; ++i) {
        std::string label = std::string(icons[i]) + (compact ? "" : std::string("  ") + tr(keys[i])) + "##" + keys[i];
        ImGui::PushStyleColor(ImGuiCol_Button, category == i ? tint() : ImGui::GetStyleColorVec4(ImGuiCol_ChildBg));
        ImGui::PushStyleColor(ImGuiCol_Text, category == i ? accent() : ImGui::GetStyleColorVec4(ImGuiCol_Text));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(compact ? .5f : 0.f, .5f));
        if (ImGui::Button(label.c_str(), ImVec2(-1, px(44)))) { category = i; filter.Clear(); }
        ImGui::PopStyleVar();
        if (compact && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(keys[i]));
        ImGui::PopStyleColor(2);
    }
    ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY() + px(20), ImGui::GetWindowHeight() - px(compact ? 115 : 95)));
    ImGui::Separator();
    branding();
    ImGui::EndChild();
    ImGui::SameLine(0, 0);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    ImGui::BeginChild("content", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::BeginChild("scroll", ImVec2(0, -px(compact ? 95 : 68)));
    muted(tr("personal"));
    ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.75f);
    ImGui::TextUnformatted(tr(keys[category]));
    ImGui::PopFont();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search", tr("search"), filter.InputBuf, sizeof(filter.InputBuf))) filter.Build();
    ImGui::Separator();
    bool perGame = config::Settings::instance().hasPerGameConfig() || m.createGameProfile;
    ImGui::TextWrapped("%s: %s", tr("applyTo"), tr(perGame ? "currentGame" : "allGames"));
    if (game_started) {
        muted(settings.content.fileName.c_str());
        if (!perGame && button("customize")) m.createGameProfile = true;
        if (perGame) muted(tr("scopeHint"));
    }
    visibleRows = 0;
    switch (category) {
    case 0: graphics(); break;
    case 1: audio(); break;
    case 2: controls(); break;
    case 3: storage(); break;
    }
    if (filter.IsActive() && visibleRows == 0) muted(tr("empty"));
    ImGui::Spacing();
    ImGui::Separator();
    // Queue navigation here; the modal is always opened in the outer window.
    bool openClassic = button("classic");
    muted(tr("classicHelp"));
    ImGui::EndChild();
    ImGui::Separator();
    int changes = m.count();
    std::string status = tr(changes ? "dirty" : "unchanged");
    auto number = status.find("{n}");
    if (number != std::string::npos) status.replace(number, 3, std::to_string(changes));
    muted(status.c_str());
    if (!compact) {
        float buttonsWidth = ImGui::CalcTextSize(tr("revert")).x + ImGui::CalcTextSize(tr("apply")).x + px(68);
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - buttonsWidth));
    }
    ImGui::BeginDisabled(changes == 0);
    if (button("revert")) m.capture();
    ImGui::SameLine();
    if (button("apply", true)) apply(finishSettings);
    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    if (openClassic) requestNavigation(Destination::Classic, finishSettings);
    // Header child uses its own popup ID stack; handle Done here as well.
    if (pending != Destination::None && !ImGui::IsPopupOpen("##clarity-pending")) ImGui::OpenPopup("##clarity-pending");
    ImGui::SetNextWindowSize(ImVec2(std::min(px(520), width - px(30)), 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("##clarity-pending", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", tr("pending"));
        if (button("applyContinue")) {
            auto to = pending; apply(finishSettings); ImGui::CloseCurrentPopup(); navigate(to, finishSettings);
        }
        if (button("discard")) {
            auto to = pending; m.capture(); ImGui::CloseCurrentPopup(); navigate(to, finishSettings);
        }
        if (button("cancel")) { pending = Destination::None; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    ImGui::End();
    return true;
}
}
