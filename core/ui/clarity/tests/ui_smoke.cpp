// SPDX-License-Identifier: GPL-2.0-or-later
// Opt-in renderer test: captures the actual ImGui DX11 backbuffer and exits.
// No game, desktop input injection, user configuration or screen-capture service.
#include "ui/clarity/clarity.h"
#include "ui/gui.h"
#include "rend/dx11/dx11context.h"
#include "stb_image_write.h"
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <map>
#include <filesystem>
#include "imgui.h"
#include "input/gamepad_device.h"
#include "imgui_internal.h"

namespace clarity {
namespace {
int frame = 0;
constexpr int framesPerCase = 8;
constexpr int cases = 114;
std::map<std::string, ImVec2> items;
class TestPad : public GamepadDevice {
public:
    TestPad() : GamepadDevice(0, "ClarityRegression") {
        _name = "Regression controller";
        _unique_id = "clarity-ui-test";
        hasAnalogStick = true;
        input_mapper = std::make_shared<InputMapping>();
        input_mapper->set_button(DC_BTN_A, 90);
    }
};
std::shared_ptr<TestPad> testPad;
u64 inputStart = 0;
int interactionStep = 0, stepFrame = 0;
bool originalDsp = false;
bool nativeTest = std::getenv("CLARITY_NATIVE_TEST") != nullptr;
int nativePhase = 0, nativeFrame = 0;
bool nativeSeen = false;
void capture(int index);
void fail(int code, const char* key) {
    capture(99);
    if (auto f = std::fopen(get_writable_config_path("clarity-smoke-fail.txt").c_str(), "w")) {
        std::fprintf(f, "Failed at %s (code %d, frame %d)\n", key, code, frame);
        std::fclose(f);
    }
    std::exit(code);
}
void capture(int index) {
    auto* context = DX11Context::Instance();
    ComPtr<ID3D11Resource> resource;
    context->getRenderTarget()->GetResource(&resource.get());
    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texture.get());
    if (FAILED(hr)) std::exit(2);
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> copy;
    if (FAILED(context->getDevice()->CreateTexture2D(&desc, nullptr, &copy.get()))) std::exit(3);
    context->getDeviceContext()->CopyResource(copy, texture);
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->getDeviceContext()->Map(copy, 0, D3D11_MAP_READ, 0, &mapped))) std::exit(4);
    std::vector<unsigned char> rgba(desc.Width * desc.Height * 4);
    bool bgra = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    for (unsigned y = 0; y < desc.Height; ++y) {
        auto* src = static_cast<unsigned char*>(mapped.pData) + y * mapped.RowPitch;
        auto* dst = rgba.data() + y * desc.Width * 4;
        for (unsigned x = 0; x < desc.Width; ++x) {
            dst[x * 4] = src[x * 4 + (bgra ? 2 : 0)];
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4 + (bgra ? 0 : 2)];
            dst[x * 4 + 3] = 255;
        }
    }
    context->getDeviceContext()->Unmap(copy, 0);
    char path[64];
    std::snprintf(path, sizeof(path), "clarity-native-%02d.png", index);
    std::string output = get_writable_config_path(path);
    if (!stbi_write_png(output.c_str(), desc.Width, desc.Height, 4, rgba.data(), desc.Width * 4)) std::exit(5);
}
}
void smokeObserve(const char* key) {
    auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    if (ImGui::IsItemVisible()) items[key] = ImVec2((a.x + b.x) / 2, (a.y + b.y) / 2);
}
void smokeQuickScroll() {
    const int index = frame / framesPerCase;
    ImGui::SetScrollY(index >= 98 ? ImGui::GetScrollMaxY() : 0);
}
void smokeBeforeFrame() {
    static bool configured = false;
    if (!configured) {
        nativeTest |= config::loadBool("qa-preserve", "native-dialog", false);
        configured = true;
    }
    if (nativeTest) {
        if (nativeFrame == 0) {
            smokeConfigure(0,false,0);
            gui_setState(nativePhase == 0 ? GuiState::Main : GuiState::Cheats);
            items.clear();
        }
        if (nativeFrame >= 8 && nativeFrame <= 11) {
            ImGui::GetIO().ClearEventsQueue();
            ImGui::GetIO().ConfigInputTrickleEventQueue = false;
            ImGui::GetIO().AddFocusEvent(true);
            auto item = items.find(nativePhase == 0 ? "Add game folder" : "Load");
            if (item == items.end()) fail(30,"native-dialog-button");
            ImGui::GetIO().AddMousePosEvent(item->second.x,item->second.y);
            ImGui::GetIO().AddMouseButtonEvent(0,nativeFrame<10);
        }
        if (windowsPickerActive()) nativeSeen = true;
        if (nativeSeen && !windowsPickerActive()) {
            std::string expected = get_writable_config_path("Folder Việt Đức");
            if (config::ContentPath.get().size() != 1
                    || !std::filesystem::equivalent(std::filesystem::u8path(config::ContentPath.get()[0]),
                        std::filesystem::u8path(expected))) fail(31,"native-folder-result");
            if (nativePhase == 0) {
                nativePhase = 1; nativeFrame = -1; nativeSeen = false;
            } else {
                SaveSettings();
                auto report=std::fopen(get_writable_config_path("native-dialog-pass.txt").c_str(),"w");
                if (report) { std::fputs("PASS: real Windows folder selection with Unicode path, shared file dialog opened and cancelled; folder preserved.\n",report); std::fclose(report); }
                std::exit(0);
            }
        }
        ++nativeFrame;
        return;
    }
    ImGui::GetIO().ClearEventsQueue();
    ImGui::GetIO().ConfigInputTrickleEventQueue = false;
    ImGui::GetIO().AddFocusEvent(true);
    int index = frame / framesPerCase;
    if (index >= cases) {
        if (ImGui::GetIO().DisplaySize.x >= 1000) {
            auto click = [](const char* key) {
                auto it = items.find(key);
                if (it == items.end()) fail(10, key);
                ImGui::GetIO().AddMousePosEvent(it->second.x, it->second.y);
                ImGui::GetIO().AddMouseButtonEvent(0, stepFrame < 2);
            };
            if (interactionStep == 0) {
                if (stepFrame == 0) {
                    smokeScrollToBottom(false);
                    settings.content.title.clear();
                    if (!GImGui->OpenPopupStack.empty()) ImGui::ClosePopupToLevel(0, true);
                    config::GGPOEnable = config::NetworkEnable = config::BattleCableEnable = false;
                    gui_setState(GuiState::Main);
                    smokeConfigure(0, false, 0);
                    originalDsp = config::DSPEnabled;
                    items.clear();
                }
                if (stepFrame >= 8) { interactionStep++; stepFrame = -1; }
            } else if (interactionStep == 1) {
                if (stepFrame <= 3) click("Settings");
                if (stepFrame == 7) {
                    if (gui_state != GuiState::Settings) fail(11, "home-settings");
                    interactionStep++; stepFrame = -1;
                }
            } else if (interactionStep == 2) {
                if (stepFrame <= 3) click("Audio");
                if (stepFrame == 7) {
                    if (smokePage() != (int)Page::Audio) fail(11, "audio-navigation");
                    interactionStep++; stepFrame = -1;
                }
            } else if (interactionStep == 3) {
                if (stepFrame <= 3) click("Enable DSP");
                if (stepFrame == 7) {
                    if (config::DSPEnabled.get() == originalDsp) fail(11, "dsp-widget");
                    interactionStep++; stepFrame = -1;
                }
            } else if (interactionStep == 4) {
                if (stepFrame <= 3) click("Save changes");
                if (stepFrame == 7) {
                    bool before = config::DSPEnabled;
                    config::DSPEnabled.load();
                    if (config::DSPEnabled.get() != before) fail(11, "dsp-persistence");
                    testPad = std::make_shared<TestPad>();
                    GamepadDevice::Register(testPad);
                    openSettings(Page::Controllers);
                    smokeConfigure(0,false,(int)Page::Controllers);
                    smokeUseTestController();
                    interactionStep++; stepFrame = -1;
                }
            } else if (interactionStep == 5) {
                // Allow layout to settle, then use the visible A mapping widget.
                if (stepFrame >= 8 && stepFrame <= 11) {
                    auto it = items.find("map-A");
                    if (it == items.end()) fail(10,"map-A");
                    ImGui::GetIO().AddMousePosEvent(it->second.x,it->second.y);
                    ImGui::GetIO().AddMouseButtonEvent(0,stepFrame<10);
                }
                if (stepFrame == 14) {
                    if (!testPad->is_input_detecting()) fail(11, "map-detection-start");
                    inputStart = getTimeMs();
                    interactionStep++; stepFrame = -1;
                }
            } else if (interactionStep == 6) {
                if (getTimeMs() - inputStart > 400) {
                    testPad->gamepad_btn_input(42,true);
                    testPad->gamepad_btn_input(42,false);
                    interactionStep++; stepFrame = -1;
                }
            } else if (interactionStep == 7) {
                if (stepFrame == 7) {
                    if (testPad->get_input_mapping()->get_button_code(0,DC_BTN_A) != 42) fail(11,"map-A-result");
                    interactionStep++; stepFrame = -1;
                }
            } else if (interactionStep == 8) {
                if (stepFrame <= 3) click("Save changes");
                if (stepFrame == 7) {
                    if (!testPad->find_mapping(DC_PLATFORM_DREAMCAST)
                            || testPad->get_input_mapping()->get_button_code(0,DC_BTN_A) != 42) fail(11,"map-file-reload");
                    GamepadDevice::Unregister(testPad);
                    interactionStep++; stepFrame = -1;
                }
            } else if (interactionStep == 9) {
                if (stepFrame == 8) {
                    testPad.reset();
                    openSettings(Page::About);
                    interactionStep++; stepFrame = -1;
                }
            } else if (interactionStep == 10) {
                if (stepFrame > 6 && stepFrame <= 10) {
                    auto it = items.find("Save & return");
                    if (it == items.end()) fail(10,"Save & return");
                    ImGui::GetIO().AddMousePosEvent(it->second.x,it->second.y);
                    ImGui::GetIO().AddMouseButtonEvent(0,stepFrame<9);
                }
                if (stepFrame == 14) {
                    if (gui_state != GuiState::Main) fail(11,"return-library");
                    interactionStep++; stepFrame = -1;
                }
            } else {
                config::DSPEnabled = originalDsp;
                closeMappings();
                SaveSettings();
                if (config::AudioBufferSize.get() != 987 || config::RenderResolution.get() != 480) fail(12,"view-preserves-settings");
                // Continue to the final report.
                interactionStep = 99;
            }
            ++stepFrame;
            if (interactionStep != 99) return;
        }
        closeMappings();
        try { smokeTestDrops(); } catch (const std::exception& e) { fail(13,e.what()); }
        SaveSettings();
        if (config::loadStr("qa-preserve", "unknown") != "keep-me") fail(12, "preserve");
        auto report = std::fopen("clarity-smoke-pass.txt", "w");
        if (!report) std::exit(6);
        std::fprintf(report, "PASS: 114 native DX11 renders: 8 locales x 2 themes for library, controllers and About; all 11 Settings pages; GGPO/Naomi/Battle, quick menu, cheats and Advanced bottom; 32 additional quick-menu locale/theme captures at top and bottom with a long title. Unicode folder drop, duplicate prevention, persistence, game/disc routing and busy-state guard passed. Unknown config preserved.\nWidgets: %s\n",
            ImGui::GetIO().DisplaySize.x >= 1000 ? "PASS: navigation, DSP save/reload, mapping A through input detection, mapping-file reload, synthetic unplug, return to library; original audio samples and resolution preserved." : "SKIPPED: compact rendering only");
        std::fclose(report);
        std::exit(0);
    }
    if (frame % framesPerCase == 0) {
        smokeScrollToBottom(index >= 80);
        if (!GImGui->OpenPopupStack.empty()) ImGui::ClosePopupToLevel(0, true);
        int locale = 0, page = 0;
        bool home = index < 16;
        if (home) locale = index/2;
        else if (index < 38) page = (index-16)/2;
        else if (index < 54) { page = 1; locale = (index-38)/2; }
        else if (index < 70) { page = 10; locale = (index-54)/2; }
        else page = (int)Page::Network;
        gui_setState(home ? GuiState::Main : GuiState::Settings);
        smokeConfigure(locale, index%2 != 0, page);
        if (index >= 70) {
            config::GGPOEnable = index<72;
            config::NetworkEnable = index>=72 && index<74;
            config::BattleCableEnable = index>=74 && index<76;
            if (index>=76 && index<78) gui_setState(GuiState::Commands);
            if (index>=78 && index<80) gui_setState(GuiState::Cheats);
            if (index>=80 && index<82) {
                openSettings(Page::Advanced);
            }
            if (index>=82) {
                smokeConfigure(((index-82)%16)/2, index%2 != 0, 0);
                settings.content.title = "Quick menu layout test - A Dreamcast game with a long title";
                gui_setState(GuiState::Commands);
            }
        }
        items.clear();
    }
}
void smokeAfterFrame() {
    if (nativeTest) return;
    if (frame < cases*framesPerCase && frame%framesPerCase == framesPerCase-1) capture(frame/framesPerCase);
    ++frame;
}
}
