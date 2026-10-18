# こぎつねと冬の灯り

[![紹介動画（40秒）](https://github.com/yui-taro/kogitsune-winter-light/releases/download/v1.0.0/kogitsune-poster.jpg)](https://github.com/yui-taro/kogitsune-winter-light/releases/download/v1.0.0/kogitsune-trailer-40s.mp4)

凍える弟妹のために、子狐が山の神社の灯籠を盗み出す 2.5D ステルスゲームです。灯籠を守る狛犬の次の一手は、AI「Jev」が、狛犬が実際に見聞きした手掛かりだけで選びます。

- **ブラウザで遊ぶ：** https://yui-taro.github.io/kogitsune-winter-light/ （インストール不要。パソコンのブラウザ向け）
- **ダウンロード版：** [最新リリース](https://github.com/yui-taro/kogitsune-winter-light/releases/latest) から Mac（Apple Silicon）版または Windows 版
- **紹介動画：** [40秒版](https://github.com/yui-taro/kogitsune-winter-light/releases/download/v1.0.0/kogitsune-trailer-40s.mp4)・[30秒版](https://github.com/yui-taro/kogitsune-winter-light/releases/download/v1.0.0/kogitsune-trailer-30s.mp4)
- **ビルド・操作・Jev 中継：** [game/README.md](game/README.md)

---

# Jev ゲーム企画・調査

TypeSafe AI の意思決定モデル Jev を使うゲームの調査作業場。ゲーム本体の方式を決める前に、小さな実験で面白さと運用条件を確かめる。

## 現在の結論

- 実装前に[要件定義書（草案）](design/requirements.md)を作成した。ユーザーが示した希望、設計提案、未決定事項を分けている。制作コードの追加は、実装の指示があるまで行わない。
- [ゲーム設計書（草案）](design/game-design.md)に、最初のJev見せ場、狐の四行動、足跡・鈴・光のルール、狛犬の対処可能な判断、盗難後の帰路を具体化した。
- [物語・レベル設計書（草案）](design/story-and-levels.md)に、巣から神社までの6場面、神社エリアの二つの出入口、チェックポイント、狛犬が巣に着く因果、場面別の必要素材を整理した。
- [美術・音・画面方針書（草案）](design/art-and-audio.md)に、画面で目に入る順番、色と光の規則、子狐・狛犬の造形基準、狛犬の予兆の演技、音の役割、評価試験をまとめた。V-01比較で画風は**「立体のある絵本映画」**に決めた。
- [基本設計書（草案）](design/basic-design.md)に、画面遷移、ゲーム状態の正本、狛犬の観測からJev判断までの境界、捕獲後の復元、通信障害、描画方式の検証条件をまとめた。
- [画面・操作仕様書（草案）](design/ui-and-controls.md)に、四行動の操作と表示、対象選択、場面ごとの案内、狛犬の予兆、ポーズ・再挑戦・通信画面をまとめた。
- [狛犬・Jev仕様書（草案）](design/komainu-jev.md)に、観測・記憶、合法な行動意図、判断の契機、応答の鮮度、障害時の代替方針を定義した。
- [技術・素材設計書（草案）](design/technical-and-assets.md)に、世界座標、描画層、Blenderからの書き出し、素材manifest、Windowsビルド候補を整理した。
- [検証・受け入れ計画書（草案）](design/verification.md)に、要件ID別の確認方法、固定盤面、画質・遅延・費用・Windowsの検証ゲートを定めた。
- [公開・運用設計書（草案）](design/release-and-operations.md)と[判断記録](design/decision-log.md)に、GitHub公開物、秘密鍵・素材権利、障害時の案内、採用待ちの判断を整理した。
- 完成版は巣から神社までのWindows向け短編。開始後5〜10分以内に狛犬のJev判断が映える場面を置く。GitHubでソースとWindows実行版を公開する。制作スケジュールはユーザーが管理する。
- [設計ドキュメント一覧](design/document-index.md)に、要件定義から基本設計・詳細設計・検証計画までの作成順と各文書の完成条件を整理した。
- [必要な画像一覧](design/image-reference-list.md)に、モデル制作の前に用意する画風比較・キャラクター設定画・背景・小道具・HUD画像と作成順をまとめた。設定画と背景の候補を制作済みで、守り手の新C-05は最終採用前。
- 現在の企画案は **『こぎつねと冬の灯り』**。子狐が凍える弟妹のために神社の灯籠を盗み、狛犬が足跡・鈴・灯りを手掛かりに追う。[世界観の異なる5案](research/animal-heist-themes.md)から選んだ方向。
- 「こぎつねと冬の灯り」を試す場合の[Jevを使う狛犬の設計](research/fox-jev-design.md)を追加した。
- 同作の[ストーリー設計書](research/fox-story-design.md)に、世界のルール、登場人物、場面進行、結末、Jevとの境界をまとめた。
- [画面・UI・実装方式の検討](research/fox-visual-implementation.md)では、右下に常設する四つの行動ボタンと、Blenderで制作した画をC++ + raylibの2.5Dゲームで使う方式を第一候補にした。
- [制作方式の追加調査](research/production/README.md)に、3D→2.5Dの素材工程、子狐の造形、Jev接続、検証ゲート、スキル・エージェント候補、3D経験済みの制作者が挑戦する新技術を整理した。
- 狛犬のJev判断は新しい手掛かりや盗難など、意味のある状況変化時に呼ぶ（D-17）。固定した性格の行動傾向と、実際に得た観測・記憶から合法な意図を選び、確率抽選はしない。移動・視界・経路探索はゲームコードで処理し、応答が遅い時もゲームは進める。連続観測のまとめ方は試作で検証する。
- 捕獲後にCP-B／Cからやり直す時は、狛犬の観測・記憶もCP保存時点へ戻す（D-18）。同じ挑戦中の記憶は残し、失敗した挑戦の後半で得た記憶は持ち越さない。
- Jev の価値は「決まった候補の中から、状況の意味を踏まえて素早く選ぶこと」。台詞やマップの新規生成には別の仕組みが必要。
- 以前の人間主人公案は[仮面夜会の怪盗](research/recommended-theme.md)と[記憶を盗む夜](research/world-and-visual-direction.md)に残し、現在の選定対象からは外す。

## フォルダ構成

```text
.
├── README.md                       # 結論と次の一歩
├── design/
│   ├── document-index.md           # 作る設計書、作成順、完成条件
│   ├── game-design.md              # 遊びの核、ルール、Jevの見せ場
│   ├── story-and-levels.md         # 6場面の導線・障害・構図・演出・結末条件
│   ├── art-and-audio.md            # 色・光・造形・演技・音の基準と評価方法
│   ├── basic-design.md             # 画面遷移、状態、Jev・描画の基本構造
│   ├── ui-and-controls.md          # 画面・操作・HUD・予兆の仕様
│   ├── komainu-jev.md              # 狛犬の観測・記憶・Jev判断契約
│   ├── technical-and-assets.md     # 座標・描画・素材・ビルド
│   ├── verification.md             # 要件別の検証・受け入れゲート
│   ├── release-and-operations.md   # 公開物・接続先・権利・障害対応
│   ├── decision-log.md             # 決定済み・採用待ちの判断記録
│   ├── image-reference-list.md      # 後で作る画像の種類・用途・順番
│   └── requirements.md             # 要件定義の草案と未決定事項
├── research/
│   ├── jev-basics.md               # 仕組み、質問型、確率、強みと弱み、制約、料金
│   ├── game-ideas.md               # NPC案と別案の比較
│   ├── animal-heist-themes.md      # 動物が盗むゲームの世界観5案
│   ├── fox-jev-design.md           # 狛犬NPCへのJevの適用と最小試作
│   ├── fox-story-design.md         # 子狐の物語、場面、結末、美術・音
│   ├── fox-visual-implementation.md # 2.5D制作方式と常設行動UI
│   ├── recommended-theme.md        # 第一候補のゲームテーマと縦切り試作
│   ├── world-and-visual-direction.md # 世界観、美術、技術候補
│   ├── app-ideas-catalog.md        # Jevの特徴を活かすアプリ案30選
│   ├── npc-prototype.md            # 毎秒判断型NPCの設計と検証計画
│   ├── remaining-validation.md     # 資料で確認済みの条件と実測が必要な項目
│   ├── sources.md                  # 一次資料と実装例
│   └── production/                # 制作方式を確定するための追加調査
│       ├── README.md              # 結論と実装時の全体構成
│       ├── rendering-pipeline.md  # Blender→raylibの素材・座標・描画
│       ├── character-art.md       # 子狐と狛犬の造形・アニメーション
│       ├── jev-integration.md     # 狛犬AIの状態と通信契約
│       ├── new-tech.md            # 今回新しく挑戦する技術
│       ├── agents-and-skills.md   # 配布されているスキル・エージェント候補
│       └── validation.md          # 実証シーンの合否・未確認条件
└── prototypes/
    └── README.md                   # 小さな実験の置き場
```

実装時の`game/`、`server/`、`art/`、`.agents/skills/`、`.codex/agents/`の全体構成は[制作方式の追加調査](research/production/README.md)に示した。C++ + raylibは技術の第一候補であり、要件と映像検証後に決める。空の実装用フォルダや外部スキルはまだ作らない。

## 次の一歩

1. [設計ドキュメント一覧](design/document-index.md)の草案と[判断記録](design/decision-log.md)をレビューし、採用待ちの案と未決定事項を整理する。制作人数・期限・スケジュールはユーザーが管理する。
2. 実APIの固定盤面と境内1画面でJev使用量、運ばれる灯籠の光、再挑戦の負担を測る。費用の上限は実測後に必要なら検討する。
3. ユーザーの実装指示を受けた範囲で、[検証計画](design/verification.md)のゲートから着手する。

資料調査と残る検証は`research/remaining-validation.md`にまとめた。実際のJev API呼び出しはまだ行っていない。APIキー、利用残高、実測費用は未確認。
