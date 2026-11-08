// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace clarity {
// A transaction only owns fields it displays. Never reset/load the entire config
// while drafting: that would erase game compatibility overrides and unknown keys.
class Draft {
public:
    struct Field {
        std::function<void()> capture;
        std::function<bool()> changed;
        std::function<bool()> commit;
    };

    template<typename T, typename Option>
    void bind(T& value, Option& option) {
        auto original = std::make_shared<T>(option.get());
        fields.push_back({
            [&value, &option, original] { value = *original = option.get(); },
            [&value, original] { return value != *original; },
            [&value, &option, original] {
                if (value == *original || option.isReadOnly())
                    return false;
                option.set(value);
                return true;
            }
        });
    }
    void capture() { for (auto& f : fields) f.capture(); }
    int count() const {
        int n = 0;
        for (const auto& f : fields) n += f.changed();
        return n;
    }
    bool commit() {
        bool changed = false;
        for (auto& f : fields) changed |= f.commit();
        return changed;
    }
private:
    std::vector<Field> fields;
};
}
