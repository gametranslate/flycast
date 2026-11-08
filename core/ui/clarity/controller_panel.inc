// SPDX-License-Identifier: GPL-2.0-or-later
// Included by settings_controls.cpp. Reuses upstream input detection, mapping
// tables, combo semantics and device names without copying the input backend.
#include "clarity/clarity.h"
namespace clarity {
namespace {
int mappingSystem = DC_PLATFORM_DREAMCAST;
int mappingGroup = 1; // Face buttons first; directions, triggers and hotkeys one click away.
std::weak_ptr<GamepadDevice> selectedController;

void selectController(const std::shared_ptr<GamepadDevice>& device) {
    closeMappings();
    selectedController = device;
    if (!device || !device->remappable()) return;
    mappingSystem = game_started && settings.platform.isArcade() ? DC_PLATFORM_NAOMI : DC_PLATFORM_DREAMCAST;
    arcade_button_mode = mappingSystem == DC_PLATFORM_NAOMI;
    gamepad_port = 0;
    if (!device->find_mapping(mappingSystem))
        if (mappingSystem == DC_PLATFORM_DREAMCAST || !device->find_mapping(DC_PLATFORM_DREAMCAST))
            device->resetMappingToDefault(arcade_button_mode, true);
    currentGamepad = device;
    device->listenButtons(buttonListener);
}
void controllerDiagram(const std::shared_ptr<GamepadDevice>& p) {
    ImGui::BeginChild("controller-visual", ImVec2(0, uiScaled(130)), ImGuiChildFlags_Borders);
    float scale = std::min(uiScaled(1), ImGui::GetContentRegionAvail().x / 460.f);
    ImVec2 a = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 ink = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    ImU32 body = ImGui::GetColorU32(ImGuiCol_FrameBg);
    ImU32 active = ImGui::ColorConvertFloat4ToU32(accent());
    dl->AddRectFilled(a + ImVec2(0, 12) * scale, a + ImVec2(285, 95) * scale, body, 28 * scale);
    dl->AddRect(a + ImVec2(0, 12) * scale, a + ImVec2(285, 95) * scale, ink, 28 * scale);
    dl->AddRectFilled(a + ImVec2(42, 29) * scale, a + ImVec2(57, 78) * scale, ink, 2 * scale);
    dl->AddRectFilled(a + ImVec2(25, 46) * scale, a + ImVec2(74, 61) * scale, ink, 2 * scale);
    dl->AddCircle(a + ImVec2(110, 67) * scale, 17 * scale, ink, 24, 2 * scale);
    const DreamcastKey keys[] = {DC_BTN_Y, DC_BTN_X, DC_BTN_B, DC_BTN_A};
    const char* labels[] = {"Y", "X", "B", "A"};
    const ImVec2 positions[] = {{230,32},{208,54},{252,54},{230,76}};
    for (int i=0; i<4; ++i) {
        ImVec2 center = a + positions[i] * scale;
        bool pressed = getButtonState(p, keys[i]);
        dl->AddCircleFilled(center, 12*scale, pressed ? active : ImGui::GetColorU32(ImGuiCol_ChildBg), 24);
        dl->AddCircle(center, 12*scale, ink, 24);
        ImVec2 t = ImGui::CalcTextSize(labels[i]);
        dl->AddText(center - t * .5f, ImGui::GetColorU32(ImGuiCol_Text), labels[i]);
    }
    if (ImGui::GetContentRegionAvail().x > uiScaled(430)) {
        ImGui::SetCursorPosX(uiScaled(320));
        ImGui::TextColored(accent(), ICON_FA_CIRCLE "  %s", tr("Live input"));
        ImGui::SetCursorPosX(uiScaled(320));
        ImGui::TextWrapped("%s", tr("Press a button to test it."));
    }
    ImGui::EndChild();
}
}
void closeMappings() {
    if (currentGamepad) {
        currentGamepad->cancel_detect_input();
        currentGamepad->save_mapping(mappingSystem);
        currentGamepad->unlistenButtons(buttonListener);
        currentGamepad.reset();
    }
    buttonState.clear();
    mapped_codes.clear();
}
void saveMappings() {
    if (currentGamepad) currentGamepad->save_mapping(mappingSystem);
}
#ifdef FLYCAST_CLARITY_SMOKE_TEST
void smokeUseTestController() {
    for (int i=0; i<GamepadDevice::GetGamepadCount(); ++i) {
        auto p = GamepadDevice::GetGamepad(i);
        if (p && p->api_name() == "ClarityRegression") { selectController(p); return; }
    }
}
#endif
void controllerPage() {
    uiLanguageHandler.init();
    auto p = selectedController.lock();
    if (p && !p->is_registered()) { closeMappings(); selectedController.reset(); p.reset(); }
    if (!p) {
        // Prefer a physical gamepad, while keeping keyboards and mice remappable.
        for (int i = 0; i < GamepadDevice::GetGamepadCount(); ++i) {
            auto candidate = GamepadDevice::GetGamepad(i);
            if (candidate && candidate->remappable()) {
                if (!p || candidate->has_analog_stick()) p = candidate;
            }
        }
        if (p) selectController(p);
    } else if (p->remappable() && currentGamepad != p) selectController(p);
    section("Input device");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##device", p ? p->name().c_str() : tr("noDevices"))) {
        for (int i = 0; i < GamepadDevice::GetGamepadCount(); ++i) {
            auto candidate = GamepadDevice::GetGamepad(i);
            if (!candidate) continue;
            ImGui::PushID(candidate.get());
            if (ImGui::Selectable((candidate->name() + " · " + candidate->api_name()).c_str(), candidate == p)) {
                selectController(candidate); p = candidate;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (!p) { muted(tr("Connect a controller or use your keyboard to configure buttons.")); return; }
    ImGui::TextDisabled("%s", p->api_name().c_str());
#if defined(USE_DREAMLINK_DEVICES)
    if (auto dreamLink = dynamic_cast<DreamLinkGamepad*>(p.get())) ImGui::Text("DreamLink: %s", dreamLink->dreamLinkStatus());
#endif
    beginRow(tr("assignment"), tr("Assign this device to a Dreamcast player."));
    int port = std::clamp(p->maple_port() + 1, 0, 5);
    if (ImGui::BeginCombo("##port", maple_ports[port])) {
        for (int j = -1; j < (int)std::size(maple_ports) - 1; ++j)
            if (ImGui::Selectable(maple_ports[j+1], p->maple_port() == j)) p->set_maple_port(j);
        ImGui::EndCombo();
    }
    endRow();
    if (p->is_rumble_enabled() || p->has_analog_stick() || p->is_virtual_gamepad()) {
        if (button("Rumble & sticks")) ImGui::OpenPopup(T("Gamepad Settings"));
        gamepadSettingsPopup(p);
    }
#if defined(__ANDROID__) || defined(TARGET_IPHONE)
    if (p->is_virtual_gamepad() && button("Edit Layout")) { vgamepad::startEditing(); gui_setState(GuiState::VJoyEdit); }
#endif
    if (!p->remappable() || !p->get_input_mapping()) { muted(tr("This device does not expose remappable controls.")); return; }
    if (ImGui::CollapsingHeader(tr("Live input"))) controllerDiagram(p);
    bool compact = ImGui::GetContentRegionAvail().x < uiScaled(620);
    ImGui::SetNextItemWidth(compact ? -1 : uiScaled(220));
    if (ImGui::BeginCombo("##system", tr(arcade_button_mode ? "Arcade Controls" : "Dreamcast Controls"))) {
        for (int i=0; i<2; ++i) {
            if (ImGui::Selectable(tr(i ? "Arcade Controls" : "Dreamcast Controls"), arcade_button_mode == (bool)i) && arcade_button_mode != (bool)i) {
                p->save_mapping(mappingSystem);
                mappingSystem = i ? DC_PLATFORM_NAOMI : DC_PLATFORM_DREAMCAST;
                arcade_button_mode = i != 0;
                if (!p->find_mapping(mappingSystem))
                    if (mappingSystem == DC_PLATFORM_DREAMCAST || !p->find_mapping(DC_PLATFORM_DREAMCAST))
                        p->resetMappingToDefault(arcade_button_mode, true);
                buttonState.clear();
            }
        }
        ImGui::EndCombo();
    }
    if (!compact) ImGui::SameLine();
    if (button("Reset mappings")) ImGui::OpenPopup("##reset-mappings");
    if (ImGui::BeginPopupModal("##reset-mappings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", tr("Are you sure you want to reset the mappings to default?"));
        static bool hitbox = false;
        if (arcade_button_mode) {
            if (ImGui::RadioButton(tr("Gamepad"), !hitbox)) hitbox = false;
            if (ImGui::RadioButton(tr("Arcade / Hit Box"), hitbox)) hitbox = true;
        }
        if (button("Reset mappings")) {
            p->resetMappingToDefault(arcade_button_mode, !hitbox);
            p->save_mapping(mappingSystem);
            buttonState.clear(); ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (p->maple_port() == MAPLE_PORTS) {
        ImGui::SetNextItemWidth(uiScaled(140));
        if (ImGui::BeginCombo(tr("Port"), maple_ports[gamepad_port+1])) {
            for (u32 i=0; i<MAPLE_PORTS; ++i)
                if (ImGui::Selectable(maple_ports[i+1], gamepad_port==i)) { gamepad_port=i; buttonState.clear(); }
            ImGui::EndCombo();
        }
    }
    if (!settings.content.gameId.empty()) {
        if (button(p->isPerGameMapping() ? "Delete Game Config" : "Make Game Config")) {
            if (p->isPerGameMapping()) ImGui::OpenPopup("##delete-mapping");
            else p->setPerGameMapping(true);
        }
        if (ImGui::BeginPopupModal("##delete-mapping", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", tr("Delete this game's button mapping?"));
            if (button("Delete Game Config")) {
                p->setPerGameMapping(false);
                if (!p->find_mapping(mappingSystem)) p->resetMappingToDefault(arcade_button_mode, true);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    const char* groups[] = {"Directions", "Buttons", "Triggers", "System Buttons", "Emulator", "All controls"};
    const char* icons[] = {ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, ICON_FA_GAMEPAD, ICON_FA_HAND_POINTER, ICON_FA_POWER_OFF, ICON_FA_KEYBOARD, ICON_FA_LIST};
    int navColumns = compact ? 2 : 3;
    ImGui::BeginTable("mapping-groups", navColumns, ImGuiTableFlags_SizingStretchSame);
    for (int i=0; i<6; ++i) {
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Button, mappingGroup == i ? ImGui::GetStyleColorVec4(ImGuiCol_Header) : ImGui::GetStyleColorVec4(ImGuiCol_Button));
        if (ImGui::Button((std::string(icons[i]) + " " + tr(groups[i])).c_str(), ImVec2(-1,0))) mappingGroup = i;
        ImGui::PopStyleColor();
    }
    ImGui::EndTable();
    const Mapping* entries = arcade_button_mode ? arcadeButtons : dcButtons;
    int group = -1;
    for (auto m = entries; m->name; ++m) {
        if (m->key == EMU_BTN_NONE) {
            // Arcade tables have the same named categories but may omit Triggers.
            group = -1;
            for (int i=0; i<5; ++i) if (std::string(m->name) == T(groups[i])) group = i;
            if (mappingGroup == 5 || group == mappingGroup) section(m->name);
            continue;
        }
        if (mappingGroup != 5 && group != mappingGroup) continue;
        ImGui::PushID((int)m->key);
        ImGui::BeginChild("binding", ImVec2(0,0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
        bool narrowRow = ImGui::GetContentRegionAvail().x < uiScaled(570);
        ImGui::BeginTable("binding-row", narrowRow ? 1 : 3, ImGuiTableFlags_SizingStretchProp);
        if (!narrowRow) {
            ImGui::TableSetupColumn("function", ImGuiTableColumnFlags_WidthStretch, 1);
            ImGui::TableSetupColumn("binding", ImGuiTableColumnFlags_WidthStretch, 1);
            ImGui::TableSetupColumn("actions", ImGuiTableColumnFlags_WidthFixed,
                ImGui::CalcTextSize(tr("Map button")).x + ImGui::CalcTextSize(tr("Unmap")).x + uiScaled(62));
        }
        ImGui::TableNextColumn();
        std::string label = m->name;
        if (arcade_button_mode) {
            const char* gameName = GetCurrentGameButtonName(m->key);
            if (!gameName) gameName = GetCurrentGameAxisName(m->key);
            if (gameName && *gameName) label += std::string(" · ") + gameName;
        }
        ImGui::TextColored(getButtonState(p, m->key) ? accent() : ImGui::GetStyleColorVec4(ImGuiCol_Text), "%s", label.c_str());
        ImGui::TableNextColumn();
        displayMappedControl(p, m->key);
        if (((m->key & DC_BTN_GROUP_MASK) == DC_AXIS_STICKS || (m->key & DC_BTN_GROUP_MASK) == DC_AXIS_TRIGGERS)
                && p->has_analog_stick()) {
            float value = getAxisValue(p, m->key);
            ImGui::ProgressBar(value, ImVec2(-1, uiScaled(6)), "");
        }
        ImGui::TableNextColumn();
        if (button("Map button", true)) {
            map_start_time = getTimeMs() + 300;
            mapped_codes.clear();
            buttonState.erase(m->key);
            p->detectInput((m->key & DC_BTN_GROUP_MASK) == EMU_BUTTONS, [](u32 code, bool analog, bool positive) {
                mapped_codes.insert_back(analog ? InputMapping::InputDef::from_axis(code, positive) : InputMapping::InputDef::from_button(code));
            });
            ImGui::OpenPopup(T("Map Control"));
        }
#ifdef FLYCAST_CLARITY_SMOKE_TEST
        if (m->key == DC_BTN_A) smokeObserve("map-A");
#endif
        detect_input_popup(m);
        ImGui::SameLine();
        if (button("Unmap")) unmapControl(p, gamepad_port, m->key);
        ImGui::EndTable();
        ImGui::EndChild();
        ImGui::PopID();
    }
    section("Mouse & keyboard");
    setCards(true);
    OptionSlider(tr("Mouse sensitivity"), config::MouseSensitivity, 1, 500);
#if defined(_WIN32) && !defined(TARGET_UWP)
    OptionCheckbox(tr("Use Raw Input"), config::UseRawInput, tr("Supports multiple pointing devices (mice, light guns) and keyboards"));
#endif
    setCards(false);
}

void devicesPage(std::array<bool,4>& maple, std::array<std::array<bool,2>,4>& expansion) {
    uiLanguageHandler.init();
    bool crosshair = false;
    for (int bus=0; bus<MAPLE_PORTS; ++bus) {
        ImGui::PushID(bus);
        ImGui::BeginChild("port", ImVec2(0,0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
        ImGui::TextColored(accent(), ICON_FA_PLUG "  %s %c", tr("Port"), 'A'+bus);
        beginRow(tr("Dreamcast Devices"), nullptr, config::MapleMainDevices[bus].isReadOnly());
        if (ImGui::BeginCombo("##device", maple_device_name(config::MapleMainDevices[bus]))) {
            for (int i=0; i<IM_ARRAYSIZE(maple_device_types); ++i)
                if (ImGui::Selectable(maple_device_types[i], config::MapleMainDevices[bus] == maple_device_type_from_index(i))) {
                    config::MapleMainDevices[bus] = maple_device_type_from_index(i);
                    maple[bus] = true;
                }
            ImGui::EndCombo();
        }
        endRow();
        for (int slot=0; slot<maple_getPortCount(config::MapleMainDevices[bus]); ++slot) {
            ImGui::PushID(slot);
            std::string label = std::string(tr("Accessory slot")) + " " + std::to_string(slot+1);
            beginRow(label.c_str(), nullptr, config::MapleExpansionDevices[bus][slot].isReadOnly() || config::NetworkExpansionDevices[bus][slot].isReadOnly());
            int type = config::MapleExpansionDevices[bus][slot];
            if (type == MDT_SegaVMU && config::NetworkExpansionDevices[bus][slot] == 1) type = MDT_DreamPotato;
            if (ImGui::BeginCombo("##accessory", maple_expansion_device_name((MapleDeviceType)type))) {
                for (int i=0; i<IM_ARRAYSIZE(maple_expansion_device_types); ++i)
                    if (ImGui::Selectable(maple_expansion_device_types[i], type == maple_expansion_device_type_from_index(i))) {
                        auto value = maple_expansion_device_type_from_index(i);
                        config::MapleExpansionDevices[bus][slot] = (int)value == MDT_DreamPotato ? MDT_SegaVMU : value;
                        config::NetworkExpansionDevices[bus][slot] = (int)value == MDT_DreamPotato ? 1 : 0;
                        expansion[bus][slot] = true;
                    }
                ImGui::EndCombo();
            }
            endRow(); ImGui::PopID();
        }
        if (config::MapleMainDevices[bus] == MDT_LightGun) {
            u32 color = config::CrosshairColor[bus];
            float rgba[4]{(color & 255)/255.f, ((color>>8)&255)/255.f, ((color>>16)&255)/255.f, ((color>>24)&255)/255.f};
            bool enabled = color != 0;
            beginRow(tr("Crosshair"), nullptr, config::CrosshairColor[bus].isReadOnly());
            bool changed = ImGui::Checkbox("##enabled", &enabled);
            ImGui::SameLine();
            changed |= ImGui::ColorEdit4("##color", rgba, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
            if (changed) {
                u32 value = (u8)std::round(rgba[0]*255) | ((u8)std::round(rgba[1]*255)<<8) | ((u8)std::round(rgba[2]*255)<<16) | ((u8)std::round(rgba[3]*255)<<24);
                config::CrosshairColor[bus] = enabled ? value ? value : 0xC0FFFFFF : 0;
            }
            crosshair |= enabled;
            endRow();
        }
        ImGui::EndChild(); ImGui::PopID();
    }
    section("Memory & display");
    OptionCheckbox(tr("Per Game VMU A1"), config::PerGameVmu, tr("When enabled, each game has its own VMU on port 1 of controller A."));
    { DisabledScope lock(game_started);
      OptionCheckbox(tr("Use Physical VMU Storage"), config::UsePhysicalVmuMemory, tr("Enables read and write access to physical VMU storage via DreamPicoPort or DreamPotato. This is not compatible with load state events.")); }
    OptionCheckbox(tr("Show VMU In-game"), config::FloatVMUs, tr("Show the VMU LCD screens while in-game"));
    { DisabledScope lock(!crosshair); OptionSlider(tr("Crosshair Size"), config::CrosshairSize, 10, 100); }
}
}
