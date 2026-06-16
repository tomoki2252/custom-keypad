#include "mouse_hook.h"
#include "topmost.h"
#include "edge_flash.h"
#include "overlay.h"
#include "switcher.h"
#include <string>
#include <cwchar>

namespace mouse_hook {
namespace {

HHOOK g_hook = nullptr;
HINSTANCE g_hInstance = nullptr;

// 右Altが押下中か（左Altとは区別される）。
bool right_alt_down() {
    return (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;
}

// 本アプリ自身のウィンドウ（オーバーレイ等）か。
bool is_own_window(HWND hwnd) {
    wchar_t cls[64] = {};
    GetClassNameW(hwnd, cls, 64);
    return wcsncmp(cls, L"CustomKeypad", 12) == 0;
}

LRESULT CALLBACK hook_proc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && wParam == WM_LBUTTONDOWN && right_alt_down()) {
        auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        HWND hwnd = WindowFromPoint(ms->pt);
        if (hwnd) {
            HWND root = GetAncestor(hwnd, GA_ROOT);
            if (root && !is_own_window(root)) {
                bool pinned = topmost::toggle(root);
                std::wstring name = switcher::display_name(root);
                std::wstring msg = pinned ? (name + L" を最前面に固定")
                                          : (name + L" の固定を解除");
                overlay::show(ms->pt.x, ms->pt.y, msg);
                edge_flash::flash();
            }
        }
        return 1;  // クリックを消費（対象アプリへ渡さない）
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

}  // namespace

bool init(HINSTANCE hInstance) {
    g_hInstance = hInstance;
    return true;
}

void enable() {
    if (g_hook) return;
    g_hook = SetWindowsHookExW(WH_MOUSE_LL, hook_proc, g_hInstance, 0);
}

void disable() {
    if (g_hook) {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
}

void shutdown() {
    disable();
}

}  // namespace mouse_hook
