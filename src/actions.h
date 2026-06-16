#pragma once
#include <functional>
#include <string>

// アクションレジストリ。
// 各機能モジュールが「アクション名 -> 実行関数」を登録し、
// 設定ファイル(config.ini)のホットキー割り当てから名前で参照される。
namespace actions {

using Action = std::function<void()>;

// アクションを登録する（同名は上書き）。
void register_action(const std::string& name, Action action);

// 登録済みアクションを名前で検索。未登録なら nullptr。
const Action* find(const std::string& name);

}  // namespace actions
