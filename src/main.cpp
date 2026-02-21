#include <windows.h>
#include <vector>
#include "hotkey.h"
#include "indicator.h"
#include "overlay.h"
#include "switcher.h"

#ifndef VK_F23
#define VK_F23 0x86
#endif

#ifndef VK_OEM_MINUS
#define VK_OEM_MINUS 0xBD
#endif

namespace {

constexpr int kToggleHotkeyId = 9999;
bool g_hotkeys_active = true;
HWND g_msg_hwnd = nullptr;

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
    {
        .id = 10,
        .modifiers = MOD_ALT,
        .vk = VK_OEM_MINUS,
        .action = [] { switcher::toggle(); },
    },
};

LRESULT CALLBACK msg_wndproc(HWND hwnd, UINT msg,
                             WPARAM wParam, LPARAM lParam) {
    if (msg == WM_HOTKEY) {
        if (wParam == kToggleHotkeyId) {
            POINT pt;
            GetCursorPos(&pt);
            g_hotkeys_active = !g_hotkeys_active;
            if (g_hotkeys_active) {
                hotkey::register_all(hwnd, g_bindings);
                indicator::show();
                overlay::show(pt.x, pt.y, L"Hotkeys ON");
            } else {
                hotkey::unregister_all(hwnd, g_bindings);
                switcher::hide();
                indicator::hide();
                overlay::show(pt.x, pt.y, L"Hotkeys OFF");
            }
            return 0;
        }
        hotkey::dispatch(wParam, g_bindings);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    if (!overlay::init(hInstance)) return 1;
    if (!indicator::init(hInstance)) return 1;
    if (!switcher::init(hInstance)) return 1;

    // Create hidden message-only window for hotkey events
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = msg_wndproc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"CustomKeypadMsg";
    RegisterClassExW(&wc);

    g_msg_hwnd = CreateWindowExW(
        0, L"CustomKeypadMsg", L"", 0,
        0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hInstance, nullptr);

    if (!g_msg_hwnd) return 1;

    // Register custom hotkeys
    if (!hotkey::register_all(g_msg_hwnd, g_bindings)) {
        DestroyWindow(g_msg_hwnd);
        return 1;
    }

    // Register Ctrl+Alt+M as toggle
    RegisterHotKey(g_msg_hwnd, kToggleHotkeyId, MOD_CONTROL | MOD_ALT, 'M');

    // Show indicator (hotkeys start active)
    indicator::show();

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    switcher::shutdown();
    indicator::shutdown();
    UnregisterHotKey(g_msg_hwnd, kToggleHotkeyId);
    hotkey::unregister_all(g_msg_hwnd, g_bindings);
    DestroyWindow(g_msg_hwnd);
    return 0;
}
