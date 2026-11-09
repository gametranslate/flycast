// SPDX-License-Identifier: GPL-2.0-or-later
#include "clarity.h"
#include "ui/settings.h"
#include "ui/boxart/boxart.h"
#include "ui/IconsFontAwesome6.h"
#include "ui/gui.h"
#include "ui/gui_util.h"
#include "input/gamepad_device.h"
#include "oslib/storage.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <algorithm>

namespace clarity {
void appTopbar();
namespace {
#ifdef FLYCAST_CLARITY_SMOKE_TEST
std::string smokeLaunchedPath;
bool smokeInsertedDisc = false;
#endif
void startMedia(const std::string& path, const std::string& title) {
#ifdef FLYCAST_CLARITY_SMOKE_TEST
    smokeLaunchedPath = path;
    smokeInsertedDisc = gui_state == GuiState::SelectDisk;
    return;
#endif
    if (gui_state == GuiState::SelectDisk) {
        try { emu.insertGdrom(path); gui_setState(GuiState::Closed); }
        catch (const FlycastException& e) { gui_error(e.what()); }
    } else {
        settings.content.title = title;
        gui_start_game(path);
    }
}
bool isDisc(const std::string& extension) {
    return extension == "gdi" || extension == "chd" || extension == "cdi" || extension == "cue";
}
bool launchSelected(bool cancelled, std::string path) {
    if (!cancelled) handleDropPath(path);
    return true;
}
bool iconButton(const char* icon, const char* label, ImVec2 size = {}) {
    return ImGui::Button((std::string(icon) + "  " + tr(label)).c_str(), size);
}
}
void handleDropPath(const std::string& droppedPath) {
    if (gui_state != GuiState::Main && gui_state != GuiState::SelectDisk) return;
#if defined(_WIN32) && !defined(TARGET_UWP)
    if (windowsPickerActive()) return;
#endif
#if defined(CLARITY_DESKTOP_PICKER) || defined(CLARITY_ANDROID_PICKER)
    if (platformPickerActive()) return;
#endif
    try {
        const bool document = droppedPath.rfind("content://", 0) == 0;
        auto path = document ? droppedPath : std::filesystem::weakly_canonical(std::filesystem::u8path(droppedPath)).u8string();
        auto info = hostfs::storage().getFileInfo(path);
        if (info.isDirectory) {
            if (gui_state == GuiState::SelectDisk) return;
            auto& paths = config::ContentPath.get();
            auto equivalent = [&](const std::string& existing) {
                if (document || existing.rfind("content://", 0) == 0) return existing == path;
                std::error_code error;
                return std::filesystem::equivalent(std::filesystem::u8path(existing), std::filesystem::u8path(path), error);
            };
            if (std::none_of(paths.begin(), paths.end(), equivalent)) {
                scanner.stop(); paths.push_back(path); SaveSettings(); scanner.refresh();
            }
            return;
        }
        std::string extension = get_file_extension(info.name);
        bool supported = isDisc(extension) || extension == "zip" || extension == "7z" || extension == "elf"
            || extension == "bin" || extension == "lst" || extension == "dat";
        if (!supported || (gui_state == GuiState::SelectDisk && !isDisc(extension))) {
            gui_error(tr("This file is not a supported game or disc."));
            return;
        }
        startMedia(path, get_file_basename(path));
    } catch (const std::exception& e) { gui_error(e.what()); }
}
void library(Boxart& boxart) {
    fullScreenWindow(false);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::Begin("##clarity-library", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);
    appTopbar();
    bool compact = ImGui::GetContentRegionAvail().x < uiScaled(900);
    bool disk = gui_state == GuiState::SelectDisk;
    bool open = false, add = false;
    float sidebarWidth = uiScaled(compact ? 84 : 245);
    if (!compact) for (const char* label : {"Library", "Add game folder", "Button mapping", "Settings", "About", "Exit"})
        sidebarWidth = std::max(sidebarWidth, ImGui::CalcTextSize(tr(label)).x + uiScaled(104));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, compact ? ScaledVec2(10, 12) : ScaledVec2(20, 16));
    ImGui::BeginChild("sidebar", ImVec2(sidebarWidth, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    auto nav = [&](const char* icon, const char* text) {
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(compact ? .5f : 0, .5f));
        bool pressed = ImGui::Button((std::string(icon) + (compact ? "" : std::string("  ") + tr(text)) + "##" + text).c_str(), ImVec2(-1, uiScaled(44)));
        ImGui::PopStyleVar();
        if (compact && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(text));
#ifdef FLYCAST_CLARITY_SMOKE_TEST
        smokeObserve(text);
#endif
        return pressed;
    };
    ImGui::BeginChild("navigation", ImVec2(0, -uiScaled(compact ? 100 : 112)), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    if (!compact) muted(tr("PLAY"));
    nav(ICON_FA_LAYER_GROUP, "Library");
    if (!disk) {
        if (nav(ICON_FA_FOLDER_PLUS, "Add game folder")) add = true;
        ImGui::Spacing();
        if (!compact) muted(tr("PERSONALIZE"));
        if (nav(ICON_FA_GAMEPAD, "Button mapping")) openSettings(Page::Controllers);
        if (nav(ICON_FA_GEAR, "Settings")) openSettings(Page::System);
        if (nav(ICON_FA_CIRCLE_INFO, "About")) openSettings(Page::About);
        ImGui::Spacing();
        if (nav(ICON_FA_POWER_OFF, "Exit")) dc_exit();
    } else {
        if (nav(ICON_FA_COMPACT_DISC, "Choose a disc")) open = true;
        if (nav(ICON_FA_ARROW_LEFT, "Cancel")) gui_setState(GuiState::Commands);
    }
    ImGui::EndChild();
    ImGui::Separator(); branding();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::SameLine(0,0);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
    ImGui::BeginChild("library-content", ImVec2(0,0), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleColor();
    scanner.fetch_game_list();
    std::vector<GameMedia> games;
    {
        std::lock_guard<std::mutex> lock(scanner.get_mutex());
        games = scanner.get_game_list();
    }
    heading(disk ? "Choose a disc" : "Your library", disk ? "Select a Dreamcast disc to insert." : "Pick a game. Make yourself at home.");
    static ImGuiTextFilter filter;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##gamesearch", tr("Search your games..."), filter.InputBuf, sizeof(filter.InputBuf))) filter.Build();
    static int platform = 0;
    const char* tabs[] = {"All games", "Dreamcast", "Arcade"};
    for (int i = 0; i < 3; ++i) {
        if (disk && i == 2) break;
        if (i) ImGui::SameLine();
        if (button(tabs[i], platform == i)) platform = i;
    }
    ImGui::Spacing();
#ifndef __ANDROID__
    if (!games.empty()) muted(tr(disk ? "Drop a disc image here to insert it." : "Drop a game to play, or a folder to add it to your library."));
#endif
    if (games.empty()) {
        ImGui::BeginChild("welcome", ImVec2(0,0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
        ImGui::TextColored(accent(), ICON_FA_COMPACT_DISC "  DREAMCAST / NAOMI / ATOMISWAVE");
#ifdef __ANDROID__
        heading(disk ? "Choose a disc" : "Your library", "Choose a game folder using the system picker, then tap a game to play.");
#else
        heading(disk ? "Choose a disc" : "Drop a game here", disk ? "Drop a disc image here to insert it."
            : "Drag a game from your file manager to start playing. Drop a folder to add its games to your library.");
#endif
        if (disk) { if (iconButton(ICON_FA_COMPACT_DISC, "Choose a disc")) open = true; }
        else if (iconButton(ICON_FA_FOLDER_PLUS, "Add game folder")) add = true;
        ImGui::EndChild();
        ImGui::Spacing();
        if (!disk) {
            section("Make it yours");
            if (iconButton(ICON_FA_GAMEPAD, "Set up your controls", ImVec2(-1, uiScaled(54)))) openSettings(Page::Controllers);
            muted(tr("Map buttons, test input, adjust sticks and assign players."));
            if (iconButton(ICON_FA_DISPLAY, "Tune your picture", ImVec2(-1, uiScaled(54)))) openSettings(Page::Video);
            muted(tr("Resolution, widescreen and the renderer that suits your PC."));
            if (iconButton(ICON_FA_FOLDER_OPEN, "Set up BIOS & folders", ImVec2(-1, uiScaled(54)))) openSettings(Page::Paths);
        }
    } else {
        bool grid = config::BoxartDisplayMode;
        if (iconButton(grid ? ICON_FA_LIST : ICON_FA_TABLE_CELLS_LARGE, grid ? "List view" : "Grid view")) {
            config::BoxartDisplayMode = !grid;
            config::BoxartDisplayMode.save();
        }
        ImGui::SameLine();
        if (iconButton(ICON_FA_ROTATE, "Rescan Content")) scanner.refresh();
        ImGui::SameLine(); muted((std::to_string(games.size()) + " " + tr("items")).c_str());
        int columns = grid ? std::max(1, (int)(ImGui::GetContentRegionAvail().x / uiScaled(190))) : 1;
        int visible = 0;
        ImGui::BeginTable("games", columns, ImGuiTableFlags_SizingStretchSame);
        for (const auto& game : games) {
            std::string ext = get_file_extension(game.path);
            if (disk && (game.path.empty() || (!game.device && ext != "gdi" && ext != "chd" && ext != "cdi" && ext != "cue"))) continue;
            if (!disk && ((platform == 1 && game.arcade) || (platform == 2 && !game.arcade))) continue;
            auto art = grid && !game.device ? boxart.getBoxartAndLoad(game) : boxart.getBoxart(game);
            std::string name = art.name.empty() ? game.name : art.name;
            if (!filter.PassFilter(name.c_str()) && !filter.PassFilter(game.name.c_str())) continue;
            ++visible;
            ImGui::TableNextColumn();
            ImGui::PushID(game.path.empty() ? "bios" : game.path.c_str());
            bool launch = false;
            if (grid) {
                float w = ImGui::GetContentRegionAvail().x;
                ImguiFileTexture tex(art.boxartPath);
                launch = tex.button("##cover", ImVec2(w, w), name);
            } else {
                launch = ImGui::Selectable((std::string(game.arcade ? ICON_FA_GAMEPAD : ICON_FA_COMPACT_DISC) + "  " + name).c_str(), false, 0, ImVec2(0, uiScaled(38)));
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s", name.c_str(), game.path.c_str());
            ImGui::PopID();
            if (launch) {
                startMedia(game.path, name == game.fileName ? get_file_basename(game.fileName) : name);
                break;
            }
        }
        ImGui::EndTable();
        if (!visible) muted(tr("No games match your search."));
    }
    ImGui::EndChild();
    if (open) ImGui::OpenPopup("Choose a disc###clarity-disc");
    select_file_popup("Choose a disc###clarity-disc", launchSelected, true);
    addContentPath(add);
    ImGui::End();
}
#ifdef FLYCAST_CLARITY_SMOKE_TEST
void smokeTestDrops() {
    namespace fs = std::filesystem;
    auto original = config::ContentPath.get();
    auto folder = fs::u8path(get_writable_config_path("drop-test-Đức"));
    fs::create_directories(folder);
    auto game = folder / "test.gdi";
    std::ofstream(game) << "UI fixture, not game content.";
    gui_setState(GuiState::Main);
    handleDropPath(folder.u8string());
    handleDropPath((folder / ".").u8string());
    if (config::ContentPath.get().size() != original.size() + 1) throw std::runtime_error("drop folder/duplicate");
    scanner.stop(); config::ContentPath.load();
    if (config::ContentPath.get().size() != original.size() + 1) throw std::runtime_error("drop folder persistence");
    handleDropPath(game.u8string());
    if (smokeLaunchedPath != fs::weakly_canonical(game).u8string() || smokeInsertedDisc) throw std::runtime_error("drop game routing");
    gui_setState(GuiState::SelectDisk);
    handleDropPath(game.u8string());
    if (!smokeInsertedDisc) throw std::runtime_error("drop disc routing");
    gui_setState(GuiState::Loading);
    smokeLaunchedPath.clear(); handleDropPath(game.u8string());
    if (!smokeLaunchedPath.empty()) throw std::runtime_error("drop while busy");
    scanner.stop(); config::ContentPath = original; SaveSettings(); scanner.refresh();
    gui_setState(GuiState::Main);
}
#endif
}
