# 5. 絵・音・ビルド

現在の画面は固定背景と透過画像を重ねる 2.5D 試作です。背景は巣・森・神社・結末を別画像にし、子狐、狛犬、灯籠は透過スプライトにしています。素材は [game/assets](../game/assets) にあります。

## 画像を描く流れ

1. `main.cpp` が場面に対応する背景を選び、1672×941 の `RenderTexture` へ描きます。
2. 子狐の `fox.position` や灯籠の位置に合わせて、影、光、スプライトを描きます。`DrawSprite` の座標は足元基準なので、絵の高さが変わっても地面の位置を合わせやすくしています。
3. 足跡、雪、目的文、右下の四ボタンを重ねます。
4. 最後にキャンバスをウィンドウへ縦横比を保って表示します。

`LoadSprite` は透過画像の余白を調べ、見える部分へ切り詰めてからテクスチャにします。灯籠の光は `Glow` で加算合成し、時間で少し揺らします。文章には `NotoSansCJKjp-Regular.otf` を使います。

## 子狐の歩行アニメーション

子狐には5種類の視点（横、斜め前、正面、斜め後ろ、背面）を用意し、それぞれに通常歩行・走り・伏せ歩きの4コマがあります。横と斜めの絵は左右反転できるので、画面上では8方向を表せます。元画像は [art-source](../game/art-source)、足元と余白を揃えたゲーム用画像は [assets](../game/assets) に保存し、[prepare_fox_animation.py](../game/tools/prepare_fox_animation.py) で加工しました。

`World.cpp` の `UpdateFox` は移動前後の位置の差から、実際に進んだ向きと距離を `fox.facingX`、`fox.facingY`、`fox.walkDistance` に記録します。障害物に押しつけて位置が変わらなければ `fox.moving` は偽になります。`main.cpp` は向きに合う画像を選び、距離が一定増えるたびに次のコマを描きます。走ると足の切替を速く、伏せると遅くします。

```text
入力 → 移動判定 → 実際の移動方向と距離を保存
                  ↓
           視点を選ぶ → 4コマの番号を選ぶ → 足元をそろえて描く
```

時間だけでコマを進めると、道の端で進めない子狐の足が空回りします。距離を使えば、位置が止まった時にアニメーションも止まります。静止中は各姿勢の1コマ目を表示し、体をわずかに上下させます。

## 音を鳴らす流れ

世界側は「鈴が鳴った」を `SoundEvents.bell` の回数で表します。`GameAudio::Poll` は前回の回数と比べ、新たな分だけ音を鳴らします。こうするとゲームの規則は「音源をどう再生するか」を知る必要がありません。音波形は [Audio.cpp](../game/src/Audio.cpp) が実行時に作ります。

```text
World.cpp: ++soundEvents_.bell
main.cpp:  audio.Poll(game.Sounds())
Audio.cpp: 回数が増えたら鈴の音を再生
```

## ビルドの二つの入口

macOS では [Makefile](../game/Makefile) が、ローカルの raylib 6.0 と libcurl をリンクします。`make -C game bundle` はアプリの `Contents/Resources/assets` に素材とライセンスをコピーします。

Windows 向けには [CMakeLists.txt](../game/CMakeLists.txt) が raylib と curl を扱います。CI の設定は [windows-build.yml](../.github/workflows/windows-build.yml) です。Windows 実機での起動、配布パッケージの署名、Jev の公開中継先は未検証・未決定です。

## 画像を変えるときの確認

背景だけを差し替える場合でも、出入り口、障害物、灯籠の絵が `World.cpp` の座標と一致するか確かめます。固定背景に描かれた倒木の位置と当たり判定がずれると、見た目は通れるのに進めない箇所ができます。

ゲームを起動したら、ウィンドウサイズを変え、文字・四ボタン・灯籠の光が崩れないか見てください。画像に問題があるときは、まず [main.cpp](../game/src/main.cpp) の素材名、`Asset`、`LoadSprite` を確認します。
