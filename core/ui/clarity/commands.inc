// SPDX-License-Identifier: GPL-2.0-or-later
// Kept in gui.cpp's translation unit so these buttons use the same lifecycle,
// savestate and disc callbacks as the original menu.
static void clarity_commands() {
    using namespace clarity;
    fullScreenWindow(false);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::Begin("##clarity-pause", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    muted("FLYCAST / CLARITY");
    heading("Game paused", settings.content.title.empty() ? settings.content.fileName.c_str() : settings.content.title.c_str());
    bool wide = ImGui::GetContentRegionAvail().x > uiScaled(700);
    ImGui::BeginTable("pause-layout", wide ? 2 : 1, ImGuiTableFlags_SizingStretchSame);
    ImGui::TableNextColumn();
    auto action = [](const char* icon, const char* label) {
        return ImGui::Button((std::string(icon) + "  " + clarity::tr(label)).c_str(), ImVec2(-1, uiScaled(48)));
    };
    section("Continue playing");
    if (action(ICON_FA_PLAY, "Resume")) {
        GamepadDevice::load_system_mappings();
        gui_setState(GuiState::Closed);
    }
    if (action(ICON_FA_GAMEPAD, "Button mapping")) openSettings(Page::Controllers);
    if (action(ICON_FA_GEAR, "Settings")) openSettings(Page::System);
    if (action(ICON_FA_COMPACT_DISC, gdr::isOpen() ? "Insert Disk" : "Eject Disk")) {
        if (gdr::isOpen()) gui_setState(GuiState::SelectDisk);
        else { emu.openGdrom(); gui_setState(GuiState::Closed); }
    }
    { DisabledScope lock(settings.network.online || settings.raHardcoreMode);
      if (action(ICON_FA_WAND_MAGIC_SPARKLES, "Cheats")) gui_setState(GuiState::Cheats); }
    { DisabledScope lock(!achievements::isActive());
      if (action(ICON_FA_TROPHY, "Achievements")) gui_setState(GuiState::Achievements); }
    if (card_reader::barcodeAvailable()) {
        char cardBuf[64]{};
        strncpy(cardBuf, card_reader::barcodeGetCard().c_str(), sizeof(cardBuf) - 1);
        ImGui::SetNextItemWidth(-1);
        if (InputText(tr("Barcode Card"), cardBuf, sizeof(cardBuf))) card_reader::barcodeSetCard(cardBuf);
    }
    if (action(ICON_FA_POWER_OFF, commandLineStart ? "Exit" : "Close Game")) gui_stop_game();
    ImGui::TableNextColumn();
    section("Save states");
    { DisabledScope lock(!dc_savestateAllowed());
      time_t date = dc_getStateCreationDate(config::SavestateSlot);
      ImguiStateTexture picture;
      picture.draw(ImVec2(std::min(uiScaled(340), ImGui::GetContentRegionAvail().x), 0));
      if (date) muted(timeToShortDateTimeString(date).c_str()); else muted(tr("Empty"));
      if (ImGui::ArrowButton("##prev-slot", ImGuiDir_Left)) cycleSaveStateSlot(-1);
      ImGui::SameLine(); ImGui::Text(tr("Slot %d"), (int)config::SavestateSlot + 1);
      ImGui::SameLine(); if (ImGui::ArrowButton("##next-slot", ImGuiDir_Right)) cycleSaveStateSlot(1);
      if (action(ICON_FA_FLOPPY_DISK, "Save State")) { gui_setState(GuiState::Closed); savestate(); }
      { DisabledScope unavailable(settings.raHardcoreMode || !date);
        if (action(ICON_FA_CLOCK_ROTATE_LEFT, "Load State")) { gui_setState(GuiState::Closed); dc_loadstate(config::SavestateSlot); } }
    }
    // Keep the active VMU LCDs available while paused, within the scrolling
    // layout so they do not cover controls in a small window.
    static std::array<ImguiVmuTexture, 8> vmus = {
        ImguiVmuTexture(0), ImguiVmuTexture(1), ImguiVmuTexture(2), ImguiVmuTexture(3),
        ImguiVmuTexture(4), ImguiVmuTexture(5), ImguiVmuTexture(6), ImguiVmuTexture(7)
    };
    bool firstVmu = true;
    for (auto& vmu : vmus) if (auto texture = vmu.getId()) {
        if (firstVmu) { section("VMU"); firstVmu = false; }
        ImGui::Image(texture, ScaledVec2(144, 96), ImVec2(0, 1), ImVec2(1, 0));
    }
    ImGui::EndTable();
    ImGui::End();
}
