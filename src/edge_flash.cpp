#include "edge_flash.h"
#include <cstdint>
#include <algorithm>

namespace edge_flash {
namespace {

constexpr wchar_t kClassName[] = L"CustomKeypadEdgeFlash";

constexpr UINT_PTR kTimerId = 1;
constexpr DWORD kFrameMs = 16;       // ~60 fps
constexpr DWORD kDurationMs = 500;
constexpr int kGlowWidth = 40;       // px from screen edge

// Accent color (same as indicator: #008CB4)
constexpr float kR = 0.0f;
constexpr float kG = 140.0f / 255.0f;
constexpr float kB = 180.0f / 255.0f;

HINSTANCE g_hInstance = nullptr;
HWND g_hwnd = nullptr;
HDC g_hdcMem = nullptr;
HBITMAP g_hbmp = nullptr;
uint32_t* g_pixels = nullptr;
int g_width = 0;
int g_height = 0;
ULONGLONG g_startTick = 0;

void cleanup() {
    if (g_hwnd) {
        KillTimer(g_hwnd, kTimerId);
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }
    if (g_hbmp) { DeleteObject(g_hbmp); g_hbmp = nullptr; }
    if (g_hdcMem) { DeleteDC(g_hdcMem); g_hdcMem = nullptr; }
    g_pixels = nullptr;
    g_width = 0;
    g_height = 0;
}

// Render a single glow pixel (premultiplied BGRA)
inline uint32_t glow_pixel(int dist) {
    float t = static_cast<float>(dist) / kGlowWidth;
    float s = 1.0f - t;
    float a = s * s * s;  // cubic falloff for soft blur

    auto to_byte = [](float v) -> uint32_t {
        return static_cast<uint32_t>(v * 255.0f);
    };
    return (to_byte(a) << 24) |
           (to_byte(kR * a) << 16) |
           (to_byte(kG * a) << 8) |
           to_byte(kB * a);
}

void render_glow(int sw, int sh) {
    ZeroMemory(g_pixels, sw * sh * sizeof(uint32_t));

    int gw = std::min(kGlowWidth, std::min(sw / 2, sh / 2));

    for (int y = 0; y < sh; ++y) {
        int dy = std::min(y, sh - 1 - y);
        if (dy >= gw) {
            // Center rows: only left and right edges
            for (int x = 0; x < gw; ++x) {
                uint32_t px = glow_pixel(x);
                g_pixels[y * sw + x] = px;
                g_pixels[y * sw + (sw - 1 - x)] = px;
            }
        } else {
            // Edge rows: full width
            for (int x = 0; x < sw; ++x) {
                int dx = std::min(x, sw - 1 - x);
                int d = std::min(dx, dy);
                if (d >= gw) continue;
                g_pixels[y * sw + x] = glow_pixel(d);
            }
        }
    }
}

LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_TIMER && wp == kTimerId) {
        float t = static_cast<float>(GetTickCount64() - g_startTick) / kDurationMs;
        if (t >= 1.0f) {
            cleanup();
            return 0;
        }

        // Quick rise, gradual fade
        float envelope;
        if (t < 0.15f) {
            float s = t / 0.15f;
            envelope = s * s;
        } else {
            float s = (t - 0.15f) / 0.85f;
            envelope = (1.0f - s) * (1.0f - s);
        }

        BYTE alpha = static_cast<BYTE>(envelope * 140.0f);
        POINT ptSrc = {0, 0};
        SIZE sz = {g_width, g_height};
        BLENDFUNCTION blend = {};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = alpha;
        blend.AlphaFormat = AC_SRC_ALPHA;
        UpdateLayeredWindow(hwnd, nullptr, nullptr, &sz,
                            g_hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool init(HINSTANCE hInstance) {
    g_hInstance = hInstance;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kClassName;

    return RegisterClassExW(&wc) != 0;
}

void flash() {
    // Restart if already flashing
    if (g_hwnd) cleanup();

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    constexpr DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW
                            | WS_EX_NOACTIVATE | WS_EX_LAYERED
                            | WS_EX_TRANSPARENT;
    g_hwnd = CreateWindowExW(exStyle, kClassName, L"", WS_POPUP,
                              0, 0, sw, sh,
                              nullptr, nullptr, g_hInstance, nullptr);
    if (!g_hwnd) return;

    // Create DIB
    HDC hdcScreen = GetDC(nullptr);
    g_hdcMem = CreateCompatibleDC(hdcScreen);
    ReleaseDC(nullptr, hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = sw;
    bmi.bmiHeader.biHeight = -sh;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    g_hbmp = CreateDIBSection(g_hdcMem, &bmi, DIB_RGB_COLORS,
                               &bits, nullptr, 0);
    if (!g_hbmp) { cleanup(); return; }
    g_pixels = static_cast<uint32_t*>(bits);
    SelectObject(g_hdcMem, g_hbmp);
    g_width = sw;
    g_height = sh;

    // Pre-render the glow pattern
    render_glow(sw, sh);

    // Show with initial alpha = 0
    g_startTick = GetTickCount64();

    POINT ptDst = {0, 0};
    POINT ptSrc = {0, 0};
    SIZE sz = {sw, sh};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 0;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(g_hwnd, nullptr, &ptDst, &sz,
                        g_hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    SetTimer(g_hwnd, kTimerId, kFrameMs, nullptr);
}

void shutdown() {
    cleanup();
    UnregisterClassW(kClassName, g_hInstance);
}

}  // namespace edge_flash
