# 『こぎつねと冬の灯り』制作方式の調査

調査日: 2026-10-04。対象は[物語設計](../fox-story-design.md)と[画面案](../fox-visual-implementation.md)。目的は、**かわいい子狐・美しい雪の神社・状況で迷う狛犬**を、C++中心の小規模制作で成立させること。

## 現時点の判断

1. **第一候補:** Blender 5.2 LTSで子狐・狛犬・背景を制作し、同じ固定斜めカメラから画像連番と背景レイヤーをレンダリング。C++ + raylib 6.0で2.5Dとして合成する。[Blender画像出力](https://docs.blender.org/manual/en/5.2/render/output/properties/output.html)、[透明フィルム](https://docs.blender.org/manual/en/latest/render/eevee/render_settings/film.html)、[raylib機能一覧](https://www.raylib.com/cheatsheet/cheatsheet.html)、[raylib公式CMake雛形](https://github.com/raysan5/raylib-game-template/blob/main/CMakeLists.txt)。
2. **Jevの仕事:** 狛犬が観測した証拠と固定した性格から、少数の合法な行動意図をChoiceで選ぶ。位置・経路・捕獲・物語の必須条件はゲーム側で管理。[Jev Choice](https://docs.typesafe.ai/primitives/choice)、[API](https://docs.typesafe.ai/api)。
3. **最大の未実証点:** 狐が灯籠を運ぶ全位置で、レンダー済みの毛・雪・石と動く光が自然に一体化するか。これが不自然なら、描画だけリアルタイム3Dへ変更する。1画面の映像試作で判定する。

## 資料

| ファイル | 内容 |
| --- | --- |
| [rendering-pipeline.md](rendering-pipeline.md) | Blenderからraylibまでの座標、素材、描画、制作順 |
| [character-art.md](character-art.md) | 子狐と狛犬の造形、表情、動き、制作・評価方法 |
| [jev-integration.md](jev-integration.md) | API境界、状態、応答の鮮度、失敗時の処理 |
| [new-tech.md](new-tech.md) | 3D経験済みという前提で新しく挑戦する技術の優先順位 |
| [agents-and-skills.md](agents-and-skills.md) | ネットで見つかった公式・コミュニティ製のスキル／エージェントと配置案 |
| [validation.md](validation.md) | 1画面試作の合格条件、機材・依存関係の確認事項 |

## 根拠の読み方

- **資料で確認済み:** Blenderの画像連番／透過、raylibの描画機能、JevのHTTP API／Choiceの型、Codexのスキル・カスタムエージェント配置。
- **設計上の選択:** カメラ角度、方向数、アニメーション量、灯籠光の2D合成、フォルダー構成、品質基準。公式資料が特定ゲームの見た目や面白さを保証するわけではない。
- **実機未検証:** このMacでのビルド、Blender実レンダー、raylibでの画質とフレームレート、Jevの判断品質／遅延／費用。確認方法を[validation.md](validation.md)に示す。

## 実装時のフォルダー構成案

```text
.
├── AGENTS.md                    # リポジトリ全体の短い制作規約。作成は実装開始時
├── .agents/skills/              # プロジェクト専用の反復作業スキル
│   ├── fox-art-review/SKILL.md  # 子狐・狛犬のシルエットと実画面サイズを確認
│   ├── blender-export/SKILL.md  # レンダー設定と書き出しの再現手順
│   └── jev-scenario-eval/SKILL.md # 固定盤面で狛犬判断を比較
├── .codex/agents/               # 必要になった時だけ使う狭い役割のCodexエージェント
│   ├── art-reviewer.toml
│   └── jev-evaluator.toml
├── design/                       # 美術設定、画面・操作・演出の仕様
├── art/
│   ├── references/              # 使用許諾と出典を記録した参考資料
│   ├── blender/                 # .blend原本、共通カメラ・光・素材
│   └── exports/                 # Blenderからの連番と背景の中間出力
├── tools/                       # Python書き出し、アトラス作成、素材検査
├── game/
│   ├── CMakeLists.txt
│   ├── src/                     # C++: world, render, ui, npc, net, save
│   ├── assets/                  # 同梱する最適化済み素材とmanifest
│   └── tests/                   # 座標・ソート・データ契約などの検査
├── server/                      # Jev中継とAPIキー管理、モック
├── tests/fixtures/             # 盤面と期待するNPC傾向の固定ケース
├── research/production/        # この調査と検証記録
└── prototypes/courtyard/       # 最初の完成品質に近い境内1画面
```

現時点では調査資料だけを作り、未使用の実装フォルダーや外部スキルは作成・導入しない。`AGENTS.md`や`SKILL.md`は読むエージェントの行動を変えるため、実際のワークフローが固まってから内容を決める。[CodexのAGENTS.md](https://learn.chatgpt.com/docs/agent-configuration/agents-md)、[Codexのスキル配置](https://learn.chatgpt.com/docs/build-skills)、[カスタムエージェント配置](https://learn.chatgpt.com/docs/agent-configuration/subagents)。
