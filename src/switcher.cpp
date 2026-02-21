#include "switcher.h"
#include "indicator.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cwctype>

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
constexpr COLORREF kTextColor = RGB(255, 255, 255);

struct WindowEntry {
    HWND hwnd;
    std::wstring title;
};

HINSTANCE g_hInstance = nullptr;
HWND g_hwnd = nullptr;
HDC g_hdcMem = nullptr;
HBITMAP g_hbmp = nullptr;
uint32_t* g_pixels = nullptr;
int g_bmpWidth = 0;
int g_bmpHeight = 0;

std::vector<WindowEntry> g_windows;
int g_cursor = -1;  // reserved for step 2-4

// Window class names to exclude (our own windows)
constexpr const wchar_t* kExcludeClasses[] = {
    L"CustomKeypadIndicator",
    L"CustomKeypadOverlay",
    L"CustomKeypadSwitcher",
    L"CustomKeypadMsg",
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

    // Extract filename from path
    std::wstring fullPath(path, pathLen);
    size_t lastSlash = fullPath.find_last_of(L'\\');
    std::wstring filename = (lastSlash != std::wstring::npos)
        ? fullPath.substr(lastSlash + 1) : fullPath;

    // Remove .exe extension
    size_t dotPos = filename.find_last_of(L'.');
    if (dotPos != std::wstring::npos) {
        filename = filename.substr(0, dotPos);
    }

    // Check friendly name mapping (case-insensitive)
    std::wstring lower = filename;
    for (auto& c : lower) c = static_cast<wchar_t>(towlower(c));

    for (const auto& mapping : kFriendlyNames) {
        if (lower == mapping.exe_lower) {
            return mapping.display;
        }
    }

    // Fallback: return exe name as-is
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

    // Only list windows that would appear in the taskbar
    HWND owner = GetWindow(hwnd, GW_OWNER);
    if (owner != nullptr && !(exStyle & WS_EX_APPWINDOW)) return TRUE;

    // Exclude our own windows by class name
    wchar_t cls[128] = {};
    GetClassNameW(hwnd, cls, 128);
    for (const auto* exc : kExcludeClasses) {
        if (std::wstring_view(cls) == exc) return TRUE;
    }

    std::wstring display = get_display_name(hwnd);

    // Exclude by process name
    for (const auto* exc : kExcludeProcesses) {
        if (display == exc) return TRUE;
    }
    if (display.empty()) {
        // Fallback to window title
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
}

void free_bitmap() {
    if (g_hbmp) { DeleteObject(g_hbmp); g_hbmp = nullptr; }
    if (g_hdcMem) { DeleteDC(g_hdcMem); g_hdcMem = nullptr; }
    g_pixels = nullptr;
    g_bmpWidth = 0;
    g_bmpHeight = 0;
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
    g_bmpWidth = w;
    g_bmpHeight = h;
}

void render() {
    if (!g_hwnd || g_windows.empty()) return;

    // Measure all items
    HDC hdcScreen = GetDC(nullptr);
    HFONT font = create_font();
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdcScreen, font));

    struct ItemMetrics {
        std::wstring text;
        int width;
    };
    std::vector<ItemMetrics> metrics;
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
        metrics.push_back({std::move(display), item_w});
    }
    if (!metrics.empty()) {
        total_width += kItemSpacing * (static_cast<int>(metrics.size()) - 1);
    }

    SelectObject(hdcScreen, oldFont);
    DeleteObject(font);
    ReleaseDC(nullptr, hdcScreen);

    int item_height = text_height + kItemPaddingY * 2;
    int panel_height = item_height + kPanelPaddingY * 2;
    int panel_width = total_width;

    // Create/resize DIB
    create_bitmap(panel_width, panel_height);
    if (!g_pixels) return;

    // Fill background
    HBRUSH bgBrush = CreateSolidBrush(kBgColor);
    RECT full = {0, 0, panel_width, panel_height};
    FillRect(g_hdcMem, &full, bgBrush);
    DeleteObject(bgBrush);

    // Draw chips and text
    SetBkMode(g_hdcMem, TRANSPARENT);
    SetTextColor(g_hdcMem, kTextColor);
    font = create_font();
    HFONT oldF = reinterpret_cast<HFONT>(SelectObject(g_hdcMem, font));

    int x = kPanelPaddingX;
    for (size_t i = 0; i < metrics.size(); ++i) {
        RECT chip = {x, kPanelPaddingY,
                     x + metrics[i].width, kPanelPaddingY + item_height};

        HBRUSH chipBrush = CreateSolidBrush(kChipColor);
        FillRect(g_hdcMem, &chip, chipBrush);
        DeleteObject(chipBrush);

        DrawTextW(g_hdcMem, metrics[i].text.c_str(), -1, &chip,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        x += metrics[i].width + kItemSpacing;
    }

    SelectObject(g_hdcMem, oldF);
    DeleteObject(font);

    // Fix alpha channel: GDI writes alpha as 0x00, set all to 0xFF
    for (int py = 0; py < panel_height; ++py) {
        for (int px = 0; px < panel_width; ++px) {
            g_pixels[py * panel_width + px] |= 0xFF000000;
        }
    }

    // Position relative to indicator
    RECT ind = indicator::get_rect();
    int ind_center_y = (ind.top + ind.bottom) / 2;
    POINT ptDst = {ind.right + kGap, ind_center_y - panel_height / 2};

    // Fallback if indicator is not visible
    if (ind.right == 0 && ind.bottom == 0) {
        RECT workArea;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        ptDst = {workArea.left + 40, workArea.bottom - panel_height - 8};
    }

    SIZE sizeWnd = {panel_width, panel_height};
    POINT ptSrc = {0, 0};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 230;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(g_hwnd, nullptr, &ptDst, &sizeWnd,
                        g_hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
}

LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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

    render();
    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
}

void hide() {
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }
    free_bitmap();
    g_windows.clear();
    g_cursor = -1;
}

void shutdown() {
    hide();
    UnregisterClassW(kClassName, g_hInstance);
}

}  // namespace switcher
