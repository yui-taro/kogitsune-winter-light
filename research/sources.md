# 参照資料

確認日: 2026-10-02。仕様は公式を優先し、ゲーム例は制作者のリポジトリを参照した。

## TypeSafe AI 公式

- [System One概要](https://docs.typesafe.ai/concepts/system-one) — Jevの用途と入出力。
- [Primitives](https://docs.typesafe.ai/primitives) — 3種類の質問、独立した並列評価、質問の作り方。
- [Choice](https://docs.typesafe.ai/primitives/choice) — 候補の確率、選択肢の設計、255候補の上限。
- [Score](https://docs.typesafe.ai/primitives/score) — 段階評価と加重平均の読み方。
- [Noul](https://docs.typesafe.ai/primitives/noul) — yes確率と段階評価との違い。
- [State](https://docs.typesafe.ai/concepts/state) — 判断材料の形式と構造。
- [API仕様](https://docs.typesafe.ai/api) — Choice、Score、Noulの形式。
- [モデル一覧](https://docs.typesafe.ai/models) — 単価、上限、入力形式、言語、バージョン。
- [Quick start](https://docs.typesafe.ai/introduction/quickstart) — エンドポイントとAPIキー。
- [Confidence](https://docs.typesafe.ai/confidence) — 確率とconfidenceの解釈。
- [AI primer](https://docs.typesafe.ai/introduction/machine-learning-primer) — RLCDと確率の較正。
- [Advanced: structure](https://docs.typesafe.ai/primitives/advanced) — 構造化した質問と候補の説明。
- [既知の弱点](https://docs.typesafe.ai/model-jaggedness/jev-1.13) — 数字、長い状態、生成など。
- [複数質問を1回で送る方法](https://docs.typesafe.ai/patterns/fan-out) — バッチ判断。
- [公式JavaScript SDK](https://github.com/typesafe-ai/typesafe-sdk-js) — Node.js向けクライアント。
- [JS SDKの設定](https://docs.typesafe.ai/sdk/javascript/api/interfaces/TypeSafeClientConfig) — タイムアウトやブラウザでのキー露出。
- [JS SDKの再試行設定](https://docs.typesafe.ai/sdk/javascript/api/interfaces/RetryPolicy) — 既定の再試行回数と対象。
- [発表記事](https://typesafe.ai/blog/introducing-system-one-models-and-jev) — メーカー公表の速度とその計測条件、ゲームデモの説明。
- [法務資料一覧](https://docs.typesafe.ai/legal) — 契約・個人データに関する一次資料への入口。
- [Master Customer Agreement](https://typesafe.ai/legal/mca) — 顧客アプリへの統合、クレジット、利用上の制限。
- [Privacy Policy](https://typesafe.ai/legal/privacy-policy) — 入力の扱いと保持に関する説明。

## ゲーム実装例（各作者による報告）

- [JevBall](https://github.com/atarikcaliskan/jevball) — 22人の行動選択、判断頻度の調整、遅延時のローカル代替、短時間の費用計測。
- [Jev Town](https://github.com/NevaMind-AI/JevTown) — Jevの行動選択と別LLMの会話・記憶を組み合わせた街の試作。
- [Jev Plays Pokémon Red](https://github.com/christianmat/jev-pokemon/blob/main/README.md) — 合法手の列挙、ループ回避、長時間実験の記録。
- [TypeSafe Playground](https://github.com/TypeSafeAI/typesafe-playground) — 公式側のDoom風ゲーム、ロボット競技などの小さな判断実験。
- [Heist One](https://github.com/AbdelStark/heist-one) — 警備員の判断を観察できるステルス試作。
- [Last Exit](https://github.com/0x963d/last-exit) — プレイヤーの発言と証拠を受けて検査官が行動を選ぶ検問ゲーム。
- [Jev Board Game](https://github.com/KeWang0622/jev-board-game) — 社会的推理ゲームで疑念の確率を扱う試作。
- [jev-fps](https://github.com/Waxmell114514/jev-fps) — 標的、発砲、テンポの質問をまとめた射撃ゲーム。
- [Jev Practice Speed](https://github.com/tubone24/jev-practice-speed) — 手札と場が変わった時にカードを選ぶゲーム。

実装例の速度・費用・成功率は、それぞれの環境での作者報告。自作ゲームの保証値として流用せず再測定する。
