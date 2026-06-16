#include "config.h"
#include "actions.h"
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

namespace config {
namespace {

constexpr const char* kToggleActionName = "app.toggle_hotkeys";
constexpr int kFirstBindingId = 10;  // main.cpp の予約 ID (9999) と衝突しない範囲

// --- 文字列ユーティリティ -------------------------------------------------

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// --- キー名 -> 仮想キーコード ---------------------------------------------

struct VkName {
    const char* name;
    UINT vk;
};

constexpr VkName kVkNames[] = {
    {"OEM_MINUS", 0xBD}, {"OEM_PLUS", 0xBB}, {"OEM_COMMA", 0xBC},
    {"OEM_PERIOD", 0xBE}, {"OEM_1", 0xBA}, {"OEM_2", 0xBF}, {"OEM_3", 0xC0},
    {"OEM_4", 0xDB}, {"OEM_5", 0xDC}, {"OEM_6", 0xDD}, {"OEM_7", 0xDE},
    {"OEM_8", 0xDF}, {"OEM_102", 0xE2},
    {"SPACE", VK_SPACE}, {"TAB", VK_TAB}, {"ENTER", VK_RETURN},
    {"RETURN", VK_RETURN}, {"ESC", VK_ESCAPE}, {"ESCAPE", VK_ESCAPE},
    {"BACK", VK_BACK}, {"DELETE", VK_DELETE}, {"DEL", VK_DELETE},
    {"INSERT", VK_INSERT}, {"HOME", VK_HOME}, {"END", VK_END},
    {"PAGEUP", VK_PRIOR}, {"PRIOR", VK_PRIOR},
    {"PAGEDOWN", VK_NEXT}, {"NEXT", VK_NEXT},
    {"LEFT", VK_LEFT}, {"UP", VK_UP}, {"RIGHT", VK_RIGHT}, {"DOWN", VK_DOWN},
};

// 単一トークン（"ALT" / "A" / "OEM_MINUS" / "F5" ...）を解釈。
// modifier の場合は out_mod に OR、通常キーの場合は out_vk に格納。
// 戻り値: 解釈できれば true。
bool parse_token(const std::string& raw, UINT& out_mod, UINT& out_vk,
                 bool& vk_seen) {
    std::string t = upper(trim(raw));
    if (t.empty()) return false;

    // 修飾キー
    if (t == "CTRL" || t == "CONTROL") { out_mod |= MOD_CONTROL; return true; }
    if (t == "ALT")                    { out_mod |= MOD_ALT;     return true; }
    if (t == "SHIFT")                  { out_mod |= MOD_SHIFT;   return true; }
    if (t == "WIN" || t == "SUPER")    { out_mod |= MOD_WIN;     return true; }

    // 通常キーは1つだけ
    if (vk_seen) return false;

    // 単一英数字
    if (t.size() == 1) {
        char c = t[0];
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            out_vk = static_cast<UINT>(c);
            vk_seen = true;
            return true;
        }
        return false;
    }

    // ファンクションキー F1..F24
    if (t[0] == 'F' && t.size() <= 3) {
        bool digits = true;
        for (size_t i = 1; i < t.size(); ++i)
            if (!std::isdigit(static_cast<unsigned char>(t[i]))) digits = false;
        if (digits) {
            int n = std::stoi(t.substr(1));
            if (n >= 1 && n <= 24) {
                out_vk = static_cast<UINT>(VK_F1 + (n - 1));
                vk_seen = true;
                return true;
            }
        }
    }

    // 名前付きキー
    for (const auto& e : kVkNames) {
        if (t == e.name) {
            out_vk = e.vk;
            vk_seen = true;
            return true;
        }
    }
    return false;
}

// "Ctrl+Alt+M" 形式のキー指定を解釈。
bool parse_keyspec(const std::string& spec, UINT& mods, UINT& vk) {
    mods = 0;
    vk = 0;
    bool vk_seen = false;
    size_t start = 0;
    while (start <= spec.size()) {
        size_t plus = spec.find('+', start);
        std::string token = (plus == std::string::npos)
                                ? spec.substr(start)
                                : spec.substr(start, plus - start);
        if (!parse_token(token, mods, vk, vk_seen)) return false;
        if (plus == std::string::npos) break;
        start = plus + 1;
    }
    return vk_seen;  // 修飾キーのみは不可
}

// --- ファイル読み込み -----------------------------------------------------

std::wstring exe_dir() {
    wchar_t path[MAX_PATH] = {};
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path, n);
    size_t s = p.find_last_of(L'\\');
    return (s == std::wstring::npos) ? L"." : p.substr(0, s);
}

// config.ini を読み込み、中身を返す。読めなければ空を返し ok=false。
std::string read_config_file(bool& ok) {
    ok = false;
    std::wstring path = exe_dir() + L"\\config.ini";
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return "";

    std::string content;
    char buf[4096];
    DWORD read = 0;
    while (ReadFile(h, buf, sizeof(buf), &read, nullptr) && read > 0) {
        content.append(buf, read);
    }
    CloseHandle(h);
    ok = true;
    return content;
}

// --- デフォルト割り当て ---------------------------------------------------

struct DefaultBinding {
    const char* keyspec;
    const char* action;
};

constexpr DefaultBinding kDefaults[] = {
    {"Ctrl+Alt+M", "app.toggle_hotkeys"},
    {"Alt+OEM_MINUS", "switcher.toggle"},
    {"Alt+OEM_7", "switcher.left"},
    {"Alt+OEM_5", "switcher.right"},
    {"Alt+P", "switcher.pin"},
};

// 1 エントリ (keyspec, action) を Config に適用。
void apply_entry(Config& cfg, int& next_id, const std::string& keyspec,
                 const std::string& action) {
    UINT mods = 0, vk = 0;
    if (!parse_keyspec(keyspec, mods, vk)) return;  // 不正キーは無視

    if (action == kToggleActionName) {
        cfg.toggle_modifiers = mods;
        cfg.toggle_vk = vk;
        return;
    }

    const actions::Action* fn = actions::find(action);
    if (!fn) return;  // 未登録アクションは無視

    cfg.bindings.push_back({next_id++, mods, vk, *fn});
}

}  // namespace

Config load() {
    Config cfg;
    int next_id = kFirstBindingId;

    bool ok = false;
    std::string content = read_config_file(ok);

    if (!ok || trim(content).empty()) {
        // フォールバック: 組み込みデフォルト
        for (const auto& d : kDefaults)
            apply_entry(cfg, next_id, d.keyspec, d.action);
        return cfg;
    }

    // INI パース: [section] 行とコメント(# / ;)を無視し、key=value を解釈
    size_t pos = 0;
    while (pos < content.size()) {
        size_t eol = content.find('\n', pos);
        std::string line = (eol == std::string::npos)
                               ? content.substr(pos)
                               : content.substr(pos, eol - pos);
        pos = (eol == std::string::npos) ? content.size() : eol + 1;

        std::string t = trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';' || t[0] == '[') continue;

        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        std::string keyspec = trim(t.substr(0, eq));
        std::string action = trim(t.substr(eq + 1));
        if (keyspec.empty() || action.empty()) continue;

        apply_entry(cfg, next_id, keyspec, action);
    }

    return cfg;
}

}  // namespace config
