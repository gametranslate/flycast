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
#include "imgui.h"

namespace clarity {
namespace {
int frame = 0;
constexpr int framesPerCase = 8;
constexpr int cases = 22;
std::map<std::string, ImVec2> items;
struct Step { const char* key; int live, draft; bool classic; };
const Step steps[] = {
    {"balanced", 480, 960, false},
    {"night", 480, 960, false},
    {"revert", 480, 480, false},
    {"sharp", 480, 1440, false},
    {"apply", 1440, 1440, false},
    {"balanced", 1440, 960, false},
    {"classic", 1440, 960, false},
    {"cancel", 1440, 960, false},
    {"classic", 1440, 960, false},
    {"discard", 1440, 1440, true},
    {"backClarity", 1440, 1440, false},
    {"original", 1440, 480, false},
    {"classic", 1440, 480, false},
    {"applyContinue", 480, 480, true},
    {"backClarity", 480, 480, false},
};
void capture(int index);
void fail(int code, const char* key) {
    capture(99);
    if (auto f = std::fopen("clarity-smoke-fail.txt", "w")) {
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
    if (!stbi_write_png(path, desc.Width, desc.Height, 4, rgba.data(), desc.Width * 4)) std::exit(5);
}
}
void smokeObserve(const char* key) {
    auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    if (ImGui::IsItemVisible()) items[key] = ImVec2((a.x + b.x) / 2, (a.y + b.y) / 2);
}
void smokeBeforeFrame() {
    // Isolate the deterministic test stream from the host window's real input.
    ImGui::GetIO().ClearEventsQueue();
    ImGui::GetIO().ConfigInputTrickleEventQueue = false;
    ImGui::GetIO().AddFocusEvent(true);
    int index = frame / framesPerCase;
    if (index >= cases) {
        // Small-window runs only exercise rendering; full-size runs also drive
        // real widget input through ImGui's public IO API (never OS input).
        if (ImGui::GetIO().DisplaySize.x >= 1000) {
            int testFrame = frame - cases * framesPerCase;
            if (testFrame == 0) smokeConfigure(0, false, 0);
            if (testFrame < framesPerCase) return;
            int step = (testFrame - framesPerCase) / framesPerCase;
            int phase = testFrame % framesPerCase;
            if (step < (int)std::size(steps)) {
                const auto& test = steps[step];
                if (phase <= 3) {
                    auto it = items.find(test.key);
                    if (it == items.end()) fail(10, test.key);
                    ImGui::GetIO().AddMousePosEvent(it->second.x, it->second.y);
                    ImGui::GetIO().AddMouseButtonEvent(0, phase < 2);
                }
                if (phase == 6 && !smokeCheck(test.live, test.draft, test.classic)) fail(11, test.key);
                return;
            }
            if (config::loadInt("config", "rend.Resolution") != 480
                || config::loadStr("qa-preserve", "unknown") != "keep-me") fail(12, "persistence");
        }
        std::FILE* report = std::fopen("clarity-smoke-pass.txt", "w");
        if (!report) std::exit(6);
        std::fprintf(report, "Rendered 8 locales x 2 themes plus 3 other categories x 2 themes.\nWidget interaction tests: %s\n",
            ImGui::GetIO().DisplaySize.x >= 1000 ? "PASS" : "SKIPPED (small window)");
        std::fclose(report);
        std::exit(0);
    }
    if (frame == 0) gui_setState(GuiState::Settings);
    if (frame % framesPerCase == 0) {
        int locale = index < 16 ? index / 2 : 0;
        int page = index < 16 ? 0 : (index - 16) / 2 + 1;
        smokeConfigure(locale, index % 2 != 0, page);
    }
}
void smokeAfterFrame() {
    if (frame < cases * framesPerCase && frame % framesPerCase == framesPerCase - 1) capture(frame / framesPerCase);
    ++frame;
}
}
