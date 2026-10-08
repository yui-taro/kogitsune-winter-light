# C-01 子狐の四方向設定画：採用候補 v1

- 画像: [fox-turnaround-v1.png](fox-turnaround-v1.png)（1672 × 941 px、PNG、背景は不透明）
- 参照: [V-02 ゲーム画面](../v-02/gameplay-v02.png)、[C-02 v3 顔・質感](../c-02/fox-face-texture-v3.png)
- 生成方法: 組み込み imagegen。2枚の参照画像を同時に入力。
- 配置: 左上＝正面、右上＝左側面、左下＝背面、右下＝斜め前45度。

## 4項目の確認結果

1. **比率と足元:** どの方向も幼い頭と短い四足、小さな胴、太い尾で揃い、行内の足元も揃っている。側面の胴は斜め前より長く見えるが、投影方向の差として許容範囲。厳密な寸法は画像から決めず、モデリング時に一つの3D形状へ整理する。
2. **C-02 v3の顔:** 正面・斜め前とも、暗褐色の丸い目、黒い鼻、三角耳、狐の口吻、白い頬毛・胸毛がつながる。正面の目が斜め前よりわずかに大きく見えるため、モデリング時は同一サイズに統一する。
3. **V-02とのつながり:** 暖かい赤茶の背、白い胸と尾先、暗い足先、低い重心と長い尾を保持。V-02に付着した雪と劇的な照明を除き、毛色の確認がしやすい中立光にした。
4. **側面・背面:** 側面は白い胸腹、四本の脚、尾の長さと白い先端を確認できる。背面では尾を横へ流し、尾の付け根と両後脚を見えるようにした。前脚は後脚の内側に一部重なるため、最終的な脚の位置関係は3Dモデルで確認する。

## 採用候補の生成プロンプト

以下は、[元の完成プロンプト](../c-01-prompt.md)の条件を保ち、初稿で尾が背面の脚を隠した問題を解消するために使った最終生成指定。

```text
Use case: stylized-concept
Asset type: C-01 four-view 3D character turnaround for 「こぎつねと冬の灯り」
References: Image 1 is V-02 gameplay: match the juvenile fox's compact full-body outline, warm red-brown upper coat, white underside, dark paws, white tail tip, soft dimensional storybook-film look. Image 2 is C-02 v3: match the same individual's large slightly downturned dark-brown eyes, upright black-tipped triangle ears with pale inner fur, black nose, fox muzzle, soft white cheek ruff and chest. The references' snow and amber lights are not part of the clean design sheet.

Create one horizontal 16:9 IMAGE with a solid, fully opaque, pale blue-gray studio background, subtle floor/contact shadows and soft neutral lighting. Arrange four separate full-body views of ONE IDENTICAL 3D FOX MODEL rotated in place, evenly spaced in a 2×2 grid: top-left direct front, top-right exact left profile facing left, bottom-left direct back, bottom-right front three-quarter 45 degrees. Equal visual scale and matched paw baseline within each row. Every view is the same quiet neutral quadruped standing pose, with the same head, small rounded torso, low center of gravity, short functional legs and long thick white-tipped tail. All ears, every paw, the complete tail and tail root must be inside the frame. No cropped parts. The front and angled face must clearly be the same fox as the C-02 reference, while all body views must be the same compact child proportions as V-02.

Critical back-view requirement: the back-view fox's tail attaches visibly at the rump and naturally curves off to its LEFT, occupying the free space beside the body. The tail must not overlap either hind leg. Show both dark hind paws and the two forelegs as distinct silhouettes, with four paws readable. The tail length, thickness and white-tip ratio must equal the profile and three-quarter views. Keep the side view's torso compact and not adult-length, with four legs distinct. The body is warm red-brown across crown, back, flanks and tail; white across muzzle, below cheeks, chin, chest and belly; dark ear tips and all four paws; crisp white tail tip. Color boundaries must connect logically around the same three-dimensional body. Tail is one continuous fluffy mass and limbs' joint locations remain legible for modeling. Render sculpted soft tuft clusters, not hyperfine photorealistic hair.

No text, labels, number, dimension lines, clothing, collar, props, shrine, snow, other animals, inconsistent identities, duplicate anatomy, doll-like round muzzle, extreme perspective, dramatic shadows, transparent background or alpha channel. The studio backdrop is an intentional solid light blue-gray color over every pixel.
```
