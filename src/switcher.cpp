#include "switcher.h"
#include "indicator.h"
#include "edge_flash.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cwctype>
#include <unordered_map>
#include <algorithm>

namespace switcher {
namespace {

constexpr wchar_t kClassName[] = L"CustomKeypadSwitcher";

// Layout
constexpr int kGap = 6;            // gap between indicator and panel
constexpr int kItemPaddingX = 10;   // horizontal padding inside each chip
constexpr int kItemPaddingY = 4;    // vertical padding inside each chip
constexpr int kItemSpacing = 2;     // gap between chips
constexpr int kPanelPaddingX = 4;   // panel-level horizontal padding
constexpr int kPanelPaddingY = 3;   // panel-level vertical padding
constexpr int kFontSize = 13;
constexpr int kMaxTitleLen = 24;

// Colors
constexpr COLORREF kBgColor = RGB(26, 26, 46);       // #1A1A2E
constexpr COLORREF kChipColor = RGB(42, 42, 64);     // #2A2A40
constexpr COLORREF kSelectedColor = RGB(0, 140, 180); // #008CB4 accent
constexpr COLORREF kTextColor = RGB(255, 255, 255);

// Animation
constexpr UINT_PTR kFocusTimerId = 1;
constexpr UINT_PTR kAnimTimerId = 2;
constexpr DWORD kFocusPollMs = 100;
constexpr DWORD kAnimFrameMs = 16;    // ~60 fps
constexpr DWORD kChipAnimMs = 400;    // each chip fade-in duration
constexpr DWORD kChipStaggerMs = 100; // delay between consecutive chips
constexpr int kSlideDistance = 8;     // px slide-up on intro
constexpr DWORD kFadeOutMs = 300;
constexpr BYTE kPanelAlpha = 230;     // steady-state SourceConstantAlpha

enum class AnimState { IDLE, INTRO, VISIBLE, FADEOUT };

struct WindowEntry {
    HWND hwnd;
    std::wstring title;
};

struct ChipLayout {
    std::wstring text;
    int x;
    int width;
};

HINSTANCE g_hInstance = nullptr;
HWND g_hwnd = nullptr;
HDC g_hdcMem = nullptr;
HBITMAP g_hbmp = nullptr;
uint32_t* g_pixels = nullptr;

std::vector<WindowEntry> g_windows;
int g_cursor = -1;

// Layout cache
std::vector<ChipLayout> g_chips;
int g_itemHeight = 0;
int g_panelW = 0;
int g_panelH = 0;
POINT g_panelPos = {};

// Animation state
AnimState g_state = AnimState::IDLE;
ULONGLONG g_animStart = 0;

// Window class names to exclude (our own windows)
constexpr const wchar_t* kExcludeClasses[] = {
    L"CustomKeypadIndicator",
    L"CustomKeypadOverlay",
    L"CustomKeypadSwitcher",
    L"CustomKeypadMsg",
    L"CustomKeypadEdgeFlash",
};

// Process names to exclude from the window list
constexpr const wchar_t* kExcludeProcesses[] = {
    L"TextInputHost",
    L"ApplicationFrameHost",
    L"SystemSettings",
};

// Exe name -> friendly display name mapping
struct NameMapping {
    const wchar_t* exe_lower;
    const wchar_t* display;
};

constexpr NameMapping kFriendlyNames[] = {
    {L"code", L"VS Code"},
    {L"msedge", L"Edge"},
    {L"chrome", L"Chrome"},
    {L"firefox", L"Firefox"},
    {L"explorer", L"Explorer"},
    {L"windowsterminal", L"Terminal"},
    {L"wt", L"Terminal"},
    {L"cmd", L"CMD"},
    {L"powershell", L"PowerShell"},
    {L"pwsh", L"PowerShell"},
    {L"notepad", L"Notepad"},
    {L"slack", L"Slack"},
    {L"discord", L"Discord"},
    {L"msteams", L"Teams"},
};

std::wstring get_display_name(HWND hwnd) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return L"";

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return L"";

