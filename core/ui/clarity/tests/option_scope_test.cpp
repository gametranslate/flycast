// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the real upstream Option/Settings classes with an in-memory cfg store.
#include "cfg/option.h"
#include "../draft.h"
#include <cassert>
#include <map>

namespace config {
std::map<std::pair<std::string, std::string>, std::string> store;
std::string loadStr(const std::string& s, const std::string& k, const std::string& d) {
    auto it = store.find({s, k}); return it == store.end() ? d : it->second;
}
void saveStr(const std::string& s, const std::string& k, const std::string& v) { store[{s, k}] = v; }
int loadInt(const std::string& s, const std::string& k, int d) { return std::stoi(loadStr(s, k, std::to_string(d))); }
void saveInt(const std::string& s, const std::string& k, int v) { saveStr(s, k, std::to_string(v)); }
bool isTransient(const std::string&, const std::string&) { return false; }
void setAutoSave(bool) {}
bool hasSection(const std::string& s) {
    for (const auto& item : store) if (item.first.first == s) return true;
    return false;
}
void deleteEntry(const std::string& s, const std::string& k) { store.erase({s, k}); }
void deleteSection(const std::string& s) {
    for (auto i = store.begin(); i != store.end();) {
        if (i->first.first == s) i = store.erase(i); else ++i;
    }
}
}

int main() {
    config::Option<int> resolution("pvr.Resolution", 480);
    config::Option<int> compatibility("compatibility", 0);
    config::Option<std::string, false> language("Language", "en", "clarity");
    auto& settings = config::Settings::instance();
    int r, c;
    clarity::Draft draft;
    draft.bind(r, resolution);
    draft.bind(c, compatibility);
    settings.save();
    assert(config::loadInt("config", "pvr.Resolution") == 480);
    assert(config::loadStr("clarity", "Language") == "en");

    settings.setGameId("TEST-GAME-A");
    compatibility.override(7);
    draft.capture();
    assert(compatibility.isReadOnly());
    c = 8; draft.commit();
    assert(compatibility.get() == 7); // Built-in game override is protected globally.
    draft.capture();

    r = 960; c = 9;
    assert(!settings.hasPerGameConfig()); // Editing alone doesn't create a profile.
    settings.setPerGameConfig(true); // The model only does this on Apply.
    draft.commit(); settings.save();
    assert(config::loadInt("config", "pvr.Resolution") == 480);
    assert(config::loadInt("TEST-GAME-A", "config.pvr.Resolution") == 960);
    assert(config::loadInt("TEST-GAME-A", "config.compatibility") == 9);

    language = "vi"; language.save();
    assert(config::loadStr("clarity", "Language") == "vi");
    assert(config::loadStr("TEST-GAME-A", "clarity.Language").empty());

    settings.reset(); settings.load(false);
    settings.setGameId("TEST-GAME-B"); settings.load(true);
    assert(resolution.get() == 480 && language.get() == "vi");
    assert(!settings.hasPerGameConfig()); // Game A does not leak into game B.
    settings.setGameId("TEST-GAME-A"); settings.load(true);
    assert(resolution.get() == 960 && compatibility.get() == 9);
    draft.capture(); r = 480; draft.commit(); settings.save();
    assert(config::loadStr("TEST-GAME-A", "config.pvr.Resolution").empty());
    // Returning to the global value removes the override using upstream semantics.
}
