#include "overlay.h"

namespace overlay {
namespace {

constexpr wchar_t kClassName[] = L"CustomKeypadOverlay";
constexpr UINT_PTR kTimerId = 1;
constexpr DWORD kDismissMs = 2000;
constexpr int kPaddingX = 16;
constexpr int kPaddingY = 12;
constexpr int kFontSize = 18;
constexpr int kCursorOffset = 10;

HINSTANCE g_hInstance = nullptr;
HWND g_hwnd = nullptr;
std::wstring g_text;

HFONT create_font() {
    return CreateFontW(
        -kFontSize, 0, 0, 0,
        FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Meiryo");
}

LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        SetBkColor(hdc, RGB(0, 0, 0));
        SetTextColor(hdc, RGB(255, 255, 255));

        HFONT font = create_font();
        HFONT old = reinterpret_cast<HFONT>(SelectObject(hdc, font));

        RECT rc;
        GetClientRect(hwnd, &rc);
        DrawTextW(hdc, g_text.c_str(), -1, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, old);
        DeleteObject(font);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_TIMER:
        if (wParam == kTimerId) {
            hide();
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

bool init(HINSTANCE hInstance) {
    g_hInstance = hInstance;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hInstance;
    wc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
    wc.lpszClassName = kClassName;

    return RegisterClassExW(&wc) != 0;
}

void show(int x, int y, const std::wstring& text) {
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }

    g_text = text;

    // Measure text to size the window
    HDC hdc = GetDC(nullptr);
    HFONT font = create_font();
    HFONT old = reinterpret_cast<HFONT>(SelectObject(hdc, font));
    SIZE sz;
    GetTextExtentPoint32W(hdc, g_text.c_str(),
                          static_cast<int>(g_text.size()), &sz);
    SelectObject(hdc, old);
    DeleteObject(font);
    ReleaseDC(nullptr, hdc);

    int w = sz.cx + kPaddingX * 2;
    int h = sz.cy + kPaddingY * 2;

    constexpr DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    constexpr DWORD style = WS_POPUP | WS_BORDER;

    g_hwnd = CreateWindowExW(
        exStyle, kClassName, L"",
        style,
        x + kCursorOffset, y + kCursorOffset,
        w, h,
        nullptr, nullptr, g_hInstance, nullptr);

    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    SetTimer(g_hwnd, kTimerId, kDismissMs, nullptr);
}

void hide() {
    if (g_hwnd) {
        KillTimer(g_hwnd, kTimerId);
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }
}

}  // namespace overlay
