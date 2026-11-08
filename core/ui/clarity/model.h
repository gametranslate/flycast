// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "draft.h"
#include <algorithm>
#include "cfg/option.h"
#include "input/gamepad_device.h"

namespace clarity {
struct DeviceDraft {
    std::weak_ptr<GamepadDevice> device;
    int port, originalPort;
    float deadZone, originalDeadZone;
    bool changed() const { return port != originalPort || deadZone != originalDeadZone; }
};

struct Model {
    Draft fields;
    int resolution, volume, bufferSize;
    RenderType renderer;
    bool widescreen, vsync, perStrip, framebuffer, dsp, vmuSound, vmuLcd, autoSave, autoLoad;
    std::vector<std::string> paths;
    std::vector<DeviceDraft> devices;
    bool createGameProfile = false;

    Model() {
        fields.bind(resolution, config::RenderResolution);
        fields.bind(renderer, config::RendererType);
        fields.bind(widescreen, config::Widescreen);
        fields.bind(vsync, config::VSync);
        fields.bind(perStrip, config::PerStripSorting);
        fields.bind(framebuffer, config::EmulateFramebuffer);
        fields.bind(volume, config::AudioVolume);
        fields.bind(bufferSize, config::AudioBufferSize);
        fields.bind(dsp, config::DSPEnabled);
        fields.bind(vmuSound, config::VmuSound);
        fields.bind(vmuLcd, config::FloatVMUs);
        fields.bind(paths, config::ContentPath);
        fields.bind(autoSave, config::AutoSaveState);
        fields.bind(autoLoad, config::AutoLoadState);
    }
    void capture() {
        fields.capture();
        createGameProfile = false;
        devices.clear();
        syncDevices();
    }
    void syncDevices() {
        devices.erase(std::remove_if(devices.begin(), devices.end(), [](const DeviceDraft& d) {
            auto p = d.device.lock();
            return !p || !p->is_registered();
        }), devices.end());
        for (int i = 0; i < GamepadDevice::GetGamepadCount(); ++i) {
            auto p = GamepadDevice::GetGamepad(i);
            if (!p || std::any_of(devices.begin(), devices.end(), [&](const DeviceDraft& d) { return d.device.lock() == p; }))
                continue;
            float deadZone = p->get_input_mapping() ? p->get_dead_zone() : 0;
            devices.push_back({p, p->maple_port(), p->maple_port(), deadZone, deadZone});
        }
    }
    int count() const {
        int n = fields.count() + createGameProfile;
        for (const auto& d : devices) n += d.changed();
        return n;
    }
    template<typename Option>
    bool locked(const Option& option) const {
        return option.isReadOnly() && !createGameProfile;
    }
    void commit() {
        if (createGameProfile)
            config::Settings::instance().setPerGameConfig(true);
        // Commit only changed, writable fields; never rewrite hidden options.
        fields.commit();
        config::AudioVolume.calcDbPower();
        for (auto& d : devices) {
            auto p = d.device.lock();
            if (!p || !p->is_registered()) continue;
            if (d.port != d.originalPort) p->set_maple_port(d.port);
            if (d.deadZone != d.originalDeadZone && p->get_input_mapping()) p->set_dead_zone(d.deadZone);
        }
    }
};
}
