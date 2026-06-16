#include <windows.h>
#include <vector>
#include "actions.h"
#include "config.h"
#include "hotkey.h"
#include "indicator.h"
#include "overlay.h"
#include "switcher.h"
#include "edge_flash.h"
#include "mouse_hook.h"

namespace {

constexpr int kToggleHotkeyId = 9999;
bool g_hotkeys_active = true;
HWND g_msg_hwnd = nullptr;

config::Config g_config;

// プロセスを DPI 認識にする。
// これをしないと、拡大率 >100% のディスプレイで WindowFromPoint 等の
// 座標が仮想化されてズレ、マウス位置と別のウィンドウが解決されてしまう。
void set_dpi_awareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetCtxFn = BOOL(WINAPI*)(HANDLE);
        auto set_ctx = reinterpret_cast<SetCtxFn>(reinterpret_cast<void*>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext")));
        if (set_ctx) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = (HANDLE)-4
            if (set_ctx(reinterpret_cast<HANDLE>(-4))) return;
            // フォールバック: PER_MONITOR_AWARE = (HANDLE)-3
            if (set_ctx(reinterpret_cast<HANDLE>(-3))) return;
        }
    }
    SetProcessDPIAware();  // 旧 Windows 向け（システム DPI 認識）
}

// 各機能モジュールのアクションをレジストリに登録する。
// 設定ファイルはここで登録した名前を参照してキー割り当てを行う。
void register_actions() {
    actions::register_action("switcher.toggle", [] { switcher::toggle(); });
    actions::register_action("switcher.left",   [] { switcher::move_left(); });
    actions::register_action("switcher.right",  [] { switcher::move_right(); });
    actions::register_action("switcher.pin",    [] { switcher::pin_current(); });
}

LRESULT CALLBACK msg_wndproc(HWND hwnd, UINT msg,
                             WPARAM wParam, LPARAM lParam) {
    if (msg == WM_HOTKEY) {
        if (wParam == kToggleHotkeyId) {
            g_hotkeys_active = !g_hotkeys_active;
            if (g_hotkeys_active) {
                hotkey::register_all(hwnd, g_config.bindings);
                mouse_hook::enable();
                indicator::show();
            } else {
                hotkey::unregister_all(hwnd, g_config.bindings);
                mouse_hook::disable();
                switcher::hide();
                indicator::hide();
            }
            return 0;
        }
        hotkey::dispatch(wParam, g_config.bindings);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    set_dpi_awareness();

    if (!overlay::init(hInstance)) return 1;
    if (!indicator::init(hInstance)) return 1;
    if (!switcher::init(hInstance)) return 1;
    if (!edge_flash::init(hInstance)) return 1;
    if (!mouse_hook::init(hInstance)) return 1;

    // アクション登録 → 設定読み込み（config.ini が無ければデフォルト）
    register_actions();
    g_config = config::load();

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

    // Register custom hotkeys (from config)
    if (!hotkey::register_all(g_msg_hwnd, g_config.bindings)) {
        DestroyWindow(g_msg_hwnd);
        return 1;
    }

    // Register master toggle hotkey (from config)
    RegisterHotKey(g_msg_hwnd, kToggleHotkeyId,
                   g_config.toggle_modifiers, g_config.toggle_vk);

    // Enable right-Alt + click pinning (hotkeys start active)
    mouse_hook::enable();

    // Show indicator (hotkeys start active)
    indicator::show();

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    mouse_hook::shutdown();
    edge_flash::shutdown();
    switcher::shutdown();
    indicator::shutdown();
    UnregisterHotKey(g_msg_hwnd, kToggleHotkeyId);
    hotkey::unregister_all(g_msg_hwnd, g_config.bindings);
    DestroyWindow(g_msg_hwnd);
    return 0;
}
