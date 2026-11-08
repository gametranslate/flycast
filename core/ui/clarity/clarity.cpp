// SPDX-License-Identifier: GPL-2.0-or-later
#include "clarity.h"
#include "ui/settings.h"
#include "ui/gui.h"
#include "ui/imgui_driver.h"
#include "ui/IconsFontAwesome6.h"
#include "oslib/resources.h"
#include "stb_image.h"
#include <unordered_map>
#include <algorithm>

namespace clarity {
namespace {
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
Page page = Page::System;
bool cards = false;
ImGuiStyle savedStyle;
std::string activeLocale;
char search[180]{};
std::string focusSetting;
const char* pageKeys[] = {"Console & appearance", "Button mapping", "Ports & VMU", "Graphics",
    "Audio", "Library & saves", "Folders & BIOS", "Network", "Achievements", "Advanced", "About", "Diagnostics"};
const char* pageIcons[] = {ICON_FA_SLIDERS, ICON_FA_GAMEPAD, ICON_FA_PLUG, ICON_FA_DISPLAY,
    ICON_FA_VOLUME_HIGH, ICON_FA_LAYER_GROUP, ICON_FA_FOLDER_OPEN, ICON_FA_WIFI,
    ICON_FA_TROPHY, ICON_FA_MICROCHIP, ICON_FA_CIRCLE_INFO, ICON_FA_BUG};
const char* pageHelp[] = {
    "Make Flycast feel at home. Console settings are independent of the interface language.",
    "Choose a device, select a control, then press the button or move the axis you want to use.",
    "Connect controllers and accessories to the four Dreamcast ports.",
    "Choose your renderer, picture quality, aspect ratio and compatibility options.",
    "Adjust volume, latency and the audio driver.",
    "Manage your game library and automatic save states.",
    "Choose where Flycast finds BIOS, games, saves and custom content.",
    "Configure Dreamcast online play, GGPO, Naomi networking and Battle Cable.",
    "Connect your RetroAchievements account and track your progress.",
    "CPU, compatibility, texture dumping, scripting and diagnostic options.",
    "The people and projects behind Flycast.",
    "Logging and profiling tools for troubleshooting."
};
float px(float v) { return uiScaled(v); }
ImVec4 color(unsigned rgb) { return ImVec4((rgb >> 16) / 255.f, ((rgb >> 8) & 255) / 255.f, (rgb & 255) / 255.f, 1); }
ImVec4 tint() { return color(night ? 0x3c2b2a : 0xffeee5); }

void preferences(bool compact) {
    ImGui::SetNextItemWidth(px(compact ? 118 : 145));
    if (ImGui::BeginCombo("##language", localeNames[localeIndex()])) {
        for (int i = 0; i < 8; ++i)
            if (ImGui::Selectable(localeNames[i], localeIndex() == i)) {
                language = localeIds[i];
                language.save();
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
}
}

const char* tr(const char* key) {
    if (!key) return nullptr;
    auto it = strings.find(key);
    return it == strings.end() ? i18n::T(key) : it->second[localeIndex()];
}
std::string locale() { return localeIds[localeIndex()]; }
const char* translateBuiltin(const char* key) {
    if (!key) return nullptr;
    auto it = strings.find(key);
    return it == strings.end() ? nullptr : it->second[localeIndex()];
}
ImVec4 accent() { return color(night ? 0xffa37c : 0xb6370d); }
void beginTheme() {
    if (activeLocale != locale()) {
        activeLocale = locale();
        i18n::reloadLanguage();
    }
    savedStyle = ImGui::GetStyle();
    auto& s = ImGui::GetStyle();
    if (night) ImGui::StyleColorsDark(); else ImGui::StyleColorsLight();
    s.Colors[ImGuiCol_WindowBg] = color(night ? 0x101722 : 0xf5f6fa);
    s.Colors[ImGuiCol_ChildBg] = color(night ? 0x182231 : 0xffffff);
    s.Colors[ImGuiCol_PopupBg] = s.Colors[ImGuiCol_ChildBg];
    s.Colors[ImGuiCol_Text] = color(night ? 0xeff2f8 : 0x1b2b43);
    s.Colors[ImGuiCol_TextDisabled] = color(night ? 0xa8b6ca : 0x596b82);
    s.Colors[ImGuiCol_Border] = color(night ? 0x303e53 : 0xdce2ec);
    s.Colors[ImGuiCol_FrameBg] = color(night ? 0x263349 : 0xebeff6);
    s.Colors[ImGuiCol_FrameBgHovered] = tint();
    s.Colors[ImGuiCol_FrameBgActive] = tint();
    s.Colors[ImGuiCol_Button] = s.Colors[ImGuiCol_FrameBg];
    s.Colors[ImGuiCol_ButtonHovered] = tint();
    s.Colors[ImGuiCol_ButtonActive] = tint();
    for (auto c : {ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive,
            ImGuiCol_Tab, ImGuiCol_TabHovered, ImGuiCol_TabSelected}) s.Colors[c] = tint();
    for (auto c : {ImGuiCol_CheckMark, ImGuiCol_SliderGrab, ImGuiCol_SliderGrabActive,
            ImGuiCol_NavCursor, ImGuiCol_TextLink, ImGuiCol_PlotHistogram}) s.Colors[c] = accent();
    s.Colors[ImGuiCol_TitleBgActive] = s.Colors[ImGuiCol_ChildBg];
    s.WindowRounding = px(12);
    s.ChildRounding = px(12);
    s.FrameRounding = px(7);
    s.PopupRounding = px(12);
    s.WindowPadding = ScaledVec2(20, 16);
    s.FramePadding = ScaledVec2(12, 8);
    s.ItemSpacing = ScaledVec2(10, 10);
    s.CellPadding = ScaledVec2(12, 12);
    s.ScrollbarSize = px(12);
}
void endTheme() { cards = false; ImGui::GetStyle() = savedStyle; }
bool cardsActive() { return cards; }
void setCards(bool value) { cards = value; }
void muted(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}
void heading(const char* text, const char* subtitle) {
    ImGui::PushFont(nullptr, px(29));
    ImGui::TextWrapped("%s", tr(text));
    ImGui::PopFont();
    if (subtitle) muted(tr(subtitle));
    ImGui::Spacing();
}
void section(const char* text) {
    ImGui::Spacing();
    const char* icon = ICON_FA_SLIDERS;
    const std::pair<const char*,const char*> icons[] = {
        {"Source credits", ICON_FA_CODE}, {"Clarity interface", ICON_FA_PALETTE},
        {"License & history", ICON_FA_SCALE_BALANCED}, {"Build & system", ICON_FA_MICROCHIP},
        {"Input device", ICON_FA_GAMEPAD}, {"Buttons", ICON_FA_GAMEPAD}, {"Directions", ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT},
        {"Triggers", ICON_FA_HAND_POINTER}, {"System Buttons", ICON_FA_POWER_OFF}, {"Emulator", ICON_FA_KEYBOARD},
        {"Sound", ICON_FA_VOLUME_HIGH}, {"Audio Driver", ICON_FA_VOLUME_HIGH}, {"Game library", ICON_FA_LAYER_GROUP},
        {"Automatic save states", ICON_FA_FLOPPY_DISK}, {"Folders & BIOS", ICON_FA_FOLDER_OPEN},
        {"Graphics API", ICON_FA_DISPLAY}, {"Picture quality", ICON_FA_DISPLAY}, {"Network Type", ICON_FA_WIFI},
        {"Network Options", ICON_FA_WIFI}, {"Memory & display", ICON_FA_MEMORY}, {"Save states", ICON_FA_FLOPPY_DISK},
        {"Continue playing", ICON_FA_PLAY}, {"Appearance", ICON_FA_PALETTE}, {"Console", ICON_FA_COMPACT_DISC}
    };
    for (auto item : icons) if (std::string(text) == item.first || std::string(text) == tr(item.first)) { icon = item.second; break; }
    ImGui::TextColored(accent(), "%s  %s", icon, tr(text));
    ImGui::Separator();
}
bool button(const char* key, bool selected, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button, selected ? tint() : ImGui::GetStyleColorVec4(ImGuiCol_Button));
    ImGui::PushStyleColor(ImGuiCol_Text, selected ? accent() : ImGui::GetStyleColorVec4(ImGuiCol_Text));
    const char* icon = "";
    const std::pair<const char*, const char*> icons[] = {
        {"Save & return", ICON_FA_CHECK}, {"Save changes", ICON_FA_FLOPPY_DISK},
        {"Browse...", ICON_FA_FOLDER_OPEN}, {"Rescan Content", ICON_FA_ROTATE},
        {"light", ICON_FA_SUN}, {"night", ICON_FA_MOON},
        {"Add", ICON_FA_PLUS}, {"Load", ICON_FA_FOLDER_OPEN},
        {"Back to game", ICON_FA_ARROW_LEFT}, {"Cancel", ICON_FA_XMARK},
        {".. Up to Parent Folder", ICON_FA_ARROW_UP}, {"Storage", ICON_FA_HARD_DRIVE},
        {"Select Current Folder", ICON_FA_FOLDER}, {"Rumble & sticks", ICON_FA_GAMEPAD},
        {"Reset mappings", ICON_FA_ROTATE_LEFT}, {"Connect", ICON_FA_LINK},
        {"Disconnect", ICON_FA_LINK_SLASH}
    };
    for (auto item : icons) if (std::string(key) == item.first) { icon = item.second; break; }
    std::string label = *icon ? std::string(icon) + "  " + tr(key) : tr(key);
    bool clicked = ImGui::Button((label + "###" + key).c_str(), size);
#ifdef FLYCAST_CLARITY_SMOKE_TEST
    smokeObserve(key);
#endif
    ImGui::PopStyleColor(2);
    return clicked;
}
bool beginRow(const char* title, const char* help, bool locked) {
    ImGui::PushID(title);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ScaledVec2(14, 2));
    ImGui::BeginChild("setting", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    float w = ImGui::GetContentRegionAvail().x;
    bool narrow = w < px(460);
    ImGui::BeginTable("setting-row", narrow ? 1 : 2, ImGuiTableFlags_SizingStretchProp);
    if (!narrow) {
        ImGui::TableSetupColumn("description", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, std::min(px(250), w * .4f));
    }
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", title);
    if (help && *help) muted(help);
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
    if (texture) ImGui::Image(texture, ScaledVec2(34, 34));
    if (ImGui::GetWindowWidth() > px(170)) {
        if (texture) ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("GameTranslate");
        ImGui::TextLinkOpenURL("gametranslate.us", "https://gametranslate.us");
        ImGui::EndGroup();
        muted(tr("Clear settings. Your way to play."));
    } else {
        ImGui::TextLinkOpenURL(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE "##GameTranslate", "https://gametranslate.us");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("gametranslate.us");
    }
}
// Shared application chrome. Each child owns its scrolling; the footer never
// covers the final setting or the branding at the bottom of the sidebar.
static void topbar(const char* context) {
    float width = ImGui::GetContentRegionAvail().x;
    bool compact = width < px(900);
    ImGui::BeginChild("topbar", ImVec2(0, px(66)), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(accent(), ICON_FA_COMPACT_DISC);
    ImGui::SameLine(); ImGui::TextUnformatted("flycast");
    if (!compact) { ImGui::SameLine(); muted("C L A R I T Y"); }
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), width - px(compact ? 208 : 365)));
    preferences(compact);
    ImGui::EndChild();
}
void drawBackdrop(const char* title) {
    fullScreenWindow(false);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::Begin("##clarity-backdrop", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored(accent(), ICON_FA_COMPACT_DISC "  flycast / CLARITY");
    ImGui::SetCursorPosY(px(84));
    heading(title);
    ImGui::End();
    ImGui::PopStyleVar();
}
void progressWindow(const char* title) {
    drawBackdrop(title);
    centerNextWindow();
    ImGui::SetNextWindowSize(ImVec2(std::min(px(560), ImGui::GetIO().DisplaySize.x - px(36)), 0));
    ImGui::SetNextWindowBgAlpha(1.f);
}
void beginSession() { search[0] = 0; }
void openSettings(Page target) {
    gui_setState(GuiState::Settings);
    page = target;
}

bool display(const std::function<void(bool)>& finishSettings,
        std::array<bool, 4>& maple, std::array<std::array<bool, 2>, 4>& expansion) {
    bool save = false, close = false;
    fullScreenWindow(false);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::Begin("Clarity###clarity-settings", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);
    topbar("Settings");
    bool compact = ImGui::GetContentRegionAvail().x < px(900);
    float sidebarWidth = px(compact ? 84 : 245);
    if (!compact) for (const char* label : pageKeys)
        sidebarWidth = std::max(sidebarWidth, ImGui::CalcTextSize(tr(label)).x + px(104));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, compact ? ScaledVec2(10, 12) : ScaledVec2(20, 16));
    ImGui::BeginChild("sidebar", ImVec2(sidebarWidth, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (ImGui::Button(compact ? ICON_FA_ARROW_LEFT : (std::string(ICON_FA_ARROW_LEFT "  ") + tr(game_started ? "Back to game" : "Library")).c_str(),
            ImVec2(-1, px(42)))) save = close = true;
#ifdef FLYCAST_CLARITY_SMOKE_TEST
    smokeObserve("backLibrary");
#endif
    if (!compact) muted(tr("settings"));
    ImGui::BeginChild("navigation", ImVec2(0, -px(compact ? 100 : 112)), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    int count = (int)Page::Count;
#if defined(NDEBUG) && !defined(DEBUGFAST) && !FC_PROFILER
    count--;
#endif
    for (int i = 0; i < count; ++i) {
        std::string label = std::string(pageIcons[i]) + (compact ? "" : std::string("  ") + tr(pageKeys[i])) + "##nav" + std::to_string(i);
        ImGui::PushStyleColor(ImGuiCol_Button, (int)page == i ? tint() : ImGui::GetStyleColorVec4(ImGuiCol_ChildBg));
        ImGui::PushStyleColor(ImGuiCol_Text, (int)page == i ? accent() : ImGui::GetStyleColorVec4(ImGuiCol_Text));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(compact ? .5f : 0.f, .5f));
        if (ImGui::Button(label.c_str(), ImVec2(-1, px(39)))) {
            closeMappings(); page = (Page)i; search[0] = 0; save = true;
        }
#ifdef FLYCAST_CLARITY_SMOKE_TEST
        smokeObserve(pageKeys[i]);
#endif
        ImGui::PopStyleVar();
        if (compact && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(pageKeys[i]));
        ImGui::PopStyleColor(2);
    }
    ImGui::EndChild();
    ImGui::Separator(); branding();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::SameLine(0, 0);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    ImGui::BeginChild("workspace", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", tr("Find a setting or category..."), search, sizeof(search));
    static Page lastPage = Page::Count;
    ImGui::BeginChild("content", ImVec2(0, -px(66)), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    if (lastPage != page) { ImGui::SetScrollY(0); lastPage = page; }
    if (search[0]) {
        heading("Search settings");
        ImGuiTextFilter query(search);
        bool found = false;
        for (int i = 0; i < count; ++i) {
            // Search titles and the full list of setting labels, in English
            // and the selected locale. Results lead directly to their page.
            bool match = query.PassFilter(pageKeys[i]) || query.PassFilter(tr(pageKeys[i]))
                || query.PassFilter(pageHelp[i]) || query.PassFilter(tr(pageHelp[i]));
            struct SearchEntry { Page page; const char* label; };
            static const SearchEntry entries[] = {
#include "search.inc"
            };
            std::string details;
            for (auto& entry : entries) if ((int)entry.page == i &&
                    (query.PassFilter(entry.label) || query.PassFilter(tr(entry.label)))) {
                match = true;
                if (details.size() < 220) { if (!details.empty()) details += " · "; details += tr(entry.label); }
            }
            if (match) {
                found = true;
                if (button(pageKeys[i], false, ImVec2(-1, px(44)))) {
                    closeMappings(); page = (Page)i; search[0] = 0; save = true;
                }
                if (!details.empty()) muted(details.c_str());
            }
        }
        if (!found) muted(tr("empty"));
    } else {
        heading(pageKeys[(int)page], pageHelp[(int)page]);
        bool perGame = config::Settings::instance().hasPerGameConfig();
        if (game_started) {
            ImGui::TextWrapped("%s: %s", tr(perGame ? "currentGame" : "allGames"), settings.content.fileName.c_str());
            if (button(perGame ? "Delete Game Config" : "Make Game Config")) {
                if (perGame) ImGui::OpenPopup("##delete-profile");
                else config::Settings::instance().setPerGameConfig(true);
            }
            if (ImGui::BeginPopupModal("##delete-profile", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextWrapped("%s", tr("Delete this game's overrides and restore global settings?"));
                if (button("Delete Game Config")) {
                    closeMappings();
                    config::Settings::instance().setPerGameConfig(false);
                    config::Settings::instance().load(false);
                    loadGameSpecificSettings();
                    maple.fill(true);
                    save = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (button("Cancel")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        cards = true;
        switch (page) {
        case Page::System: generalPage(); break;
        case Page::Controllers: cards = false; controllerPage(); break;
        case Page::Devices: devicesPage(maple, expansion); break;
        case Page::Video: graphicsPage(); break;
        case Page::Audio: audioPage(); break;
        case Page::Library: libraryPage(); break;
        case Page::Paths: pathsPage(); break;
        case Page::Network: networkPage(); break;
        case Page::Achievements:
#ifdef USE_RACHIEVEMENTS
            gui_settings_achievements();
#else
            muted(tr("RetroAchievements is not available in this build."));
#endif
            break;
        case Page::Advanced: gui_settings_advanced(); break;
        case Page::About: aboutPage(); break;
        case Page::Debug:
#if !defined(NDEBUG) || defined(DEBUGFAST) || FC_PROFILER
            gui_debug_tab();
#endif
            break;
        default: break;
        }
        cards = false;
    }
    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::Separator();
    if (button("Save & return", true)) save = close = true;
    ImGui::SameLine();
    if (button("Save changes")) save = true;
    if (!compact) {
        ImGui::SameLine();
        muted(tr("Edits apply immediately. Saved when you leave."));
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
    if (save) {
        if (close) closeMappings();
        else saveMappings();
        finishSettings(close);
    }
    return true; // No Classic destination in Clarity builds.
}

// Used by the home/library screen, sharing the same top bar and preferences.
void appTopbar() { topbar("Library"); }
#ifdef FLYCAST_CLARITY_SMOKE_TEST
int smokePage() { return (int)page; }
void smokeConfigure(int index, bool dark, int selectedPage) {
    closeMappings();
    language = localeIds[index]; night = dark;
    page = (Page)selectedPage;
    search[0] = 0;
}
#endif
}
