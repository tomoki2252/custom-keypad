#pragma once
#include <windows.h>
#include <string>

namespace switcher {

bool init(HINSTANCE hInstance);

// リストに表示するのと同じ表示名を返す（フレンドリ名 + 長すぎる場合は省略）。
std::wstring display_name(HWND hwnd);
void toggle();       // Enumerate + show/refresh list
void move_left();    // Move cursor left + focus
void move_right();   // Move cursor right + focus
void pin_current();  // Toggle always-on-top (topmost) for the selected window
void hide();
void shutdown();

}  // namespace switcher
