# custom-keypad

## プロジェクト概要

Windows上で動作する常駐型カスタムショートカットキーツール。
Windows APIを使用し、C++で実装する。
ホットキーに応じたアクションを実行し、オーバーレイUIでフィードバックを表示する。

## ターゲットユーザ

- 効率化を重視するパワーユーザ
- ダークテーマのプロフェッショナルなデザインを好む

## 開発環境

- OS: WSL2 (Ubuntu) 上で開発し、Windows向けにクロスコンパイル
- 言語: C++20
- ビルドシステム: CMake (3.20+)
- クロスコンパイラ: MinGW-w64 (`x86_64-w64-mingw32-g++`)
- コンパイルオプション: `-Wall -Wextra -Wpedantic`, UNICODE / _UNICODE 定義
- 高速化: ccache（利用可能な場合は自動適用）

## ビルド手順

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build
```

## 実行

```bash
./build/custom-keypad.exe       # WSL2 interop経由でWindowsプロセスとして起動
taskkill.exe /IM custom-keypad.exe /F  # 終了
```

## ホットキー / 機能

ホットキー割り当ては `config.ini`（実行ファイルと同じ場所）で設定する。
ファイルが無い場合は以下のデフォルトが使われる。

- Ctrl+Alt+M: ホットキーON/OFFトグル（画面左下のインジケーター連動）
- Alt+OEM_MINUS: ウィンドウ切替パネルの表示/更新（switcher.toggle）
- Alt+OEM_7 / Alt+OEM_5: 選択を左 / 右へ（switcher.left / switcher.right）
- Alt+P: 選択ウィンドウを最前面に固定/解除（switcher.pin、ピン中は黄色マーカー）

### マウスジェスチャー（config.ini 非対象）

- 右Alt + 左クリック: クリックしたウィンドウを最前面に固定/解除（トグル）。
  低レベルマウスフック（`WH_MOUSE_LL`）で実装。当該クリックは対象アプリへ
  渡さず消費する。固定/解除時はクリック位置にウィンドウ名（リストと同じ
  表示名）をメッセージ表示する。マスタートグル（Ctrl+Alt+M）OFF 時はフックも
  解除される。
  注意: 管理者権限で起動したウィンドウへは非管理者の本アプリからは固定不可。

- DPI 認識: `WinMain` 冒頭で Per-Monitor DPI 認識を有効化している
  （`set_dpi_awareness`）。これがないと拡大率 >100% のディスプレイで
  `WindowFromPoint` の座標がズレ、クリック位置と別のウィンドウが固定される。

### 設定駆動アーキテクチャ

- `actions`: 「アクション名 → 実行関数」のレジストリ。各機能モジュールが
  `main.cpp` の `register_actions()` で自分のアクションを登録する。
- `config`: `config.ini` を自前 INI パーサで読み、キー指定をアクション名で
  解決して `hotkey::Binding` のリストを生成する。`app.toggle_hotkeys` は
  マスタートグルとして特別扱い。
- 機能追加時は「モジュール実装 → アクション登録 → config.ini にキー追加」のみ。

## Git ワークフロー

- ブランチ: `main`
- リモート: `origin` (git@github.com:tomoki2252/custom-keypad.git)
- コミットメッセージは英語

## ディレクトリ構成

```
custom-keypad/
├── CMakeLists.txt
├── CLAUDE.md
├── config.ini                   # ホットキー割り当て（ビルド時に exe 横へコピー）
├── .gitignore
├── cmake/
│   └── mingw-w64-x86_64.cmake   # MinGW-w64クロスコンパイル用ツールチェーン
├── src/
│   ├── main.cpp                      # WinMainエントリ、メッセージループ、アクション登録
│   ├── actions.h / actions.cpp       # アクションレジストリ（名前→関数）
│   ├── config.h / config.cpp         # config.ini パーサ（キー指定→Binding）
│   ├── overlay.h / overlay.cpp       # オーバーレイポップアップUI
│   ├── hotkey.h / hotkey.cpp         # ホットキー登録・ディスパッチ
│   ├── indicator.h / indicator.cpp   # アニメーション付きステータスインジケーター
│   ├── switcher.h / switcher.cpp     # ウィンドウ切替パネル + 最前面固定
│   ├── topmost.h / topmost.cpp       # 最前面固定トグルの共通ロジック
│   ├── mouse_hook.h / mouse_hook.cpp # 右Alt+左クリック固定（WH_MOUSE_LL）
│   └── edge_flash.h / edge_flash.cpp # 画面端フラッシュ演出
└── build/                        # .gitignore で除外
```
