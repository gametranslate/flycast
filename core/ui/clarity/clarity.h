// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <functional>
#include <string>
#include <array>
#include "imgui.h"

class Boxart;
namespace clarity {
enum class Page { System, Controllers, Devices, Video, Audio, Library, Paths, Network, Achievements, Advanced, About, Debug, Count };
void beginSession();
void openSettings(Page page);
bool display(const std::function<void(bool)>& finishSettings,
    std::array<bool, 4>& maple, std::array<std::array<bool, 2>, 4>& expansion);
void library(Boxart& boxart);
void handleDropPath(const std::string& path);
#if defined(_WIN32) && !defined(TARGET_UWP)
bool windowsPickerMain(int argc, char* argv[], int& exitCode);
void windowsPicker(const char* prompt, bool (*callback)(bool, std::string), bool file, const std::string& extension);
void pollWindowsPicker();
void shutdownWindowsPicker();
bool windowsPickerActive();
#endif
#if defined(CLARITY_DESKTOP_PICKER) || defined(CLARITY_ANDROID_PICKER)
bool platformPicker(const char* prompt, bool (*callback)(bool, std::string), bool file, const std::string& extension);
void pollPlatformPicker();
void shutdownPlatformPicker();
bool platformPickerActive();
#endif
void beginTheme();
void endTheme();
const char* tr(const char* key);
const char* translateBuiltin(const char* text);
std::string locale();
ImVec4 accent();
void muted(const char* text);
void heading(const char* text, const char* subtitle = nullptr);
void section(const char* text);
bool button(const char* key, bool selected = false, ImVec2 size = {});
void branding();
bool cardsActive();
void setCards(bool value);
bool beginRow(const char* title, const char* help = nullptr, bool locked = false);
void endRow();
void graphicsPage();
void audioPage();
void pathsPage();
void networkPage();
void generalPage();
void libraryPage();
void aboutPage();
void drawBackdrop(const char* title);
void progressWindow(const char* title);
void closeMappings();
void saveMappings();
void controllerPage();
void devicesPage(std::array<bool, 4>& maple, std::array<std::array<bool, 2>, 4>& expansion);
#ifdef FLYCAST_CLARITY_SMOKE_TEST
void smokeBeforeFrame();
void smokeAfterFrame();
void smokeConfigure(int locale, bool dark, int page);
void smokeObserve(const char* key);
int smokePage();
void smokeUseTestController();
void smokeScrollToBottom(bool value);
void smokeQuickScroll();
void smokeTestDrops();
#endif
}