    wchar_t path[MAX_PATH] = {};
    DWORD pathLen = MAX_PATH;
    BOOL ok = QueryFullProcessImageNameW(hProcess, 0, path, &pathLen);
    CloseHandle(hProcess);

    if (!ok) return L"";

    std::wstring fullPath(path, pathLen);
    size_t lastSlash = fullPath.find_last_of(L'\\');
    std::wstring filename = (lastSlash != std::wstring::npos)
        ? fullPath.substr(lastSlash + 1) : fullPath;

    size_t dotPos = filename.find_last_of(L'.');
    if (dotPos != std::wstring::npos) {
        filename = filename.substr(0, dotPos);
    }

    std::wstring lower = filename;
    for (auto& c : lower) c = static_cast<wchar_t>(towlower(c));

    for (const auto& mapping : kFriendlyNames) {
        if (lower == mapping.exe_lower) {
            return mapping.display;
        }
    }

    return filename;
}

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

BOOL CALLBACK enum_callback(HWND hwnd, LPARAM lParam) {
    auto* windows = reinterpret_cast<std::vector<WindowEntry>*>(lParam);

    if (!IsWindowVisible(hwnd)) return TRUE;
    if (IsIconic(hwnd)) return TRUE;

    int len = GetWindowTextLengthW(hwnd);
    if (len == 0) return TRUE;

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

    HWND owner = GetWindow(hwnd, GW_OWNER);
    if (owner != nullptr && !(exStyle & WS_EX_APPWINDOW)) return TRUE;

    wchar_t cls[128] = {};
    GetClassNameW(hwnd, cls, 128);
    for (const auto* exc : kExcludeClasses) {
        if (std::wstring_view(cls) == exc) return TRUE;
    }

    std::wstring display = get_display_name(hwnd);

    for (const auto* exc : kExcludeProcesses) {
        if (display == exc) return TRUE;
    }
    if (display.empty()) {
        std::wstring title(len + 1, L'\0');
        GetWindowTextW(hwnd, title.data(), len + 1);
        title.resize(len);
        display = std::move(title);
    }

    windows->push_back({hwnd, std::move(display)});
    return TRUE;
}

void enumerate_windows() {
    g_windows.clear();
    g_cursor = -1;
    EnumWindows(enum_callback, reinterpret_cast<LPARAM>(&g_windows));

    // Disambiguate duplicate titles
    std::unordered_map<std::wstring, int> counts;
    for (const auto& w : g_windows) counts[w.title]++;

    std::unordered_map<std::wstring, int> seen;
    for (auto& w : g_windows) {
        if (counts[w.title] > 1) {
            int idx = ++seen[w.title];
            w.title += L" (" + std::to_wstring(idx) + L")";
        }
    }
}

void free_bitmap() {
    if (g_hbmp) { DeleteObject(g_hbmp); g_hbmp = nullptr; }
    if (g_hdcMem) { DeleteDC(g_hdcMem); g_hdcMem = nullptr; }
    g_pixels = nullptr;
}

void create_bitmap(int w, int h) {
    free_bitmap();

    HDC hdcScreen = GetDC(nullptr);
    g_hdcMem = CreateCompatibleDC(hdcScreen);
    ReleaseDC(nullptr, hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    g_hbmp = CreateDIBSection(g_hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    g_pixels = static_cast<uint32_t*>(bits);
    SelectObject(g_hdcMem, g_hbmp);
}

// Compute layout metrics (text measurement + positions)
void compute_layout() {
    HDC hdcScreen = GetDC(nullptr);
    HFONT font = create_font();
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdcScreen, font));

    g_chips.clear();
    int total_width = kPanelPaddingX * 2;
    int text_height = 0;

    for (const auto& w : g_windows) {
        std::wstring display = w.title;
        if (display.size() > kMaxTitleLen) {
            display = display.substr(0, kMaxTitleLen - 3) + L"...";
        }
        SIZE sz;
        GetTextExtentPoint32W(hdcScreen, display.c_str(),
                              static_cast<int>(display.size()), &sz);
        if (sz.cy > text_height) text_height = sz.cy;
        int item_w = sz.cx + kItemPaddingX * 2;
        total_width += item_w;
        g_chips.push_back({std::move(display), 0, item_w});
    }
    if (!g_chips.empty()) {
        total_width += kItemSpacing * (static_cast<int>(g_chips.size()) - 1);
    }

    SelectObject(hdcScreen, oldFont);
    DeleteObject(font);
    ReleaseDC(nullptr, hdcScreen);

    g_itemHeight = text_height + kItemPaddingY * 2;
    g_panelH = g_itemHeight + kPanelPaddingY * 2;
    g_panelW = total_width;

    // X positions
    int x = kPanelPaddingX;
    for (auto& cl : g_chips) {
        cl.x = x;
        x += cl.width + kItemSpacing;
    }

    // Final position (relative to indicator)
    RECT ind = indicator::get_rect();
    int ind_center_y = (ind.top + ind.bottom) / 2;
    g_panelPos = {ind.right + kGap, ind_center_y - g_panelH / 2};

    if (ind.right == 0 && ind.bottom == 0) {
        RECT workArea;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        g_panelPos = {workArea.left + 40, workArea.bottom - g_panelH - 8};
    }
}

// Render one frame. progress: 0.0 (start of intro) to 1.0 (fully visible)
void render_frame(float global_progress) {
    if (!g_hwnd || !g_pixels || g_chips.empty()) return;

    int n = static_cast<int>(g_chips.size());

    // 1. Draw panel background
    HBRUSH bgBrush = CreateSolidBrush(kBgColor);
    RECT full = {0, 0, g_panelW, g_panelH};
    FillRect(g_hdcMem, &full, bgBrush);
    DeleteObject(bgBrush);

    // Fix bg alpha
    for (int i = 0; i < g_panelW * g_panelH; ++i)
        g_pixels[i] |= 0xFF000000;

    // 2. Draw each chip with per-chip animation
    SetBkMode(g_hdcMem, TRANSPARENT);
    SetTextColor(g_hdcMem, kTextColor);
    HFONT font = create_font();
    HFONT oldF = reinterpret_cast<HFONT>(SelectObject(g_hdcMem, font));

    // Compute total intro time for stagger scaling
    DWORD totalMs = kChipAnimMs + (n > 1 ? (n - 1) * kChipStaggerMs : 0);

    for (int i = 0; i < n; ++i) {
        // Per-chip progress with stagger
        float delay = static_cast<float>(i * kChipStaggerMs) / totalMs;
        float chipDur = static_cast<float>(kChipAnimMs) / totalMs;
        float chip_t = std::clamp((global_progress - delay) / chipDur, 0.0f, 1.0f);
        // Ease-out quadratic
        float progress = 1.0f - (1.0f - chip_t) * (1.0f - chip_t);

        if (progress <= 0.001f) continue;

        auto& cl = g_chips[i];
        RECT chip = {cl.x, kPanelPaddingY,
                     cl.x + cl.width, kPanelPaddingY + g_itemHeight};

        // Save background pixels before chip drawing
        int cw = chip.right - chip.left;
        int ch = chip.bottom - chip.top;
        std::vector<uint32_t> bg_saved(cw * ch);
        for (int cy = 0; cy < ch; ++cy)
            for (int cx = 0; cx < cw; ++cx)
                bg_saved[cy * cw + cx] =
                    g_pixels[(chip.top + cy) * g_panelW + chip.left + cx];

        // Draw chip rect + text
        COLORREF color = (i == g_cursor) ? kSelectedColor : kChipColor;
        HBRUSH chipBrush = CreateSolidBrush(color);
        FillRect(g_hdcMem, &chip, chipBrush);
        DeleteObject(chipBrush);
        DrawTextW(g_hdcMem, cl.text.c_str(), -1, &chip,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Blend chip over saved bg with per-chip progress
        if (progress >= 0.999f) {
            for (int cy = 0; cy < ch; ++cy)
                for (int cx = 0; cx < cw; ++cx)
                    g_pixels[(chip.top + cy) * g_panelW + chip.left + cx] |=
                        0xFF000000;
        } else {
            uint32_t p8 = static_cast<uint32_t>(progress * 255.0f);
            uint32_t ip8 = 255 - p8;
            for (int cy = 0; cy < ch; ++cy) {
                for (int cx = 0; cx < cw; ++cx) {
                    uint32_t bg = bg_saved[cy * cw + cx];
                    uint32_t& pixel =
                        g_pixels[(chip.top + cy) * g_panelW + chip.left + cx];
                    uint32_t bR = (bg >> 16) & 0xFF;
                    uint32_t bG = (bg >> 8) & 0xFF;
                    uint32_t bB = bg & 0xFF;
                    uint32_t cR = (pixel >> 16) & 0xFF;
                    uint32_t cG = (pixel >> 8) & 0xFF;
                    uint32_t cB = pixel & 0xFF;
                    uint32_t fR = (bR * ip8 + cR * p8) / 255;
                    uint32_t fG = (bG * ip8 + cG * p8) / 255;
                    uint32_t fB = (bB * ip8 + cB * p8) / 255;
                    pixel = 0xFF000000 | (fR << 16) | (fG << 8) | fB;
                }
            }
        }
    }

    SelectObject(g_hdcMem, oldF);
    DeleteObject(font);

    // 3. Position with slide-up offset
    float slide_t = std::clamp(global_progress * 2.0f, 0.0f, 1.0f);
    float slide_ease = 1.0f - (1.0f - slide_t) * (1.0f - slide_t);
    int dy = static_cast<int>((1.0f - slide_ease) * kSlideDistance);

    POINT ptDst = {g_panelPos.x, g_panelPos.y + dy};
    SIZE sizeWnd = {g_panelW, g_panelH};
    POINT ptSrc = {0, 0};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = kPanelAlpha;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(g_hwnd, nullptr, &ptDst, &sizeWnd,
                        g_hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
}

void do_hide() {
    if (g_hwnd) {
        KillTimer(g_hwnd, kAnimTimerId);
        KillTimer(g_hwnd, kFocusTimerId);
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }
    free_bitmap();
    g_windows.clear();
    g_chips.clear();
    g_cursor = -1;
    g_state = AnimState::IDLE;
}

void focus_current() {
    if (g_cursor < 0 || g_cursor >= static_cast<int>(g_windows.size())) return;
    HWND target = g_windows[g_cursor].hwnd;
    if (!IsWindow(target)) return;

    if (IsIconic(target)) ShowWindow(target, SW_RESTORE);
    SetForegroundWindow(target);
    edge_flash::flash();
}

void sync_cursor_to_foreground() {
    if (!g_hwnd || g_windows.empty()) return;
    if (g_state == AnimState::FADEOUT) return;

    HWND fg = GetForegroundWindow();
    for (int i = 0; i < static_cast<int>(g_windows.size()); ++i) {
        if (g_windows[i].hwnd == fg) {
            if (g_cursor != i) {
                g_cursor = i;
                if (g_state == AnimState::VISIBLE)
                    render_frame(1.0f);
                // During INTRO, next animation frame picks up new cursor
            }
            return;
        }
    }
    if (g_cursor != -1) {
        g_cursor = -1;
        if (g_state == AnimState::VISIBLE)
            render_frame(1.0f);
    }
}

LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TIMER) {
        if (wParam == kFocusTimerId) {
            sync_cursor_to_foreground();
            return 0;
        }
        if (wParam == kAnimTimerId) {
            ULONGLONG now = GetTickCount64();
            float elapsed = static_cast<float>(now - g_animStart);

            if (g_state == AnimState::INTRO) {
                int n = static_cast<int>(g_chips.size());
                DWORD totalMs = kChipAnimMs
                    + (n > 1 ? (n - 1) * kChipStaggerMs : 0);
                float t = elapsed / totalMs;
                if (t >= 1.0f) {
                    g_state = AnimState::VISIBLE;
                    KillTimer(g_hwnd, kAnimTimerId);
                    render_frame(1.0f);
                } else {
                    render_frame(t);
                }
            } else if (g_state == AnimState::FADEOUT) {
                float t = elapsed / kFadeOutMs;
                if (t >= 1.0f) {
                    do_hide();
                } else {
                    // Ease-in quadratic (accelerating fade)
                    float alpha = 1.0f - t * t;
                    BYTE a = static_cast<BYTE>(alpha * kPanelAlpha);
                    POINT ptSrc = {0, 0};
                    SIZE sizeWnd = {g_panelW, g_panelH};
                    BLENDFUNCTION blend = {};
                    blend.BlendOp = AC_SRC_OVER;
                    blend.SourceConstantAlpha = a;
                    blend.AlphaFormat = AC_SRC_ALPHA;
                    UpdateLayeredWindow(g_hwnd, nullptr, nullptr, &sizeWnd,
                                        g_hdcMem, &ptSrc, 0, &blend,
                                        ULW_ALPHA);
                }
            }
            return 0;
        }
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

void toggle() {
    // Cancel fade-out if in progress
    if (g_state == AnimState::FADEOUT) {
        KillTimer(g_hwnd, kAnimTimerId);
        g_state = AnimState::IDLE;
    }

    enumerate_windows();
    if (g_windows.empty()) {
        hide();
        return;
    }

    if (!g_hwnd) {
        constexpr DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW
                                | WS_EX_NOACTIVATE | WS_EX_LAYERED;
        g_hwnd = CreateWindowExW(
            exStyle, kClassName, L"",
            WS_POPUP,
            0, 0, 0, 0,
            nullptr, nullptr, g_hInstance, nullptr);
        if (!g_hwnd) return;
    }

    compute_layout();
    create_bitmap(g_panelW, g_panelH);
    if (!g_pixels) return;

    // Set cursor to current foreground window
    HWND fg = GetForegroundWindow();
    g_cursor = -1;
    for (int i = 0; i < static_cast<int>(g_windows.size()); ++i) {
        if (g_windows[i].hwnd == fg) { g_cursor = i; break; }
    }

    // Start intro animation
    g_state = AnimState::INTRO;
    g_animStart = GetTickCount64();
    render_frame(0.0f);

    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    SetTimer(g_hwnd, kAnimTimerId, kAnimFrameMs, nullptr);
    SetTimer(g_hwnd, kFocusTimerId, kFocusPollMs, nullptr);
}

void move_left() {
    if (!g_hwnd || g_windows.empty()) return;

    // Cancel fade-out
    if (g_state == AnimState::FADEOUT) {
        KillTimer(g_hwnd, kAnimTimerId);
        g_state = AnimState::VISIBLE;
        SetTimer(g_hwnd, kFocusTimerId, kFocusPollMs, nullptr);
    }

    int n = static_cast<int>(g_windows.size());
    if (g_cursor <= 0)
        g_cursor = n - 1;
    else
        g_cursor--;

    if (g_state == AnimState::VISIBLE)
        render_frame(1.0f);
    focus_current();
}

void move_right() {
    if (!g_hwnd || g_windows.empty()) return;

    // Cancel fade-out
    if (g_state == AnimState::FADEOUT) {
        KillTimer(g_hwnd, kAnimTimerId);
        g_state = AnimState::VISIBLE;
        SetTimer(g_hwnd, kFocusTimerId, kFocusPollMs, nullptr);
    }

    g_cursor = (g_cursor + 1) % static_cast<int>(g_windows.size());

    if (g_state == AnimState::VISIBLE)
        render_frame(1.0f);
    focus_current();
}

void hide() {
    if (!g_hwnd || g_state == AnimState::FADEOUT) return;

    KillTimer(g_hwnd, kFocusTimerId);
    if (g_state == AnimState::INTRO)
        KillTimer(g_hwnd, kAnimTimerId);

    // Render final frame for clean fade-out source
    render_frame(1.0f);

    g_state = AnimState::FADEOUT;
    g_animStart = GetTickCount64();
    SetTimer(g_hwnd, kAnimTimerId, kAnimFrameMs, nullptr);
}

void shutdown() {
    g_state = AnimState::IDLE;
    do_hide();
    UnregisterClassW(kClassName, g_hInstance);
}

}  // namespace switcher
