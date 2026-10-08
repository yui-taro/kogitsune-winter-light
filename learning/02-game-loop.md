# 2. 1 フレームを追う

ゲームの中心は [main.cpp](../game/src/main.cpp) の `while (!WindowShouldClose())` と [World.cpp](../game/src/World.cpp) の `Game::Update` です。約 60 分の 1 秒ごとに、入力を集め、世界を進め、結果を描きます。

```text
入力を読む → Input にまとめる → Game::Update(input, dt)
         → JevBridge::Poll → GameAudio::Poll → WorldState を描く
```

`dt` は秒です。`World.cpp` は 0～0.05 秒に制限し、ウィンドウが一時的に止まった後に大きく飛ぶのを抑えます。

## 入力と状態

`main.cpp` は押し続けているキーを `IsKeyDown` で読みます。さらに `GetKeyPressed` の押下キューを読み、短いキー入力も取りこぼさないよう移動方向を短時間保持します。伏せる・触れるなど一回だけ起こす操作も押下キューを使います。画面右下の四ボタンは同じ `Input` にまとめます。キーと画面ボタンは、世界側から見ると区別されません。

```cpp
foxgame::Input input{};
input.move.x = right - left;
input.toggleCrouch = crouchKeyPressed || crouchButtonClicked;
game.Update(input, GetFrameTime());
```

これは説明用に短くしたコードです。実コードには矢印、走る、尾で払う、触れる、再挑戦もあります。

## 子狐の移動

`Game::UpdateFox` は方向を正規化して、斜め移動だけ速くならないようにします。伏せは 105、通常は 185、走りは 310 キャンバス単位/秒です。灯籠を持つと最大 235 になります。

```text
次の位置 = 現在位置 + 正規化した方向 × 速さ × dt
```

`CanMoveTo` は場面ごとの移動可能範囲と障害物を調べます。巣では背景の小道と手前の広場を多角形、森と神社では鳥居・倒木・灯籠へ続く道と雪の広場を線分と幅で表します。足元がその範囲を外れたら進めません。斜めに障害物へ当たったときは X または Y だけの移動も試し、縁に沿って動けるようにしています。倒木の低い通路は伏せた子狐だけが通れます。

低い通路の中で `C` や Shift を押しても、子狐は立った姿勢や走行へ切り替わりません。`UpdateFox` が現在位置を確かめ、操作できない理由を画面の短い文で伝えます。`tests/test_contract.cpp` の `LowPassageRequiresCrouch` で確かめられます。

## 足跡と尾

移動距離を `fox.stepDistance` にため、一定距離ごとに `Footprint` を追加します。走ると足跡の間隔が変わります。`Q` で近くの足跡を `erased` にし、再使用まで `sweepCooldown` を置きます。描画も狛犬の観測も `erased` を無視します。

ここで「データを消す」のではなく「消した印を付ける」理由は、時間や観測済みかどうかを含む足跡の履歴をテストしやすいためです。

## 描画座標

世界と画像の基準は 1672×941 です。`main.cpp` はこの大きさの `RenderTexture` に描いてから、ウィンドウに収まるよう等倍比率で拡大縮小します。`ToCanvas` はマウス座標をこの基準へ戻します。ウィンドウを変形しても、当たり判定とボタンの位置が一致するためです。

描画は、背景 → 灯りの光 → 足跡・影 → キャラクターと灯籠 → 雪 → 文章とボタン、という順序が基本です。後に描いたものが手前に見えます。

## 小さな実験

`tests/test_contract.cpp` の `MovementModesAndFrameClamp` と `MovementTracksAndSweep` を読んでから、次を実行します。

```sh
make -C game test
```

「1 秒の更新を渡しても子狐が一気に遠くへ移動しない」「尾を連打しても毎フレーム消せない」が、どのコードで保証されるかを探してください。
