#include "indicator.h"
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace indicator {
namespace {

constexpr wchar_t kClassName[] = L"CustomKeypadIndicator";
constexpr UINT_PTR kAnimTimerId = 100;
constexpr DWORD kFrameIntervalMs = 16;  // ~60fps
constexpr int kSize = 32;
constexpr int kMargin = 8;

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

// Drag state
constexpr int kDragThreshold = 5;  // px to distinguish click from drag
bool g_mouse_down = false;
bool g_dragging = false;
POINT g_drag_start = {};    // screen coords at mouse down
POINT g_window_start = {};  // window pos at mouse down

// Spin animation
constexpr float kPi = 3.14159265f;
constexpr DWORD kSpinDurationMs = 1200;
constexpr float kSpinRevolutions = 0.5f;
ULONGLONG g_spinStartTick = 0;

// Fade out animation
constexpr DWORD kFadeDurationMs = 400;
bool g_fading_out = false;
ULONGLONG g_fadeStartTick = 0;

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

void do_hide() {
    if (!g_hwnd) return;
    g_fading_out = false;
    KillTimer(g_hwnd, kAnimTimerId);
    if (g_hbmp) { DeleteObject(g_hbmp); g_hbmp = nullptr; }
    if (g_hdcMem) { DeleteDC(g_hdcMem); g_hdcMem = nullptr; }
    g_pixels = nullptr;
    DestroyWindow(g_hwnd);
    g_hwnd = nullptr;
}

void start_spin() {
    g_spinStartTick = GetTickCount64();
}

void render_frame() {
    if (!g_hwnd || !g_pixels) return;

    ULONGLONG now = GetTickCount64();
    double elapsed = (now - g_startTick) / 1000.0;
    float breath = 0.65f + 0.35f * std::sin(static_cast<float>(elapsed * kBreathSpeed));

    // Spin animation (ease-out cubic)
    float spin_angle = 0.0f;
    if (g_spinStartTick > 0) {
        float t = static_cast<float>(now - g_spinStartTick) / kSpinDurationMs;
        if (t >= 1.0f) {
            g_spinStartTick = 0;
        } else {
            float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
            spin_angle = ease * kSpinRevolutions * 2.0f * kPi;
        }
    }

    // Fade out animation (ease-in quadratic)
    float fade_alpha = 1.0f;
    if (g_fading_out) {
        float t = static_cast<float>(now - g_fadeStartTick) / kFadeDurationMs;
        if (t >= 1.0f) {
            do_hide();
            return;
        }
        fade_alpha = 1.0f - t * t;
    }

    // Precompute rotation for hexagon (+angle) and diamond (-angle)
    float hex_cos = std::cos(spin_angle);
    float hex_sin = std::sin(spin_angle);
    float dia_cos = std::cos(-spin_angle);
    float dia_sin = std::sin(-spin_angle);

    float cx = kSize * 0.5f;
    float cy = kSize * 0.5f;

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            float px = x + 0.5f - cx;
            float py = y + 0.5f - cy;

            // Rotated coordinates
            float hpx = px * hex_cos - py * hex_sin;
            float hpy = px * hex_sin + py * hex_cos;
            float dpx = px * dia_cos - py * dia_sin;
            float dpy = px * dia_sin + py * dia_cos;

            float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;

            // Layer 1: Outer glow (radial, no rotation)
            float dist = std::sqrt(px * px + py * py);
            float glow_inner = 10.7f;
            float glow_outer = 15.3f;
            if (dist < glow_outer) {
                float t = std::clamp((dist - glow_inner) / (glow_outer - glow_inner), 0.0f, 1.0f);
                float glow_a = (1.0f - t * t) * 0.6f * breath;
                composite_over(kAccentR, kAccentG, kAccentB, glow_a, r, g, b, a);
            }

            // Layer 2: Hexagon body (rotated)
            float hex_d = sdf_hexagon(hpx, hpy, 12.0f);
            float hex_a = std::clamp(-hex_d + 0.5f, 0.0f, 1.0f);
            if (hex_a > 0.0f) {
                composite_over(kBodyR, kBodyG, kBodyB, hex_a, r, g, b, a);
            }

            // Layer 3: Inner hexagon ring (rotated with body)
            float ring_d = std::abs(sdf_hexagon(hpx, hpy, 10.0f)) - 0.5f;
            float ring_a = std::clamp(-ring_d + 0.5f, 0.0f, 1.0f) * breath;
            if (ring_a > 0.0f) {
                composite_over(kAccentR, kAccentG, kAccentB, ring_a, r, g, b, a);
            }

            // Layer 4: Center diamond (rotated opposite)
            float diamond_d = sdf_diamond(dpx, dpy, 4.0f);
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

    // Update layered window (SourceConstantAlpha for fade)
    POINT ptSrc = {0, 0};
    SIZE sizeWnd = {kSize, kSize};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = static_cast<BYTE>(fade_alpha * 255.0f);
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(g_hwnd, nullptr, nullptr, &sizeWnd,
                        g_hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
}

LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TIMER && wParam == kAnimTimerId) {
        render_frame();
        return 0;
    }
    if (msg == WM_LBUTTONDOWN) {
        g_mouse_down = true;
        g_dragging = false;
        GetCursorPos(&g_drag_start);
        RECT rc;
        GetWindowRect(hwnd, &rc);
        g_window_start = {rc.left, rc.top};
        SetCapture(hwnd);
        return 0;
    }
    if (msg == WM_MOUSEMOVE && g_mouse_down) {
        POINT pt;
        GetCursorPos(&pt);
        int dx = pt.x - g_drag_start.x;
        int dy = pt.y - g_drag_start.y;
        if (!g_dragging &&
            (std::abs(dx) > kDragThreshold || std::abs(dy) > kDragThreshold)) {
            g_dragging = true;
        }
        if (g_dragging) {
            SetWindowPos(hwnd, nullptr,
                         g_window_start.x + dx, g_window_start.y + dy,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    }
    if (msg == WM_LBUTTONUP && g_mouse_down) {
        g_mouse_down = false;
        ReleaseCapture();
        if (!g_dragging) {
            start_spin();
        }
        g_dragging = false;
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
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    wc.lpszClassName = kClassName;

    return RegisterClassExW(&wc) != 0;
}

void show() {
    // Cancel fade-out if in progress, just spin instead
    if (g_fading_out) {
        g_fading_out = false;
        start_spin();
        return;
    }
    if (g_hwnd) return;

    // Position at bottom-left with margin
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int pos_x = workArea.left + kMargin;
    int pos_y = workArea.bottom - kSize - kMargin;

    constexpr DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW
                            | WS_EX_NOACTIVATE | WS_EX_LAYERED;

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

    // Initial render with spin and show
    g_startTick = GetTickCount64();
    start_spin();
    render_frame();

    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    SetTimer(g_hwnd, kAnimTimerId, kFrameIntervalMs, nullptr);
}

void hide() {
    if (!g_hwnd || g_fading_out) return;
    g_fading_out = true;
    g_fadeStartTick = GetTickCount64();
    // Timer keeps running to animate the fade; do_hide() called on completion
}

void shutdown() {
    g_fading_out = false;
    do_hide();
    UnregisterClassW(kClassName, g_hInstance);
}

RECT get_rect() {
    RECT rc = {};
    if (g_hwnd) GetWindowRect(g_hwnd, &rc);
    return rc;
}

}  // namespace indicator
