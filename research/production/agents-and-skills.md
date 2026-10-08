# 使えそうなスキル・エージェント・MCPの調査

調査日: 2026-10-04。公式配布に限定しない。**ここでいう制作エージェントは開発作業を助けるCodex等の役割であり、ゲーム内の狛犬NPCとは別。** 候補は未インストール。配布元のREADME・SKILL.mdを確認した範囲で評価する。

## 採用候補

| 優先 | 候補 | 使いどころ | 現案との適合・注意 |
| --- | --- | --- | --- |
| 高 | [TypeSafe公式 Agent Skill](https://docs.typesafe.ai/agent-skill) | Jevの質問設計、API契約、状態と応答の評価 | この作品に直結。公式は`npx skills add typesafe-ai/skills --skill typesafe-ai`を案内。導入時に最新版の[SKILL.md](https://github.com/typesafe-ai/skills/blob/main/skills/typesafe-ai/SKILL.md)も確認 |
| 高 | [Blender Agent Studio](https://github.com/ifBars/blender-agent-studio)（コミュニティ製） | `.blend`とPython原本を残すモデル／場面制作、複数角度の確認、出力検査 | 本作の「再生成できる3D→2.5D素材」に合う。Blender 5.2 LTSとBunを前提。配布元の少数の比較試験は品質保証ではない |
| 中 | [MCP for Blender](https://github.com/ahujasid/mcp-for-blender)（コミュニティ製） | Blender画面内の小物配置、材質・カメラ調整、実際のシーン確認 | 直接操作に向く。Blender Agent Studioと同じ目的で重複する部分があるため、最初は**どちらか一方**で灯籠1個を試す。アドオンとサーバーが必要 |
| 中 | [character-image-to-3d](https://github.com/laxmimerit/Claude-Skills/blob/main/character-image-to-3d/SKILL.md)（コミュニティ製） | 子狐の設定画から3D形状へ移る時の、比率・隠れた面・シルエット・変形の検討 | 参照手順として有用。Claude/Blender MCP寄りで、そのままCodexの自動化が動くかは未検証。技能の一部だけ参考にする |
| 参考 | [OpenAI Game Studioのsprite-pipeline](https://github.com/openai/plugins/blob/main/plugins/game-studio/skills/sprite-pipeline/SKILL.md) | フレームの基準位置・透過・プレビュー確認 | ブラウザゲーム用でAI生成2Dストリップを想定。本作はBlenderレンダーなので、足元ピボットと連番QAの考え方だけ借りる |
| 見送り | [GameStudio](https://github.com/bullish0x/GameStudio)（コミュニティ製） | 大規模な役割別ゲーム制作 | 多数のエージェント／スキルを含むが、主要な実装対象はWeb系エンジン。本作のC++ + raylib短編には規模が大きい |

コミュニティ製は、READMEの機能表が実機での有効性を証明するわけではない。導入前にライセンス、依存関係、実際に実行されるスクリプト、更新状況を確認し、**灯籠1個・狐の粗いモデル1体**で試す。MCP for Blenderは作者自身が第三者製と明記し、テレメトリー設定も公開している。[配布元README](https://github.com/ahujasid/mcp-for-blender)。

## この作品用に作ると価値が高いスキル

汎用スキルを大量導入するより、試作から手順が固まったものを`.agents/skills/`へ置く。Codexはリポジトリの`.agents/skills`を読み、各スキルの`SKILL.md`を必要時に利用できる。[OpenAI Docs: Build skills](https://learn.chatgpt.com/docs/build-skills)。

| 予定スキル | 入力 | 出力と検査 |
| --- | --- | --- |
| `fox-art-review` | 子狐・狛犬の通常画面、輪郭画像、10秒動画 | 耳・尾・顔・行動・光の読め方を同じチェック項目で評価 |
| `blender-export` | `.blend`、行動・向き・カメラ設定 | RGBA連番、背景・前景、manifest、欠落・ピボット・透過の検査結果 |
| `jev-scenario-eval` | 固定盤面、性格定義、候補、Jev応答ログ | ルール版との差、性格差、古い応答・候補外値・遅延の結果 |

スキルに置くのは**反復手順**、`AGENTS.md`には全員に常時必要な短い制作規約、`research/`には根拠と調査履歴を置く。実装コードそのものは`game/`と`server/`。この分担は[CodexのAGENTS.md](https://learn.chatgpt.com/docs/agent-configuration/agents-md)と[スキル仕様](https://learn.chatgpt.com/docs/build-skills)に沿う。

## 必要になった時だけ使う制作エージェント

Codexはプロジェクト用カスタムエージェントを`.codex/agents/*.toml`で定義できる。[OpenAI Docs: Subagents](https://learn.chatgpt.com/docs/agent-configuration/subagents)。最初の候補は次の2役。

| 役割 | 担当 | 与える情報・制約 |
| --- | --- | --- |
| `art_reviewer` | 子狐・狛犬・灯籠の実画面サイズの読みやすさと、2.5D合成の破綻をレビュー | 画像、動画、[造形基準](character-art.md)、[品質基準](validation.md)。原本を勝手に書き換えない |
| `jev_evaluator` | 固定盤面のJev判断を比較し、性格の一貫性・観測外情報・遅延を報告 | 盤面、応答ログ、[Jev契約](jev-integration.md)。ゲーム内ルールとAPI呼び出しを混同しない |

エージェントを増やすこと自体は技術的成果ではない。境内1画面の試作で、独立したレビューが必要と分かった時に定義する。現時点では**配置案だけ**にし、起動・導入は行わない。
