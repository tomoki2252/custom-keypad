#include <windows.h>
#include <vector>
#include "hotkey.h"
#include "overlay.h"

namespace {

const std::vector<hotkey::Binding> g_bindings = {
    {
        .id = 1,
        .modifiers = MOD_CONTROL | MOD_SHIFT,
        .vk = 'P',
        .action = [] {
            POINT pt;
            GetCursorPos(&pt);
            overlay::show(pt.x, pt.y, L"\u30c6\u30b9\u30c8\u8868\u793a");
        },
    },
};

LRESULT CALLBACK msg_wndproc(HWND hwnd, UINT msg,
                             WPARAM wParam, LPARAM lParam) {
    if (msg == WM_HOTKEY) {
        hotkey::dispatch(wParam, g_bindings);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Register overlay window class
    if (!overlay::init(hInstance)) {
        return 1;
    }

    // Create hidden message-only window for hotkey events
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = msg_wndproc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"CustomKeypadMsg";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0, L"CustomKeypadMsg", L"", 0,
        0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hInstance, nullptr);

    if (!hwnd) {
        return 1;
    }

    // Register hotkeys
    if (!hotkey::register_all(hwnd, g_bindings)) {
        DestroyWindow(hwnd);
        return 1;
    }

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    hotkey::unregister_all(hwnd, g_bindings);
    DestroyWindow(hwnd);
    return 0;
}
