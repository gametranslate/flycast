// SPDX-License-Identifier: GPL-2.0-or-later
// Uses the same hostfs paths and callback contract as the upstream picker.
static void clarity_file_picker(const char* prompt, StringCallback callback,
        bool selectFile, const std::string& extension) {
    using namespace clarity;
    centerNextWindow();
    ImVec2 screen = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(std::min(uiScaled(1000), screen.x - uiScaled(24)),
        std::max(uiScaled(280), screen.y - uiScaled(40))));
    if (!ImGui::BeginPopupModal(prompt, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        return;
    static std::string lastPrompt, error, enteredPath;
    static bool lastSelectFile = false;
    static std::string lastExtension;
    if (lastPrompt != prompt || lastSelectFile != selectFile || lastExtension != extension) {
        subfolders_read = false;
        lastPrompt = prompt; lastSelectFile = selectFile; lastExtension = extension;
    }
    if (select_current_directory == "**home**") select_current_directory = hostfs::storage().getDefaultDirectory();
    if (!subfolders_read) {
        subfolders.clear(); folderFiles.clear(); error.clear();
        enteredPath = select_current_directory;
        try {
            for (const auto& entry : hostfs::storage().listContent(select_current_directory)) {
                if (entry.isDirectory) subfolders.push_back(entry);
                else if (selectFile && (extension.empty() || get_file_extension(entry.name) == extension)) folderFiles.push_back(entry);
            }
            std::sort(subfolders.begin(), subfolders.end());
            std::sort(folderFiles.begin(), folderFiles.end());
        } catch (const hostfs::StorageException& e) { error = e.what(); }
        subfolders_read = true;
    }
    std::string title = prompt ? prompt : "";
    title = title.substr(0, title.find("##"));
    heading(title.c_str());
    bool cancelled = button("Cancel");
    ImGui::SameLine();
    if (button(".. Up to Parent Folder")) {
        select_current_directory = hostfs::storage().getParentPath(select_current_directory);
        subfolders_read = false;
    }
    ImGui::SameLine();
    if (button("Storage")) { select_current_directory.clear(); subfolders_read = false; }
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##location", &enteredPath, ImGuiInputTextFlags_EnterReturnsTrue)) {
        select_current_directory = enteredPath;
        subfolders_read = false;
    }
    if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
    ImGui::BeginChild("files", ImVec2(0, -uiScaled(58)), ImGuiChildFlags_Borders);
    std::string selectedFile;
    for (const auto& folder : subfolders) {
        if (ImGui::Selectable((std::string(ICON_FA_FOLDER "  ") + folder.name).c_str(), false, 0, ImVec2(0,uiScaled(32)))) {
            select_current_directory = folder.path; subfolders_read = false;
        }
    }
    for (const auto& file : folderFiles) {
        if (ImGui::Selectable((std::string(ICON_FA_FILE "  ") + file.name).c_str(), false, 0, ImVec2(0,uiScaled(32))))
            selectedFile = file.path;
#ifdef FLYCAST_CLARITY_SMOKE_TEST
        smokeObserve(("file:" + file.name).c_str());
#endif
    }
    ImGui::EndChild();
    if (!selectFile) {
        ImGui::BeginDisabled(select_current_directory.empty());
        if (button("Select Current Folder", true) && callback(false, select_current_directory)) {
            subfolders_read = false; ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
    }
    if (!selectedFile.empty() && callback(false, selectedFile)) { subfolders_read = false; ImGui::CloseCurrentPopup(); }
    if (cancelled) { subfolders_read = false; callback(true, ""); ImGui::CloseCurrentPopup(); }
    error_popup();
    ImGui::EndPopup();
}
