#include "hotkey.h"

namespace hotkey {

bool register_all(HWND hwnd, const std::vector<Binding>& bindings) {
    for (const auto& b : bindings) {
        if (!RegisterHotKey(hwnd, b.id, b.modifiers, b.vk)) {
            return false;
        }
    }
    return true;
}

void unregister_all(HWND hwnd, const std::vector<Binding>& bindings) {
    for (const auto& b : bindings) {
        UnregisterHotKey(hwnd, b.id);
    }
}

void dispatch(WPARAM id, const std::vector<Binding>& bindings) {
    for (const auto& b : bindings) {
        if (b.id == static_cast<int>(id)) {
            b.action();
            return;
        }
    }
}

}  // namespace hotkey
