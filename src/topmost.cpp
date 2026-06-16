#include "topmost.h"

namespace topmost {

bool is_pinned(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    return (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

bool toggle(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    bool pinned_now = is_pinned(hwnd);
    SetWindowPos(hwnd, pinned_now ? HWND_NOTOPMOST : HWND_TOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    return !pinned_now;
}

}  // namespace topmost
