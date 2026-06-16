#pragma once
#include <windows.h>

// 低レベルマウスフック（WH_MOUSE_LL）。
// 右Alt + 左クリックで、クリックしたウィンドウの最前面固定をトグルする。
// 当該クリックは対象アプリへ渡さず消費する。
namespace mouse_hook {

bool init(HINSTANCE hInstance);
void enable();    // フックを設置（マスタートグルON時）
void disable();   // フックを解除（マスタートグルOFF時）
void shutdown();

}  // namespace mouse_hook
