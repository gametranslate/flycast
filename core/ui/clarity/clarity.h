// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>

namespace clarity {
// All hooks are compiled out when FLYCAST_CLARITY is disabled.
void beginSession();
bool display(const std::function<void(bool close)>& finishSettings);
bool returnFromClassic();
#ifdef FLYCAST_CLARITY_SMOKE_TEST
void smokeBeforeFrame();
void smokeAfterFrame();
void smokeConfigure(int locale, bool dark, int page);
void smokeObserve(const char* key);
bool smokeCheck(int liveResolution, int draftResolution, bool inClassic);
#endif
}
