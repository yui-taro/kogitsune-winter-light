# こぎつねと冬の灯り — 実装版

実装の仕組みを順に学ぶ場合は [日本語の学習ノート](../learning/README.md) を参照してください。設計に対する到達点は [実装状況](../design/implementation-status.md) にまとめています。

設計書に沿って、巣 → 森 → 神社（鳥居と倒木の二経路）→ 灯籠を持つ帰路 → 巣の結末を遊べる実装です。描画は raylib 6.0、ゲーム状態は C++17、Jev への判断要求は別プロセスの中継で処理します。

## 操作

| 入力 | 行動 |
| --- | --- |
| `WASD` / 矢印 | 移動 |
| `C` / 左 Ctrl | 伏せる・解除 |
| 左 Shift | 走る・解除 |
| `Q` | 近くの足跡を尾で払う |
| `E` | 小枝・鈴・灯籠に触れる／灯籠を置く |
| 右下の四ボタン | 同じ四行動をマウスで操作 |
| `Esc` | 一時停止・再開 |
| `R` | 捕獲後の再挑戦／結末後の最初から再開 |

巣では最初に小枝に触れ、左上の小道へ進みます。森の鳥居か右の倒木から神社に入り、社の前の唯一の灯籠を取ります。倒木の下を抜けるには伏せる必要があります。

短く押した移動キーも受け付けます。`C` などを押して文字入力欄に文字が出る時は、キーボードの入力先がゲームではありません。ゲーム画面を一度クリックしてから操作してください。ゲームから別のアプリへ切り替えると進行は一時的に止まります。

## macOS でビルド

raylib 6.0 を `RAYLIB_DIR` の場所にビルドしてから、以下を実行します。既定値は `/private/tmp/jev-raylib-6.0` です。

```sh
make -C game RAYLIB_DIR=/path/to/raylib
make -C game test
make -C game bundle RAYLIB_DIR=/path/to/raylib
```

開発用実行ファイルは `game/build/foxgame`、配布しやすいアプリは `game/build/こぎつねと冬の灯り.app` です。Mac ビルドには Xcode Command Line Tools と libcurl が必要です。

## Windows でビルド

Visual Studio 2022、CMake 3.22 以降、vcpkg を使います。PowerShell でリポジトリのルートから:

```powershell
git clone https://github.com/microsoft/vcpkg.git vcpkg
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics
.\vcpkg\vcpkg.exe install curl:x64-windows-static
cmake -S game -B game/build-windows -A x64 -DCMAKE_TOOLCHAIN_FILE="$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build game/build-windows --config Release
ctest --test-dir game/build-windows -C Release --output-on-failure
```

CMake が raylib 6.0 を取得し、`Release/foxgame.exe` の隣へ `assets` をコピーします。実行時は exe と `assets` フォルダを一緒に置いてください。Windows 実機での動作確認と配布パッケージの署名は、まだ完了していません。

## Jev 中継

ゲームに API キーを埋め込みません。中継を置く環境だけに `TYPESAFE_API_KEY` を設定し、`server/relay.mjs` を Node.js 20 以降で起動します。開発中は中継を `127.0.0.1:8787` に限定しています。

```sh
TYPESAFE_API_KEY=your-key node server/relay.mjs
JEV_RELAY_URL=http://127.0.0.1:8787/decision game/build/foxgame
```

API キーなしで通信を試す場合は `JEV_MOCK=1 node server/relay.mjs` を使います。`JEV_RELAY_URL` を指定しない時は画面に「ローカル試作」と表示し、同じ観測契約を読む決定的な代替方針で動きます。中継を指定した後に短く応答できない場合も代替方針を使い、画面にその状態を示します。実際の Jev 応答は API キーがまだないため未検証です。

実際の Jev 呼び出しには TypeSafe の Choice を使い、返答の `choice` だけを採用します。`probabilities` は抽選に使いません。中継は狐の真の位置や巣の事情を受け取らず、ゲーム側は試行 ID・判断 ID・改訂番号・合法候補を検査して古い返答を破棄します。

中継は既定で固定版 `jev-1.13.0` を使います（`JEV_MODEL` で変更可）。実 API の応答ごとに、応答したモデルと入力トークン数を標準出力へ `{"event":"jev_usage","model":...,"inputTokens":...}` の形で1行記録します。

## 検証

```sh
make -C game test build/test_bridge
cd server && node --test test.mjs
```

最後のテストはローカルポートを使い、模擬中継と C++ クライアントを実際に HTTP で接続します。模擬 Jev を相手に 429/529・形式不正・候補外の応答も確認します。ゲームの状態テストは、観測の境界、候補外の拒否、古い返答、捕獲後の記憶復元、神社の右経路、全場面の結末到達を確認します。GitHub Actions の `Tests` ワークフローが push と PR で同じテストを Ubuntu と macOS で実行します。

どのテストが設計書の要件 ID・検証ゲート・固定盤面に対応し、何が手動・実機・実API での確認待ちかは [テスト対応表](../tests/README.md) にまとめています。自動テストの通過だけでは公開品質の合格になりません。

## 素材と現状

画面は固定背景と透過スプライトの 2.5D 試作です。子狐の通常歩行・走り・伏せ歩きに、それぞれ横・斜め前・正面・斜め後ろ・背面の4コマを用意しました。左右反転を合わせて8方向、合計60コマ分の画像を使います。コマは実際に進んだ距離で切り替え、立ち止まると止まります。元画像は `art-source`、ゲーム用に足元を揃えた画像は `assets` にあり、`tools/prepare_fox_animation.py` で再生成できます。効果音は実行時に合成します。フォントのライセンスは `assets/OFL.txt`、JSON と raylib のライセンスは `vendor` にあります。公開時の中継先・利用者認証・費用上限は未決定です。
