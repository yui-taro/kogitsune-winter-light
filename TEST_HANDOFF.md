# テスト整備の引き継ぎ（2026-10-07）

## 依頼と判断基準

依頼は「設計書に基づいて網羅的なテストを作成し、合格を品質担保に使う」。
最上位の試験方針は [`design/verification.md`](design/verification.md)。要件 ID は
[`design/requirements.md`](design/requirements.md)、ゲーム状態とJev契約は
[`design/basic-design.md`](design/basic-design.md) と
[`design/komainu-jev.md`](design/komainu-jev.md) を参照する。

検証計画は、絶対条件、初見プレイ・画質などの人による評価、速度・費用などの実測を分けている。
自動テストの通過だけで V0〜V6 の総合合格や公開品質を宣言しない。

## この作業で追加・変更したもの

- [`tests/test_contract.cpp`](tests/test_contract.cpp): J-01〜05 の固定盤面、観測の重複と改訂、候補外・古い応答・再適用の拒否、石像の変化、移動・足跡・尾で払う・鈴・灯籠、CP-B/C の復元を追加。
- [`server/test.mjs`](server/test.mjs): 観測外情報を除外する正規化、ローカル方針、異常入力、HTTP 応答、C++ ブリッジの成功・通信失敗・古い応答の結合テストを追加。固定ポートを避け、OS が割り当てたループバックポートを使う。
- [`tests/test_bridge.cpp`](tests/test_bridge.cpp): 成功・fallback・stale の3モードを追加。検査は Release ビルドでも働く明示的な例外に変更。
- [`tests/test_world.cpp`](tests/test_world.cpp): `NDEBUG` で消えない検査に変更。既存の通し進行・経路・復元テストは保持。
- [`server/relay.mjs`](server/relay.mjs): 正規化と模擬判断をテストから import 可能にし、直接起動した時だけサーバーを listen する。起動メッセージに実際の割当ポートを出す。
- [`game/Makefile`](game/Makefile): `make -C game test` に新テストを追加。`build/test_bridge` は別ターゲット。
- [`game/CMakeLists.txt`](game/CMakeLists.txt): `design_contract` を CTest に追加。

リポジトリのルートに `.git` はなく、ここではコミット・差分表示ができない。変更前後の厳密な差分が必要なら、元のリポジトリと照合する。

## 実行済みの検証

この環境は macOS。以下は 2026-10-07 に通過した。

```sh
make -C game test build/test_bridge
node --test server/test.mjs
```

結果: C++ の既存状態テストと新規契約テスト9件が成功。Node のテスト7件が成功。
Node の結合テストはループバックポート使用のため、ローカル環境では実行権限が必要だった。
`cmake` コマンドがこの環境にないため CMake/CTest と Windows Release ビルドは未確認。
実Jev APIへの接続、Windows実機、初見プレイ、画質・音・性能・費用は未試験。

## 2回目の作業（2026-10-07）で行ったこと

- [`tests/README.md`](tests/README.md): V0〜V6、要件 ID、J-01〜J-11 とテストの対応表を作成。自動／一部自動／未実装／手動を区別し、テストで見つかった設計との差を記載。
- [`.github/workflows/tests.yml`](.github/workflows/tests.yml): push と PR で C++ テストと Node 中継テストを Ubuntu・macOS で実行する CI を追加。[`windows-build.yml`](.github/workflows/windows-build.yml) にも PR トリガーを追加。**GitHub 上での実行結果は未確認**（Ubuntu ではローカル未検証）。
- [`tests/test_contract.cpp`](tests/test_contract.cpp): J-06（結末で判断要求・捕獲が起きない）を追加。J-11 として CP-C 保存後の盗難の観測が復元で消えることを追加検査。
- [`server/relay.mjs`](server/relay.mjs): 上流 URL を `TYPESAFE_API_URL` で差し替え可能にした（既定は公式 URL のまま）。Jev が形式不正 JSON を返した時に 400 と誤応答していた不具合を 503 に修正。
- [`server/test.mjs`](server/test.mjs): 模擬 Jev サーバーで、Jev へ届く本文に観測外情報・キー・識別子が含まれないこと（J-07）、429/529/401/422・形式不正・候補外が 503 になり高速再試行しないこと（V4/J-08）を追加。
- [`game/README.md`](game/README.md): 検証手順、CI、対応表へのリンクを更新。

検証結果（macOS、2026-10-07）: `make -C game test build/test_bridge` で契約テスト10件と状態テストが成功。`node --test server/test.mjs` で9件成功。`CXX=g++` のクリーンビルドでも同じ。

## 次に行う作業

1. GitHub へ push して `Tests` と `Windows build` の CI 結果を確認する（このフォルダに `.git` がないため未実施）。
2. `server/relay.mjs` の `model: 'jev-latest'` を固定版へ変え、応答の `model` と `usage.input_tokens` を記録する（実API試験の前提）。
3. 429/529 のバックオフ方針を決めて実装する。現在は再試行せず 503。
4. 「巣で弟妹を見た」観測をゲームに入れるか決める（J-06 の前提）。
5. 実APIでの J-01〜J-05・J-08・J-09 の固定盤面評価（APIキー、固定モデル版、費用記録が必要）。
6. A-04 の予兆、F-05 の捕獲理由、F-06 の対処の残り方を実画面または追加の状態テストで検証する。
7. V5・V6 の受け入れ記録。テストが通っても「未測定」を合格扱いしない。

## 注意点

- `server/test.mjs` の結合テスト前に `game/build/test_bridge` のビルドが必要。
- `tests/test_contract.cpp` は意図的に世界状態の確定的な契約を検証しており、Jev の判断品質そのものは検証しない。
- `design/verification.md` の主観評価と対象機材・費用依存の閾値は、現時点で正式値が未設定。試験後に都合よく基準を決めない。
- `server/relay.mjs` の import 時の起動条件を変更したため、Node の直接起動と import の両方を継続して検証する。
