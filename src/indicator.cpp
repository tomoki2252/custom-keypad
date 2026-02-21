#include "indicator.h"
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace indicator {
namespace {

constexpr wchar_t kClassName[] = L"CustomKeypadIndicator";
constexpr UINT_PTR kAnimTimerId = 100;
constexpr DWORD kFrameIntervalMs = 16;  // ~60fps
constexpr int kSize = 48;
constexpr int kMargin = 24;

// Colors (normalized 0.0-1.0)
constexpr float kAccentR = 0.0f;
constexpr float kAccentG = 0.831f;
constexpr float kAccentB = 1.0f;    // #00D4FF

constexpr float kBodyR = 0.102f;
constexpr float kBodyG = 0.102f;
constexpr float kBodyB = 0.180f;    // #1A1A2E

// Animation
constexpr float kBreathSpeed = 1.8f;  // rad/s (~3.5s cycle)

HINSTANCE g_hInstance = nullptr;
HWND g_hwnd = nullptr;
HDC g_hdcMem = nullptr;
HBITMAP g_hbmp = nullptr;
uint32_t* g_pixels = nullptr;
ULONGLONG g_startTick = 0;

// Signed distance to a flat-top regular hexagon centered at origin
float sdf_hexagon(float px, float py, float r) {
    constexpr float k = 0.8660254f;  // sqrt(3)/2
    float ax = std::abs(px);
    float ay = std::abs(py);
    float d = std::max(ax * 0.5f + ay * k, ax) - r;
    return d;
}

// Signed distance to a diamond (45-deg rotated square) centered at origin
float sdf_diamond(float px, float py, float r) {
    return (std::abs(px) + std::abs(py)) - r;
}

// Premultiplied alpha composite: src over dst
void composite_over(float sr, float sg, float sb, float sa,
                    float& dr, float& dg, float& db, float& da) {
    float inv = 1.0f - sa;
    dr = sr * sa + dr * inv;
    dg = sg * sa + dg * inv;
    db = sb * sa + db * inv;
    da = sa + da * inv;
}

void render_frame() {
    ULONGLONG now = GetTickCount64();
    double elapsed = (now - g_startTick) / 1000.0;
    float breath = 0.65f + 0.35f * std::sin(static_cast<float>(elapsed * kBreathSpeed));

    float cx = kSize * 0.5f;
    float cy = kSize * 0.5f;

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            float px = x + 0.5f - cx;
            float py = y + 0.5f - cy;

            float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;

            // Layer 1: Outer glow (radial gradient, animated)
            float dist = std::sqrt(px * px + py * py);
            float glow_inner = 16.0f;
            float glow_outer = 23.0f;
            if (dist < glow_outer) {
                float t = std::clamp((dist - glow_inner) / (glow_outer - glow_inner), 0.0f, 1.0f);
                float glow_a = (1.0f - t * t) * 0.6f * breath;
                composite_over(kAccentR, kAccentG, kAccentB, glow_a, r, g, b, a);
            }

            // Layer 2: Hexagon body (dark fill)
            float hex_d = sdf_hexagon(px, py, 18.0f);
            float hex_a = std::clamp(-hex_d + 0.5f, 0.0f, 1.0f);
            if (hex_a > 0.0f) {
                composite_over(kBodyR, kBodyG, kBodyB, hex_a, r, g, b, a);
            }

            // Layer 3: Inner hexagon ring (accent outline, breathing)
            float ring_d = std::abs(sdf_hexagon(px, py, 15.0f)) - 0.75f;
            float ring_a = std::clamp(-ring_d + 0.5f, 0.0f, 1.0f) * breath;
            if (ring_a > 0.0f) {
                composite_over(kAccentR, kAccentG, kAccentB, ring_a, r, g, b, a);
            }

            // Layer 4: Center diamond (constant accent)
            float diamond_d = sdf_diamond(px, py, 6.0f);
            float diamond_a = std::clamp(-diamond_d + 0.5f, 0.0f, 1.0f);
            if (diamond_a > 0.0f) {
                composite_over(kAccentR, kAccentG, kAccentB, diamond_a, r, g, b, a);
            }

            // Store as premultiplied BGRA (DIB byte order)
            auto to_byte = [](float v) -> uint8_t {
                return static_cast<uint8_t>(std::clamp(v * 255.0f, 0.0f, 255.0f));
            };
            g_pixels[y * kSize + x] =
                (static_cast<uint32_t>(to_byte(a)) << 24) |
                (static_cast<uint32_t>(to_byte(r)) << 16) |
                (static_cast<uint32_t>(to_byte(g)) << 8) |
                static_cast<uint32_t>(to_byte(b));
        }
    }

    // Update layered window
    POINT ptSrc = {0, 0};
    SIZE sizeWnd = {kSize, kSize};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(g_hwnd, nullptr, nullptr, &sizeWnd,
                        g_hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
}

LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TIMER && wParam == kAnimTimerId) {
        render_frame();
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
    wc.lpszClassName = kClassName;

    return RegisterClassExW(&wc) != 0;
}

void show() {
    if (g_hwnd) return;

    // Use work area (excludes taskbar) for positioning
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int pos_x = workArea.left + kMargin;
    int pos_y = workArea.bottom - kSize - kMargin;

    constexpr DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW
                            | WS_EX_NOACTIVATE | WS_EX_LAYERED
                            | WS_EX_TRANSPARENT;

    g_hwnd = CreateWindowExW(
        exStyle, kClassName, L"",
        WS_POPUP,
        pos_x, pos_y, kSize, kSize,
        nullptr, nullptr, g_hInstance, nullptr);

    if (!g_hwnd) return;

    // Create render target (DIB section for direct pixel access)
    HDC hdcScreen = GetDC(nullptr);
    g_hdcMem = CreateCompatibleDC(hdcScreen);
    ReleaseDC(nullptr, hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = kSize;
    bmi.bmiHeader.biHeight = -kSize;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    g_hbmp = CreateDIBSection(g_hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    g_pixels = static_cast<uint32_t*>(bits);
    SelectObject(g_hdcMem, g_hbmp);

    // Initial render and show
    g_startTick = GetTickCount64();
    render_frame();

    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    SetTimer(g_hwnd, kAnimTimerId, kFrameIntervalMs, nullptr);
}

void hide() {
    if (!g_hwnd) return;

    KillTimer(g_hwnd, kAnimTimerId);

    if (g_hbmp) {
        DeleteObject(g_hbmp);
        g_hbmp = nullptr;
    }
    if (g_hdcMem) {
        DeleteDC(g_hdcMem);
        g_hdcMem = nullptr;
    }
    g_pixels = nullptr;

    DestroyWindow(g_hwnd);
    g_hwnd = nullptr;
}

void shutdown() {
    hide();
    UnregisterClassW(kClassName, g_hInstance);
}

}  // namespace indicator
