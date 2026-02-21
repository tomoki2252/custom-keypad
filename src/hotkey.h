#pragma once
#include <windows.h>
#include <functional>
#include <vector>

namespace hotkey {

struct Binding {
    int id;
    UINT modifiers;
    UINT vk;
    std::function<void()> action;
};

bool register_all(HWND hwnd, const std::vector<Binding>& bindings);
void unregister_all(HWND hwnd, const std::vector<Binding>& bindings);
void dispatch(WPARAM id, const std::vector<Binding>& bindings);

}  // namespace hotkey
