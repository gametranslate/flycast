// SPDX-License-Identifier: GPL-2.0-or-later
#include "../draft.h"
#include <cassert>
#include <string>
#include <vector>

template<typename T> struct Option {
    T value;
    bool locked = false;
    int writes = 0;
    T& get() { return value; }
    bool isReadOnly() const { return locked; }
    void set(T next) { value = next; ++writes; }
};

int main() {
    Option<int> resolution{480}, volume{100}, compatibility{42, true};
    Option<std::vector<std::string>> paths{{"D:/Games"}};
    int r, v, c;
    std::vector<std::string> p;
    clarity::Draft draft;
    draft.bind(r, resolution);
    draft.bind(v, volume);
    draft.bind(c, compatibility);
    draft.bind(p, paths);
    draft.capture();
    assert(draft.count() == 0);
    assert(!draft.commit());
    assert(resolution.writes == 0 && volume.writes == 0 && paths.writes == 0);

    r = 960; p.push_back("E:/日本語");
    assert(draft.count() == 2);
    assert(resolution.value == 480 && paths.value.size() == 1); // No writes before Apply.
    draft.capture(); // Undo discards every staged value.
    assert(r == 480 && p.size() == 1 && draft.count() == 0);

    r = 1440; c = 100;
    volume.value = 65; // External change to an untouched option must survive Apply.
    assert(draft.commit());
    assert(resolution.value == 1440 && compatibility.value == 42 && volume.value == 65);
    assert(volume.writes == 0 && compatibility.writes == 0);
    draft.capture();
    assert(v == 65 && c == 42 && draft.count() == 0);

    v = 80; volume.locked = true; // Lock may be introduced after the draft was opened.
    assert(!draft.commit() && volume.value == 65);
    volume.locked = false;
    assert(draft.commit() && volume.value == 80);
    draft.capture();
    r = 960; r = 1440; // Changing back to the original is not a write.
    int writes = resolution.writes;
    assert(!draft.commit() && resolution.writes == writes);
}
