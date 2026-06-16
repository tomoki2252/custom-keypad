#pragma once
#include <windows.h>
#include <vector>
#include "hotkey.h"

// config.ini からホットキー割り当てを読み込む。
//
// 形式 (INI):
//   [hotkeys]
//   Ctrl+Alt+M    = app.toggle_hotkeys   ; マスタートグル（特別扱い）
//   Alt+OEM_MINUS = switcher.toggle
//   Alt+P         = switcher.pin
//
// アクション名は actions レジストリで解決される。
// ファイルが無い／読めない場合は組み込みデフォルトにフォールバックする。
namespace config {

struct Config {
    // ディスパッチ対象のホットキー（ON/OFF トグルで登録/解除される）
    std::vector<hotkey::Binding> bindings;

    // マスタートグル（ホットキー全体の有効/無効を切り替える特別キー）
    UINT toggle_modifiers = MOD_CONTROL | MOD_ALT;
    UINT toggle_vk = 'M';
};

Config load();

}  // namespace config
