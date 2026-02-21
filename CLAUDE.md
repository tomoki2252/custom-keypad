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

## ホットキー

- Ctrl+Alt+M: ホットキーON/OFFトグル（画面左下のインジケーター連動）
- Ctrl+Shift+P: テスト表示（マウスカーソル付近にポップアップ）

## Git ワークフロー

- ブランチ: `main`
- リモート: `origin` (git@github.com:tomoki2252/custom-keypad.git)
- コミットメッセージは英語

## ディレクトリ構成

```
custom-keypad/
├── CMakeLists.txt
├── CLAUDE.md
├── .gitignore
├── cmake/
│   └── mingw-w64-x86_64.cmake   # MinGW-w64クロスコンパイル用ツールチェーン
├── src/
│   ├── main.cpp                      # WinMainエントリ、メッセージループ、トグル管理
│   ├── overlay.h / overlay.cpp       # オーバーレイポップアップUI
│   ├── hotkey.h / hotkey.cpp         # ホットキー登録・ディスパッチ
│   └── indicator.h / indicator.cpp   # アニメーション付きステータスインジケーター
└── build/                        # .gitignore で除外
```
