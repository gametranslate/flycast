// SPDX-License-Identifier: GPL-2.0-or-later
// Uses gui.cpp's existing game, disc and savestate lifecycle callbacks.
static void clarity_commands() {
    using namespace clarity;
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    const bool dark = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg).x < .3f;
    fullScreenWindow(false);
    ImGui::SetNextWindowBgAlpha(.84f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::Begin("##pause-shade", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
    ImGui::End();
    ImGui::PopStyleVar();

    ImVec2 size(std::min(uiScaled(1030), screen.x - uiScaled(32)), std::min(uiScaled(660), screen.y - uiScaled(32)));
    ImGui::SetNextWindowPos(screen * .5f, ImGuiCond_Always, ImVec2(.5f,.5f));
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ScaledVec2(28,24));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, uiScaled(18));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_ChildBg));
    ImGui::Begin("##clarity-pause", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    muted(tr("Quick menu"));
    ImGui::Spacing();
    GameMedia media;
    media.path = settings.content.path; media.fileName = settings.content.fileName;
    auto art = boxart.getBoxart(media);
    ImguiFileTexture cover(art.boxartPath);
    if (auto texture = cover.getId()) ImGui::Image(texture, ScaledVec2(64,64));
    else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        ImGui::Button(ICON_FA_COMPACT_DISC "##game-art", ScaledVec2(64,64));
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    ImGui::BeginGroup();
    const std::string title = settings.content.title.empty() ? settings.content.fileName : settings.content.title;
    ImGui::PushFont(nullptr, uiScaled(25));
    ImGui::TextWrapped("%s", title.empty() ? "Flycast" : title.c_str());
    ImGui::PopFont();
    ImGui::TextColored(accent(), ICON_FA_CIRCLE_PAUSE "  %s", tr("Game paused"));
    ImGui::EndGroup();
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    ImGui::BeginChild("quick-scroll", ImVec2(0,-uiScaled(55)), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
#ifdef FLYCAST_CLARITY_SMOKE_TEST
    smokeQuickScroll();
#endif
    bool wide = ImGui::GetContentRegionAvail().x >= uiScaled(760);
    ImGui::BeginTable("quick-layout", wide ? 2 : 1, ImGuiTableFlags_SizingStretchProp);
    if (wide) {
        ImGui::TableSetupColumn("actions", ImGuiTableColumnFlags_WidthStretch, 1.25f);
        ImGui::TableSetupColumn("saves", ImGuiTableColumnFlags_WidthStretch, 1.f);
    }
    ImGui::TableNextColumn();
    auto action = [](const char* icon, const char* label, float height = 52.f) {
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0,.5f));
        bool clicked = ImGui::Button((std::string(icon) + "  " + tr(label)).c_str(), ImVec2(-1,uiScaled(height)));
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s",tr(label));
        return clicked;
    };
    ImGui::PushStyleColor(ImGuiCol_Button, accent());
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, dark ? ImVec4(1,.72f,.56f,1) : ImVec4(.62f,.16f,.04f,1));
    ImGui::PushStyleColor(ImGuiCol_Text, dark ? ImVec4(.10f,.13f,.18f,1) : ImVec4(1,1,1,1));
    if (action(ICON_FA_PLAY, "Resume", 64)) {
        GamepadDevice::load_system_mappings(); gui_setState(GuiState::Closed);
    }
    ImGui::PopStyleColor(3);
    ImGui::Spacing();
    muted(tr("Controls & tools"));
    float minTile = uiScaled(180);
    for (const char* label : {"Button mapping", "Settings", "Cheats", "Achievements"})
        minTile = std::max(minTile, ImGui::CalcTextSize(tr(label)).x + uiScaled(58));
    int columns = ImGui::GetContentRegionAvail().x > minTile*2 + uiScaled(20) ? 2 : 1;
    ImGui::BeginTable("quick-tools", columns, ImGuiTableFlags_SizingStretchSame);
    ImGui::TableNextColumn();
    if (action(ICON_FA_GAMEPAD,"Button mapping",64)) openSettings(Page::Controllers);
    ImGui::TableNextColumn();
    if (action(ICON_FA_GEAR,"Settings",64)) openSettings(Page::System);
    ImGui::TableNextColumn();
    { DisabledScope lock(settings.network.online || settings.raHardcoreMode);
      if (action(ICON_FA_WAND_MAGIC_SPARKLES,"Cheats",64)) gui_setState(GuiState::Cheats); }
    ImGui::TableNextColumn();
    { DisabledScope lock(!achievements::isActive());
      if (action(ICON_FA_TROPHY,"Achievements",64)) gui_setState(GuiState::Achievements); }
    ImGui::EndTable();
    if (action(ICON_FA_COMPACT_DISC, gdr::isOpen() ? "Insert Disk" : "Eject Disk")) {
        if (gdr::isOpen()) gui_setState(GuiState::SelectDisk);
        else { emu.openGdrom(); gui_setState(GuiState::Closed); }
    }
    if (card_reader::barcodeAvailable()) {
        char value[64]{}; strncpy(value, card_reader::barcodeGetCard().c_str(), sizeof(value)-1);
        ImGui::SetNextItemWidth(-1);
        if (InputText(tr("Barcode Card"), value, sizeof(value))) card_reader::barcodeSetCard(value);
    }

    ImGui::TableNextColumn();
    ImGui::BeginChild("save-card", ImVec2(0,0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    ImGui::TextUnformatted(tr("Save states"));
    bool allowed = dc_savestateAllowed();
    { DisabledScope lock(!allowed);
      if (ImGui::ArrowButton("##prev-slot", ImGuiDir_Left)) cycleSaveStateSlot(-1);
      ImGui::SameLine(); ImGui::Text(tr("Slot %d"),(int)config::SavestateSlot+1);
      ImGui::SameLine(); if (ImGui::ArrowButton("##next-slot",ImGuiDir_Right)) cycleSaveStateSlot(1);
    }
    time_t date = dc_getStateCreationDate(config::SavestateSlot);
    ImguiStateTexture picture;
    float width = ImGui::GetContentRegionAvail().x;
    if (auto texture = picture.getId()) {
        // Keep actions visible when a screenshot is present, preserving its
        // native aspect ratio inside a stable preview area.
        float previewWidth = std::min(width, uiScaled(240));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width-previewWidth)*.5f);
        picture.draw(ImVec2(previewWidth, previewWidth*.75f));
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        ImGui::BeginChild("save-preview", ImVec2(0,uiScaled(158)), ImGuiChildFlags_Borders);
        ImGui::SetCursorPosY(uiScaled(45));
        const char* label = tr("No saved state yet");
        ImGui::SetCursorPosX(std::max(uiScaled(12), (width-ImGui::CalcTextSize(ICON_FA_FLOPPY_DISK).x)*.5f));
        ImGui::TextColored(accent(), ICON_FA_FLOPPY_DISK);
        ImGui::SetCursorPosX(std::max(uiScaled(12),(width-ImGui::CalcTextSize(label).x)*.5f));
        muted(label);
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
    if (date) muted(timeToShortDateTimeString(date).c_str());
    { DisabledScope lock(!allowed);
      if (action(ICON_FA_FLOPPY_DISK,"Save State",42)) { gui_setState(GuiState::Closed); savestate(); }
      DisabledScope unavailable(settings.raHardcoreMode || !date);
      if (action(ICON_FA_CLOCK_ROTATE_LEFT,"Load State",42)) { gui_setState(GuiState::Closed); dc_loadstate(config::SavestateSlot); }
    }
    ImGui::EndChild();
    static std::array<ImguiVmuTexture,8> vmus = {ImguiVmuTexture(0),ImguiVmuTexture(1),ImguiVmuTexture(2),ImguiVmuTexture(3),
        ImguiVmuTexture(4),ImguiVmuTexture(5),ImguiVmuTexture(6),ImguiVmuTexture(7)};
    for (auto& vmu : vmus) if (auto texture=vmu.getId()) ImGui::Image(texture,ScaledVec2(144,96),ImVec2(0,1),ImVec2(1,0));
    ImGui::EndTable();
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    if (ImGui::Button((std::string(ICON_FA_POWER_OFF "  ") + tr(commandLineStart ? "Exit" : "Close Game")).c_str())) gui_stop_game();
    ImGui::PopStyleColor();
    ImGui::End();
}
