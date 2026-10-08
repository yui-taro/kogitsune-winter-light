# Jev 自体の特徴

調査日: 2026-10-02。以下はTypeSafe AIの**公式資料にある仕様**と、そこから導く**ゲームでの設計上の推論**を分けて記す。APIの実測はまだ行っていない。仕様と料金は変更され得るため、実装時に[公式モデル一覧](https://docs.typesafe.ai/models)を再確認する。

## 正体と動作

JevはTypeSafe AIの最初の「System One」意思決定モデル。`state`（判断材料）と`questions`（答えの形式を決めた質問）を送ると、**型付きの回答と確率**を返す。台詞、説明文、コード、画像は生成しない。公式は、十分な情報を与えられた人がすぐ判断できるような**短く焦点を絞った問い**を推奨している。[公式概要](https://docs.typesafe.ai/concepts/system-one)・[Primitives](https://docs.typesafe.ai/primitives)

HTTP APIは`POST https://api.typesafe.ai/v1/systemone`。`state`には文字列またはJSONのオブジェクト／配列を渡す。各質問は同じ`state`を見て**独立に**評価され、質問のIDをキーとして回答が返る。同じリクエスト内で「質問Aの答えを使って質問Bを考える」ことはできない。[公式API仕様](https://docs.typesafe.ai/api)・[State](https://docs.typesafe.ai/concepts/state)

ゲームに置き換えると、`state = 敵が見聞きした状況`、`question = 次の戦術はどれか`、`answer = 調査する／追跡する／増援を呼ぶ と各確率`。Jevは**選択**を担い、移動や経路探索などの実行はゲームコードが担う。この分担は公式の「コードと小さな判断を組み合わせる」方針に基づく設計案。

### 一般的なチャットLLMとの違い

| | Jev | 一般的なチャットLLM |
| --- | --- | --- |
| 主な出力 | 用意した型・候補への判断と確率 | 文章、コードなどの生成 |
| 行動の追加 | APIが定義した候補から選ぶ | 文章やツール引数として新しい内容を書ける |
| ゲームでの役割 | 敵の戦術選択、検知・評価 | 台詞や説明、長い計画の作成 |

「候補外の文字列を作らない」ことは**型の保証**であり、判断内容が常に正しい保証ではない。販売元の「ハルシネーションがない」という表現は、この出力形式の制約として解釈するのが適切。[公式発表記事](https://typesafe.ai/blog/introducing-system-one-models-and-jev)・[既知の弱点](https://docs.typesafe.ai/model-jaggedness/jev-1.13)

## 3種類の質問

| 質問型 | 返すもの | ゲームでの用途 |
| --- | --- | --- |
| **Choice** | 定義済み候補から1つ、全候補の確率、`confidence` | 敵の次の戦術、NPCの目的、イベント選択 |
| **Score** | 言葉で定義した段階の確率、加重平均した`score`、`confidence` | 警戒の質、交渉の危うさ、緊張の段階 |
| **Noul** | 命題が真である確率（0〜1） | 「音は囮だと思うか」 |

Choiceは最大**255候補**。候補名だけでなく、各候補が意味する行動と境界条件を`criteria`に書ける。どの候補も合わない可能性があるなら`none`などを用意する。返る`choice`は**確率が最大の候補**であり、多様な行動のために確率を使って抽選するならゲームコードで行う。[Choice](https://docs.typesafe.ai/primitives/choice)

Scoreは**2〜10段階**を言葉で定義する。`score`は各段階番号の確率加重平均なので、たとえば1.4のように段階の間を取る。ただし、1.0という値だけでは「段階1に集中」と「段階0と2に半分ずつ」を区別できない。`probabilities`と`confidence`も読む。精密な数量測定には使わない。[Score](https://docs.typesafe.ai/primitives/score)

Noulの0.5は「警戒度が中程度」ではない。質問した命題の**yesとnoが拮抗**しているという意味。警戒の強さを測りたいなら段階を定義したScoreを使う。Noulには別個の`confidence`フィールドがない。[Noul](https://docs.typesafe.ai/primitives/noul)

以下は**概念を示す架空の応答**。実際にAPIを呼んだ結果ではない。

```json
{
  "answers": {
    "next_action": {
      "type": "choice",
      "choice": "investigate",
      "probabilities": {"patrol": 0.15, "investigate": 0.7, "call_backup": 0.15},
      "confidence": 0.55
    }
  }
}
```

この例では、ゲームコードは`investigate`をそのまま採用するか、確率を使って候補から抽選する。どちらにせよ、実行直前にその行動が現在も合法か確認する。

## 確率・confidence・学習方針

Choice/Scoreの`probabilities`は候補・段階ごとの分布。`confidence`はその**集中度**を0〜1に要約した値で、単独の判断が正しい確率ではない。Choiceでは最大候補の確率と候補数から計算される。選択肢の数や定義を変えた前後では、同じ`confidence`でも単純比較しない。[公式Confidence解説](https://docs.typesafe.ai/confidence)

TypeSafeはRLCD（Reinforcement Learning for Calibrated Decisions）で確率を較正すると説明している。**較正は多くの予測をまとめて見た時の性質**で、個々の判断の正しさは保証しない。ゲーム内の盤面データで確率帯ごとの妥当性を検証する必要がある。[公式AI primer](https://docs.typesafe.ai/introduction/machine-learning-primer)・[System One概要](https://docs.typesafe.ai/concepts/system-one)

ゲームでの応用案は、選択肢の確率をプレイヤーに見せる、低confidence時にNPCが追加情報を探す、またはローカル方針を使うこと。ただし低confidenceは候補の定義が曖昧なだけかもしれない。しきい値は実測で決める。

## 現行の公開条件

[公式モデル一覧](https://docs.typesafe.ai/models)を2026-10-04に再確認すると、Jev 1.13 (`jev-1.13.0`) は **入力100万トークンあたり$0.042、出力トークン無料**。公開レート上限は **80リクエスト/秒、10万トークン/秒**。上限は調整中で変更される場合がある。1リクエスト最大64kトークン、`state`と最長の1質問の組み合わせは最大32kトークン。大きな状態を送れることと、大きな状態で精度がよいことは別。

入力はテキストのみ。文字列、テキストを含むJSONオブジェクト／配列を受け取るが、画像・音声・動画を直接入力できない。画面のピクセルから判断させる場合は、別の認識処理で状態に変換する必要がある。[公式モデル一覧](https://docs.typesafe.ai/models)

`jev-latest`は現時点で`jev-1.13.0`を指す安定版エイリアス。更新で中身が変わり得る。比較実験やリプレイで挙動を固定したい時はバージョンIDを指定し、レスポンスの`model`も記録する。[公式モデル一覧](https://docs.typesafe.ai/models)

TypeSafeの[発表記事](https://typesafe.ai/blog/introducing-system-one-models-and-jev)は同社条件での応答時間を**70〜500ms**としている。これは販売元による計測値で、自作ゲームの遅延保証ではない。地域、回線、入力サイズ、サーバー状態を含めてp50/p95とタイムアウト率を実測する。発表記事の速度・費用の比較は同社が選んだワークフローによるもので、一般的な全用途への倍率ではない。

公式の[クイックスタート](https://docs.typesafe.ai/introduction/quickstart)は `POST https://api.typesafe.ai/v1/systemone` と `TYPESAFE_API_KEY` を案内している。[公式JavaScript SDK](https://github.com/typesafe-ai/typesafe-sdk-js)は `@typesafe-ai/sdk`、Node.js 20以上。ブラウザゲームでもAPIキーはサーバー側に置く。

APIは`401`（認証）、`422`（入力形式）、`429`（レート超過）、`529`（過負荷）などを返す。公式SDKは`429`/`529`をバックオフで再試行する。ゲームでは再試行中もローカルの方針で進行させる。[公式APIエラー一覧](https://docs.typesafe.ai/api)

JavaScript SDKの既定値は**1試行のタイムアウト10秒、最大2回の再試行**。毎秒判断するNPCでは、そのまま結果を待つと古い判断が届き得る。リクエストごとの期限と結果の鮮度チェックが必要。[SDK設定](https://docs.typesafe.ai/sdk/javascript/api/interfaces/TypeSafeClientConfig)・[再試行設定](https://docs.typesafe.ai/sdk/javascript/api/interfaces/RetryPolicy)

## 得意な設計・苦手な設計

**強み:** 状況説明と事前に定義した行動候補をもとにした反復判断。複数の**独立した**質問を1リクエストで並列評価でき、共通の`state`を1回送れる。回答が型付きなので文章からコマンドを抽出する必要がない。[公式fan-outパターン](https://docs.typesafe.ai/patterns/fan-out)・[Primitives](https://docs.typesafe.ai/primitives)

[公式の既知の弱点](https://docs.typesafe.ai/model-jaggedness/jev-1.13)をゲームに引き寄せると、次の分担になる。

| Jevが苦手なもの | ゲーム側の対処 |
| --- | --- |
| 台詞・物語など新しい文章の生成 | 台詞テンプレートか別の生成モデルを使う |
| 数学、数え上げ、日付・時刻の比較 | 距離、残弾、クールダウンなどはコードで計算する |
| 多段の推論・複雑な間接参照 | 一度に決めることを小さくし、上位計画は別処理にする |
| 無関係な情報で膨らんだ状態 | 今回の判断に必要な事実だけ送る |
| 曖昧・矛盾した質問や選択肢 | 条件と候補の境界を明確に記す |
| 状態に混じる誘導文 | 自由入力をデータとして扱い、実行可能な候補とルールをコードで制限する |

**日本語:** 対応はしているが、公式には英語が主な学習言語で精度も最良とされる。同じ盤面の日本語／英語の入力を比較したい。[公式モデル一覧](https://docs.typesafe.ai/models)

**カスタマイズ:** 公式によると、顧客データでの個別ファインチューニングやLoRAは行わず、同じ重みを各アカウントに提供する。ゲームごとの性格・ルールは`state`、`instructions`、`criteria`で与える。[公式モデル一覧](https://docs.typesafe.ai/models)

**データ:** 公式は顧客のリクエスト・レスポンスをモデル学習に使わないとしている。保存期間など詳細は利用前に公式の法務文書を確認する。[公式モデル一覧](https://docs.typesafe.ai/models)

**ゲームへの組み込み:** [Master Customer Agreement](https://typesafe.ai/legal/mca)は、契約条件に従った顧客アプリへのAPI統合を扱っている。API自体の単体提供には制限があり、利用にはクレジットが必要。残高切れを含む運用上の確認は[残る調査と検証](remaining-validation.md)に整理した。

## 毎秒呼び出しの概算

費用の単純な概算は `NPC数 × 判断回数/秒 × プレイ秒数 × 1回の入力トークン数 ÷ 1,000,000 × $0.042`。例えば **10 NPC、各1回/秒、1時間、1回300入力トークン** なら1080万入力トークン、**約$0.45**。これは**NPCごとに独立リクエスト**を送る想定の計算で、実測ではない。複数NPCの質問を1回にまとめると共通`state`を再送しないため費用は変わる。質問や候補も入力トークンに含まれる。実際の`usage.input_tokens`で再計算する。[公式モデル一覧](https://docs.typesafe.ai/models)・[fan-out](https://docs.typesafe.ai/patterns/fan-out)

## まだ計測が必要な特徴

自作ゲームでの**判断品質、応答時間、同じ入力を繰り返した時の安定性、確率の較正、日本語と英語の差**は未確認。固定した盤面を複数回投入し、モデル版、質問文、候補、全確率、トークン数、応答時間を保存して比較する。APIが候補外の値を返さないことと、状況に合った候補を選ぶことは別の検証項目。

## 実例からの示唆

- [JevBall](https://github.com/atarikcaliskan/jevball): 合法候補と物理はローカルで作り、Jevは行動を選ぶ。関与度に応じて判断頻度を変え、遅延・失敗時はローカル方針に切り替える。開発者の短時間計測では平均366ms/リクエストだが、この値を自作ゲームの保証値とは扱わない。
- [Jev Town](https://github.com/NevaMind-AI/JevTown): Jevが行動選択、別のLLMが会話や記憶を担当する。多数NPCでは呼び出し数の制御が必要。
- [Jev Plays Pokémon Red](https://github.com/christianmat/jev-pokemon/blob/main/README.md): ゲーム状態から合法手を列挙し、経路探索やメニュー操作はハーネスが処理。判断ループを防ぐため失敗した選択肢の記録やチェックポイントを使う。
