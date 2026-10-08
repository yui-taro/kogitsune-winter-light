# Jevの特徴を主役にするアプリ案 30選

調査日: 2026-10-02。ユーザーのゲーム制作目的に合わせ、**遊ぶゲーム24案**、**ゲームを作る・運営するアプリ6案**を挙げる。これは企画案であり、各案の面白さ・採算・Jevの判断品質は未検証。

## 評価の物差し

Jevが得意とするのは「短い状態を読み、あらかじめ列挙した候補から選び、確率を返す」判断。複数の独立した質問は1回にまとめられる。画像認識、台詞生成、正確な計算、長い先読みは別処理にする。[公式Primitives](https://docs.typesafe.ai/primitives)・[Fan-out](https://docs.typesafe.ai/patterns/fan-out)・[既知の弱点](https://docs.typesafe.ai/model-jaggedness/jev-1.13)

各案は次の4点で選んだ。(1) プレイヤーにとって判断の変化が遊びになる、(2) 候補とゲームルールを明確にできる、(3) 意味を含む短い状態を渡せる、(4) 回線遅延時もローカル方針で続けられる。**出典はJevの機能や近い実装の根拠であり、新しい企画の成功を証明するものではない。**

規模は2Dブラウザで作る最小試作の相対的な目安（小・中・大）。優先順位はこのプロジェクトでの企画判断。

## A. 判断を誘導するリアルタイムゲーム

| # | 案・プレイヤーの体験 | Jevが選ぶ候補／Jevらしい仕掛け | 規模・根拠 |
| --- | --- | --- | --- |
| **01** | **音と囮の心理戦ステルス**。足音、偽の通信、消灯で警備網を乱して脱出する。 | 警備員が`巡回/調査/追跡/増援/退避`を選ぶ。画面に行動候補の確率を出し、プレイヤーが判断を操作できる。 | **中**。[公式Choice](https://docs.typesafe.ai/primitives/choice)・[Heist One](https://github.com/AbdelStark/heist-one) |
| **02** | **読み合いボス戦**。同じ攻撃を続けると対処されるので、フェイントと装備変更でボスの次の構えを誘う。 | `守る/突進/距離を取る/罠を置く`の戦術だけJevが選び、攻撃判定と予告モーションはコードで固定する。 | **中**。[公式の弱点](https://docs.typesafe.ai/model-jaggedness/jev-1.13)・[JevDoom実装](https://github.com/TypeSafeAI/typesafe-playground) |
| **03** | **救出任務のAI相棒**。プレイヤーは「自分が先行」「囮になる」などの行動で相棒の勇気と判断を変える。 | 相棒が`援護/救助/隠れる/物資回収`を選ぶ。行動候補の確率を見せ、指示の出し方を学ぶ。 | **中**。[公式ChoiceとScore](https://docs.typesafe.ai/primitives)・[JevBall](https://github.com/atarikcaliskan/jevball) |
| **04** | **指揮官を騙す追跡戦**。敵分隊は個別の敵よりも「誰をどこへ派遣するか」で対抗してくる。 | Jevは分隊長の`包囲/探索/封鎖/撤退`を選び、各兵士の経路はコード。プレイヤーは偽の痕跡で配置を変えさせる。 | **中**。[JevBallの戦術と物理の分担](https://github.com/atarikcaliskan/jevball) |
| **05** | **正体確認射撃場**。標的、味方、囮を見分ける短い判断と、発砲を待つ勇気が勝負。 | Choiceで標的、Noulで撃ってよいか、Scoreで切迫度を同時に尋ねる。誤射判定はコードで厳格に処理。 | **小**。[jev-fps](https://github.com/Waxmell114514/jev-fps)・[公式Fan-out](https://docs.typesafe.ai/patterns/fan-out) |
| **06** | **ロボット奪取パズル**。敵ロボットのセンサーに何を見せるかで、その次の行動を誘導して通路を開く。 | `運搬/充電/警報/障害物回避`から選ぶ。プレイヤーは入力状態を変え、選択確率が変化するのを観察する。 | **小〜中**。[公式Choice](https://docs.typesafe.ai/primitives/choice)・[MicroDuck実装](https://github.com/TypeSafeAI/typesafe-playground) |
| **07** | **反応する怪物の研究所**。同じ怪物を何度も観察し、光・音・餌で行動を予測して逃げる。 | `近づく/隠れる/獲物を変える/威嚇`を選ぶ。確率は研究ノートに蓄積し、推理ゲームになる。 | **中**。[公式確率の説明](https://docs.typesafe.ai/confidence)・[Jev Town](https://github.com/NevaMind-AI/JevTown) |
| **08** | **一瞬のカード速度戦**。場が変わった瞬間にAIが合法手の中から優先順位を決める。 | Jevは`どのカードをどちらの山へ`を選び、合法判定と勝敗はコード。プレイヤーはAIの迷いを利用する。 | **小**。[Jev Practice Speed](https://github.com/tubone24/jev-practice-speed) |

## B. 嘘、信頼、交渉を遊ぶゲーム

| # | 案・プレイヤーの体験 | Jevが選ぶ候補／Jevらしい仕掛け | 規模・根拠 |
| --- | --- | --- | --- |
| **09** | **検問突破**。書類、荷物、発言を組み合わせて検査官の疑念をずらす。 | 検査官が`質問/書類照合/荷物検査/通過/拘束`を選ぶ。セリフは作者が書き、証拠処理はコードで確定する。 | **小〜中**。[Last Exit](https://github.com/0x963d/last-exit) |
| **10** | **嘘つき村の会議**。住民ごとの疑念を読み、手掛かりを出す順番で投票を動かす。 | Jevは容疑者別の疑いと`投票/保留/追及`を出す。確率分布が推理の情報源になる。 | **中**。[Jev Board Game](https://github.com/KeWang0622/jev-board-game)・[公式Choice](https://docs.typesafe.ai/primitives/choice) |
| **11** | **証人を導く探偵劇**。誰に何を見せるかで証人の協力度が変わる。 | `話す/沈黙/嘘をつく/退席`から選択。証言文は用意した断片から再生し、Jevは反応の判断に絞る。 | **中**。[公式Primitives](https://docs.typesafe.ai/primitives)・[Last Exit](https://github.com/0x963d/last-exit) |
| **12** | **戦わずに倒すボス**。ボスの誇り、恐怖、利害に合わせて条件を提示し、戦闘を避ける。 | `拒否/条件付き同意/時間稼ぎ/敵対`を選び、Noulで約束を信じるかを評価する。条件の合法性はコード。 | **中**。[公式Noul](https://docs.typesafe.ai/primitives/noul)・[Last Exit](https://github.com/0x963d/last-exit) |
| **13** | **派閥を揺らす外交盤**。同じ政策でも宗教・職業・過去の被害で支持が割れる。 | 派閥ごとに`支持/中立/反対/取引`を選ぶ。確率を見ながら政策を修正する。多数派の計算はコード。 | **中**。[公式複数質問](https://docs.typesafe.ai/primitives)・[Jev Town](https://github.com/NevaMind-AI/JevTown) |
| **14** | **噂の伝播実験**。ひとつの噂を誰に、どう言い換えて渡すかで町の反応を変える。 | 各住民が`信じる/広める/疑う/通報`を選択。噂の文章生成はプレイヤーか定型文、Jevは反応を担当。 | **中**。[公式Choice・Noul](https://docs.typesafe.ai/primitives)・[Jevtown](https://github.com/gaborishka/jevtown) |
| **15** | **模擬裁判の陪審**。証拠提示順と尋問の選択で各陪審員の心証を動かす。 | 各人の`有罪/無罪/保留`と論点別Scoreを得る。判決ルールと証拠の真偽はコード。 | **中**。[公式Score](https://docs.typesafe.ai/primitives/score)・[公式複数質問](https://docs.typesafe.ai/primitives) |

## C. 多数の判断から世界が動くゲーム

| # | 案・プレイヤーの体験 | Jevが選ぶ候補／Jevらしい仕掛け | 規模・根拠 |
| --- | --- | --- | --- |
| **16** | **観察できる小さな生態系**。天候、餌場、危険を変えて生物の行動連鎖を作る。 | 生物が`採食/休息/逃走/群れる`を選ぶ。確率を生態図鑑に蓄積し、予測の正確さを競う。 | **中**。[Jev Town](https://github.com/NevaMind-AI/JevTown)・[公式Choice](https://docs.typesafe.ai/primitives/choice) |
| **17** | **自治する小さな街**。住民に直接命令せず、法律・仕事・公共設備を変えて生活を誘導する。 | 住民が`働く/休む/訪問/抗議/移住`を選ぶ。時間と資源の計算はコードで行う。 | **大**。[Jev Town](https://github.com/NevaMind-AI/JevTown) |
| **18** | **遭難船の船長**。限られた情報を乗組員へ伝え、各人が救命・修理・反乱のどれを選ぶか見守る。 | 性格と目前の危険に対する短いChoice。プレイヤーは情報の公開順と役割配置を決める。 | **中**。[公式Choice](https://docs.typesafe.ai/primitives/choice)・[Jev Town](https://github.com/NevaMind-AI/JevTown) |
| **19** | **スポーツの作戦板**。自分は監督として大枠を決め、選手は状況ごとに自律判断する。 | `パス/シュート/プレス/下がる`を選び、確率を作戦板で可視化。物理と反則はコード。 | **中〜大**。[JevBall](https://github.com/atarikcaliskan/jevball) |
| **20** | **小隊を率いるミニRTS**。命令は目的だけにし、現場の隊員が危険に応じて手段を選ぶ。 | `進軍/援護/包囲/撤退`のマクロ行動。移動・射程・勝敗はコード。数値計算をJevに任せない。 | **中〜大**。[JevBall](https://github.com/atarikcaliskan/jevball)・[公式の弱点](https://docs.typesafe.ai/model-jaggedness/jev-1.13) |
| **21** | **ローグライク演出監督**。プレイヤーの状態に応じ、次の部屋で何を見せるかが変わる。 | `休息/追跡者/資源/謎/強敵`を選ぶ。部屋自体は合法なパーツからコードで組み立て、難易度の上限もコードで守る。 | **小〜中**。[公式Choice](https://docs.typesafe.ai/primitives/choice)・[公式Score](https://docs.typesafe.ai/primitives/score) |
| **22** | **商人の市場**。客の事情に合わせて品揃えや価格提示を変え、常連との信頼を育てる。 | 客が`購入/値切り/保留/立ち去る`を選ぶ。価格や在庫の算術はコード。 | **中**。[公式Primitives](https://docs.typesafe.ai/primitives)・[公式の数値上の弱点](https://docs.typesafe.ai/model-jaggedness/jev-1.13) |
| **23** | **役割を教えるペット／怪物**。指示とご褒美の積み重ねで、次の場面の行動選択を変える。 | `守る/探索/戻る/遊ぶ`を選ぶ。長期記憶はゲーム側に明示的な記録として持たせる。 | **中**。[公式State](https://docs.typesafe.ai/concepts/state)・[Jev Town](https://github.com/NevaMind-AI/JevTown) |
| **24** | **AIを説得する脱出パズル**。扉を開けるロボットに理由と証拠を示し、拒否の根拠を突き崩す。 | `開ける/追加確認/通報/待機`を選ぶ。ゲーム側に真のルールを置き、曖昧な判断だけをJevに渡す。 | **小**。[公式Choice](https://docs.typesafe.ai/primitives/choice)・[Last Exit](https://github.com/0x963d/last-exit) |

## D. ゲームを作る・運営するアプリ

| # | 案・利用者の仕事 | Jevが担う判断／得られる価値 | 規模・根拠 |
| --- | --- | --- | --- |
| **25** | **NPC行動ラボ**。作者が盤面、性格、候補説明を編集し、何が行動を変えるか比較する。 | Choiceの全確率と`confidence`を可視化し、固定盤面を繰り返し評価。候補文言の変更をA/B比較する。 | **小**。[公式Choice](https://docs.typesafe.ai/primitives/choice)・[公式Confidence](https://docs.typesafe.ai/confidence) |
| **26** | **AI対戦の公平性検査**。不自然に強い／弱いNPC行動を作者がプレイログから探す。 | 戦術の妥当性をScoreで評価し、ルール違反はコードで確定。Jevとローカル方針の差分を集計する。 | **中**。[公式Score](https://docs.typesafe.ai/primitives/score)・[JevBallの比較UI](https://github.com/atarikcaliskan/jevball) |
| **27** | **状況に合うヒント選択器**。プレイヤーの直近の失敗から、作者が用意したヒントのうち最適な1つを出す。 | Choiceでヒント候補を選び、Scoreで困り具合を評価。ヒント文そのものは作者が書く。 | **小**。[公式Choice・Score](https://docs.typesafe.ai/primitives) |
| **28** | **リプレイ分析コーチ**。実況文ではなく、「なぜ失敗したか」を行動ログの観点で分類して見せる。 | `視界/資源/タイミング/判断`のChoiceと危険度Score。事実の時系列・数値はコードで計算する。 | **中**。[公式複数質問](https://docs.typesafe.ai/primitives)・[公式の数値上の弱点](https://docs.typesafe.ai/model-jaggedness/jev-1.13) |
| **29** | **TRPG進行補助**。GMが用意した展開候補から、今の緊張とプレイヤーの行動に合うイベントを選ぶ。 | Jevは`手掛かり/休息/対立/代償`を選び、台詞・描写はGMか生成モデル。 | **小〜中**。[公式Choice](https://docs.typesafe.ai/primitives/choice)・[公式の生成上の弱点](https://docs.typesafe.ai/model-jaggedness/jev-1.13) |
| **30** | **投稿シナリオの整理係**。ユーザー制作イベントをタグ付けし、公開前に作者が見たい項目へ振り分ける。 | Choiceでジャンル、Scoreで複雑さ、Noulで内容上の注意点を評価し、曖昧なものは人へ回す。 | **小〜中**。[公式Primitives](https://docs.typesafe.ai/primitives)・[公式Confidence](https://docs.typesafe.ai/confidence) |

## このプロジェクトでの優先候補

| 順位 | 案 | 選ぶ理由 | 最初の一画面 |
| --- | --- | --- | --- |
| **1** | **01 音と囮の心理戦ステルス** | 元の毎秒NPC案を活かし、プレイヤーがJevの判断を直接誘導できる。 | 1室、警備員2人、囮1種、出口1つ。 |
| **2** | **24 AIを説得する脱出パズル** | 小さく作れ、Jevの言語理解と閉じた選択肢の組み合わせが分かりやすい。 | 扉1つ、ロボット1体、証拠カード3枚。 |
| **3** | **16 観察できる小さな生態系** | 確率の蓄積が図鑑・予測という固有の遊びになる。 | 生物3種、餌2種、天候2種。 |
| **4** | **10 嘘つき村の会議** | 「疑いの確率」というJevの出力がそのままゲーム情報になる。 | 容疑者4人、手掛かり3つ、1回の投票。 |
| **5** | **25 NPC行動ラボ** | ゲーム本体の前に作れば、他の案にも使える比較・評価基盤になる。 | 盤面入力、候補編集、確率表示、ログ保存。 |

## 共通の実装原則

- 見えていない情報はNPCの`state`に入れない。候補は現在合法なものだけ出し、返答後にも合法性を検証する。[Jev Plays Pokémon](https://github.com/christianmat/jev-pokemon/blob/main/README.md)・[JevBall](https://github.com/atarikcaliskan/jevball)
- 確率は観察・推理・誘導のUIに使う。低confidenceを「感情の強さ」と誤読せず、候補の曖昧さも調べる。[公式Confidence](https://docs.typesafe.ai/confidence)
- 毎フレーム呼ばず、局面が変わった時や定めた間隔で呼ぶ。通信中もゲームを進め、古い結果は捨てる。[Jev Practice Speed](https://github.com/tubone24/jev-practice-speed)・[JevDoom実装](https://github.com/TypeSafeAI/typesafe-playground)
- プレイヤーに見せる台詞・物語・マップは作者が用意するか別手段を使う。Jevは短い判断を担当。[公式の既知の弱点](https://docs.typesafe.ai/model-jaggedness/jev-1.13)

既存の近い実装をリンクした案は、その仕組みを借りるための参照。実装やアートを再利用する場合は、各リポジトリのライセンスを別途確認する。
