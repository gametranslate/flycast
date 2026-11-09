// SPDX-License-Identifier: GPL-2.0-or-later
#include "clarity.h"
#include "oslib/storage.h"
#include "imgui_internal.h"
#include <mutex>

namespace clarity {
namespace {
std::mutex pickerMutex;
bool pending = false, ready = false, cancelled = false;
std::string selectedPath;
bool (*selectionCallback)(bool, std::string) = nullptr;
void receive(bool cancel, std::string path) {
    std::lock_guard<std::mutex> lock(pickerMutex);
    if (!pending) return;
    cancelled = cancel; selectedPath = std::move(path); ready = true;
}
}
bool platformPickerActive() { std::lock_guard<std::mutex> lock(pickerMutex); return pending; }
void shutdownPlatformPicker() {
    std::lock_guard<std::mutex> lock(pickerMutex);
    pending = ready = false; selectionCallback = nullptr; selectedPath.clear();
}
void pollPlatformPicker() {
    bool cancel;
    std::string path;
    bool (*callback)(bool, std::string);
    {
        std::lock_guard<std::mutex> lock(pickerMutex);
        if (!ready) return;
        cancel = cancelled; path = std::move(selectedPath); callback = selectionCallback;
        pending = ready = false; selectionCallback = nullptr;
    }
    // Java delivers SAF results on the activity thread; mutate Flycast options
    // only here, on the same thread that draws and owns the settings UI.
    if (callback) callback(cancel, std::move(path));
}
bool platformPicker(const char* prompt, bool (*callback)(bool, std::string), bool file, const std::string&) {
    if (!ImGui::IsPopupOpen(prompt)) return true;
    if (platformPickerActive()) return true;
    std::string title = prompt;
    title = title.substr(0, title.find("##"));
    {
        std::lock_guard<std::mutex> lock(pickerMutex);
        pending = true; selectionCallback = callback;
    }
    if (!hostfs::addStorage(!file, !file, tr(title.c_str()), receive, file ? "*/*" : "")) {
        shutdownPlatformPicker();
        return false;
    }
    ImGui::ClosePopupToLevel(GImGui->BeginPopupStack.Size, false);
    return true;
}
}
