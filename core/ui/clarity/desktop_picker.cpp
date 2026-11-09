// SPDX-License-Identifier: GPL-2.0-or-later
#include "clarity.h"
#include "ui/gui.h"
#include "oslib/storage.h"
#include "imgui_internal.h"
#include <nfd.h>

namespace clarity {
namespace {
struct Request {
    std::string title, extension;
    bool file = false;
    bool (*callback)(bool, std::string) = nullptr;
};
struct Result { bool failed = false, cancelled = true; std::string path; };
Request retry;
std::string lastFolder;
ImGuiID fallback = 0;
Result choose(const Request& request) {
    Result result;
    if (NFD_Init() != NFD_OKAY) { result.failed = true; return result; }
    nfdu8char_t* path = nullptr;
    nfdresult_t status;
    if (request.file) {
        nfdu8filteritem_t filter{request.extension.c_str(), request.extension.c_str()};
        nfdopendialogu8args_t args{};
        args.title = request.title.c_str();
        args.defaultPath = lastFolder.empty() ? nullptr : lastFolder.c_str();
        if (!request.extension.empty()) { args.filterList = &filter; args.filterCount = 1; }
        status = NFD_OpenDialogU8_With(&path, &args);
    } else {
        nfdpickfolderu8args_t args{};
        args.title = request.title.c_str();
        args.defaultPath = lastFolder.empty() ? nullptr : lastFolder.c_str();
        status = NFD_PickFolderU8_With(&path, &args);
    }
    result.failed = status == NFD_ERROR;
    result.cancelled = status != NFD_OKAY;
    if (path) { result.path = path; NFD_FreePathU8(path); }
    NFD_Quit();
    // Discard mouse/key presses queued while the native modal owned input.
    ImGui::GetIO().ClearEventsQueue();
    ImGui::GetIO().ClearInputKeys();
    ImGui::GetIO().ClearInputMouse();
    return result;
}
void complete(const Request& request, const Result& result) {
    if (!result.cancelled)
        lastFolder = request.file ? hostfs::storage().getParentPath(result.path) : result.path;
    if (request.callback && !request.callback(result.cancelled, result.path) && !result.cancelled)
        retry = request;
}
}
bool platformPickerActive() { return retry.callback != nullptr; }
void shutdownPlatformPicker() { retry = {}; fallback = 0; }
void pollPlatformPicker() {
    if (!retry.callback || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) return;
    Request request = retry;
    retry = {};
    complete(request, choose(request));
}
bool platformPicker(const char* prompt, bool (*callback)(bool, std::string), bool file, const std::string& extension) {
    ImGuiID id = ImGui::GetID(prompt);
    if (!ImGui::IsPopupOpen(prompt)) { if (fallback == id) fallback = 0; return true; }
    if (fallback == id) return false;
    if (platformPickerActive()) return true;
    std::string title = prompt;
    title = title.substr(0, title.find("##"));
    Request request{tr(title.c_str()), extension, file, callback};
    auto result = choose(request);
    // A minimal/headless Linux desktop may not provide a portal. Keep the
    // existing popup and fall back to Clarity's browser without losing paths.
    if (result.failed) { fallback = id; return false; }
    ImGui::ClosePopupToLevel(GImGui->BeginPopupStack.Size, false);
    complete(request, result);
    return true;
}
}
