# 狛犬のJev実装境界

> この文書は初期調査。確定した判断方針は[判断記録 D-17](../../design/decision-log.md)と[狛犬・Jev仕様書](../../design/komainu-jev.md)を参照する。Choiceが返す確率値をゲームの抽選には使わない。

## 「毎秒AI」の扱い

Jevは`state`と型付き`questions`を送り、Choiceなら候補と確率を返すHTTP API。これは**行動意図を選ぶ層**に使う。[TypeSafe API](https://docs.typesafe.ai/api)、[Choice](https://docs.typesafe.ai/primitives/choice)。走行・視界・音・衝突・経路探索・灯籠の所有権はC++のゲーム状態が確定させる。応答のたびに全行動を再計画すると揺れや遅延が目立つため、通常は「証拠を発見」「灯籠を奪われた」「追跡先で空振り」「巣を見た」などの**出来事で判断**する。毎秒方式は比較試験の対象として残す。

## ゲーム内の情報モデル

```text
GroundTruth       狐の本当の位置、足跡・鈴・灯籠の実状態。ゲームだけが保持
Observation       狛犬が今見た／聞いた事実。検知処理が生成
BeliefMemory      狛犬が最後に知った位置、調査済み地点、誤認の履歴
Personality       灯籠への忠実さ、慎重さ、幼い動物への情。固定
DecisionContext   Personality + Observation + 関連するBeliefMemory
```

Jevへ送るのは`DecisionContext`だけ。たとえば狐が木陰にいるだけなら座標は送らない。記憶は「見た」「聞いた」「推測した」を分け、最後の観測時刻を添える。狛犬の性格を毎回同じ欄で渡し、変化した経験と混ぜない。これは性格の一貫性を**試すための設計**であり、モデルが必ず望ましい人格を演じる保証ではない。[State](https://docs.typesafe.ai/concepts/state)、[TypeSafe公式Agent Skill](https://github.com/typesafe-ai/skills/blob/main/skills/typesafe-ai/SKILL.md)。

## 行動選択の契約

入力例は概念仕様。実際のAPIスキーマは実装時に現行の[API仕様](https://docs.typesafe.ai/api)で再確認する。

```json
{
  "npc": "komainu_1",
  "personality": {"duty": "守る", "caution": "高い", "mercy_for_cubs": "高い"},
  "known_lantern": "台座にある",
  "observation": {"kind": "bell", "where": "左の木", "age_s": 0},
  "memory": {"last_false_alarm": "左の木", "last_seen_fox": null},
  "legal_intents": ["guard_lantern", "inspect_bell", "inspect_tracks"]
}
```

実際にはサーバーがこの情報を`state`へ入れ、`questions.next_intent`をChoiceとして送る。候補の説明は「何をするか」と「どの状況で選ぶか」を短く書く。候補がない時の`wait_and_watch`も用意する。**合法候補の列挙はC++側、選択の妥当性と期限の確認もC++側**。Choiceの`choice`は最大確率の候補であり、`confidence`は分布の集中度なので、判断の正しさや性格の保証としては使わない。[Choice応答仕様](https://docs.typesafe.ai/primitives/choice)。

TypeSafeへ渡すHTTP本文の形は次の通り。`criteria`はその時点の合法候補だけから組み立てる。モデルIDは試験時に固定版へ置き換え、再現性を確保する。

```json
{
  "model": "jev-latest",
  "state": {
    "personality": {"duty": "高い", "caution": "高い", "mercy_for_cubs": "高い"},
    "known_lantern": "台座にある",
    "observation": {"kind": "bell", "where": "左の木"},
    "memory": {"last_false_alarm": "左の木"}
  },
  "questions": {
    "next_intent": {
      "type": "choice",
      "instructions": "この狛犬が今、最初に取る行動はどれか。見聞きした事実と性格に従う。",
      "criteria": {
        "guard_lantern": "灯籠の近くに残り、守りながら観察する",
        "inspect_bell": "左の木の鈴を調べる",
        "wait_and_watch": "今は移動せず、次の手掛かりを待つ"
      }
    }
  }
}
```

HTTPは`POST https://api.typesafe.ai/v1/systemone`、`Authorization: Bearer <API_KEY>`。応答の`answers.next_intent.choice`を読む。[TypeSafe API仕様](https://docs.typesafe.ai/api)。ゲームから中継サーバーへの通信形式はこれとは別に定義する。

## 非同期処理と失敗時

1. ゲームが`decision_id`、判断に関わる出来事だけで増える`decision_revision`、対象NPC、候補集合、締切時刻を付けて中継サーバーへ送る。APIキーは中継サーバーだけが保持する。
2. 中継サーバーはJevへ問い合わせ、応答とモデル版・所要時間・入力トークン数を返す。秘密鍵はログに含めない。
3. ゲームは受信時に、NPCがまだ存在し、`decision_revision`が一致し、候補が現在も合法で、締切内なら採用する。新たな足跡の発見、灯籠の盗難、調査の空振りなどでrevisionを進め、古い応答を捨てる。
4. タイムアウト、通信断、形式エラー、候補外の値、レート制限の場合はローカル方針を使う。灯籠があるなら守る、盗難後は最後に見た証拠を調べるなど、プレイが止まらない短い方針。
5. 連続失敗時は新規問い合わせを一定時間止める。再試行で1イベントが多重に適用されないよう`decision_id`を記録する。

TypeSafeはHTTP `401/422/429/529`などを定義し、`429/529`ではバックオフを勧める。[APIエラー](https://docs.typesafe.ai/api)。中継サーバーでの再試行方針とゲーム内締切は別にし、ゲームの次のフレームをネットワーク待ちで止めない。締切値は実測から決める。

## プレイヤーに「AIでそんなことができる」と伝える

Jevの価値は意外な選択そのものより、**狛犬が見たものと性格を踏まえた違い**が読めること。鈴だけなら社に残り、足跡と鈴が同時なら迷い、過去に同じ鈴で空振りしたなら慎重になり、幼い弟妹を見れば追い方を変える。ゲーム内では耳・視線・足取り・灯りを使って予兆を出す。クリア後の短い「狛犬の見聞きした記録」で、観測→選択→結果を振り返れる。ただし記録は**ゲームが保存した観測事実と選択**で組み立て、Jevが返していない思考理由を捏造しない。

評価ケースは`tests/fixtures/`に置く想定。同じ盤面を固定ルール版、Jev版、性格を入れ替えたJev版で比較し、選択の変化がプレイヤーに見えるか確認する。詳細は[validation.md](validation.md)。
