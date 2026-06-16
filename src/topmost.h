#pragma once
#include <windows.h>

// ウィンドウの最前面固定（WS_EX_TOPMOST / always-on-top）の操作。
// switcher と mouse_hook の双方から利用する共通ロジック。
namespace topmost {

// 指定ウィンドウが現在最前面固定されているか。
bool is_pinned(HWND hwnd);

// 最前面固定をトグルする。トグル後に固定状態なら true を返す。
// 無効なウィンドウには何もせず false を返す。
bool toggle(HWND hwnd);

}  // namespace topmost
