# 子狐の歩行画像

画像生成スキルの built-in `image_gen` で作成した元画像です。`../assets/fox-*-v1.png` がゲームで読む版で、`../tools/prepare_fox_animation.py` が4コマの余白と足元を揃えます。生成時には `../assets/fox-idle-v1.png` をキャラクターの参照に使いました。

## 生成プロンプト

最初に横向きのシートを作成しました。

> Use case: stylized-concept. Asset type: animation sprite sheet for the existing game character. The supplied fox image is a character identity and art-style reference, not an edit target. Create a single horizontal FOUR-FRAME sprite sheet of the SAME young orange fox, right-facing side profile in all four frames. Frames depict a natural walk cycle: (1) neutral contact, (2) foreleg reaching and body lowered, (3) passing pose with foreleg tucked, (4) opposite contact with hind leg extended. Preserve the fox's fluffy orange coat, white cheeks and chest, white-tipped large tail, oversized expressive brown eyes, dark paws and warm softly rendered 3D-storybook style. Entire full fox with all paws and tail visible in each frame; identical scale, lighting, camera angle, and baseline for each frame. Four equal-width cells, evenly separated by generous transparent empty gutters. GENUINELY TRANSPARENT alpha background everywhere outside the foxes: no black or colored backdrop, no ground, shadows, snow, labels, borders, or text. No overlapping silhouettes across cell boundaries. This is production game art, not a concept sketch.

残る4方向は以下の共通プロンプトの `{angle}` に視点を入れ、元の子狐画像と横向きシートを参照して作成しました。

> Use case: stylized-concept. Asset type: transparent game animation sprite sheet. Image 1 is the original fox character reference; Image 2 is the approved right-side walk sheet and defines color, scale and rendering style. Generate one HORIZONTAL FOUR-FRAME sprite sheet of exactly the SAME young orange fox from this angle: {angle}. Frame 1 neutral contact; frame 2 foreleg reaches; frame 3 passing pose with foreleg tucked; frame 4 opposite contact. Natural quadruped locomotion, believable alternating paws, small body rise/fall, tail swish. Keep same orange fur, white cheeks/chest and white-tipped tail, big brown eyes, dark paws, soft richly textured 3D storybook game art. Identical camera angle, lighting, full-body scale, and paw baseline in all four frames. Four equal-width cells without overlap, each complete fox centered in its cell with generous separation. GENUINE TRANSPARENT ALPHA background, no floor, no shadow, no scenery, no borders, no text, no labels, no black backdrop. Production-ready 4-frame animation strip.

| ファイル | `{angle}` |
| --- | --- |
| `fox-walk-front-right-source.png` | three-quarter FRONT view, fox moving diagonally TOWARD the camera and to screen right; face and both eyes visible, rightward profile emphasis |
| `fox-walk-front-source.png` | straight FRONT view, fox walking toward the viewer; symmetric face with both eyes visible, tail visible behind its side |
| `fox-walk-back-right-source.png` | three-quarter REAR view, fox moving diagonally AWAY from camera and to screen right; show back of head and large white-tipped tail |
| `fox-walk-back-source.png` | straight REAR view, fox walking directly away from the viewer; two ears and fluffy tail visible from behind |

伏せ歩きでは、元の子狐と同じ視点の通常歩行シートを参照し、表の `{angle}` を使って次のプロンプトを指定しました。

> Use case: stylized-concept. Asset type: transparent game sprite sheet for CROUCH-WALK animation. Image 1 is the exact young orange fox character; Image 2 is the approved regular walk strip for the required viewing angle and style. Create a HORIZONTAL FOUR-FRAME sheet of this same fox in a LOW CROUCH in {angle}. Fox's torso remains close to the ground, legs deeply bent, head lowered slightly while eyes stay alert, tail low; do not simply squash or vertically distort a standing fox. Four different natural sneaking steps: contact, forepaw forward, passing, opposite contact. Keep identical fox identity, fluffy orange fur, white face and chest, broad white-tipped tail, brown eyes, dark paws, soft detailed 3D storybook rendering and same blue-night lighting. Full fox visible in every frame, four equal-width nonoverlapping cells with consistent scale and foot baseline. GENUINE TRANSPARENT ALPHA outside characters: no ground, shadow, snow, black backdrop, scenery, text, border, or labels. Production game art.

走りも同じ2枚を参照し、表の `{angle}` を使いました。

> Use case: stylized-concept. Asset type: transparent game sprite sheet for RUN animation. Image 1 is the exact fox character reference; Image 2 is the approved regular walk strip for the required camera angle and style. Create one HORIZONTAL FOUR-FRAME sheet of the SAME young fox in a lively, believable quadruped RUN from {angle}. Four distinct locomotion key poses: (1) hind legs pushing off and body extending, (2) brief airborne leap with forelegs reaching, (3) forepaws landing and torso compressed, (4) hind legs gathering beneath body. Keep all four frames recognizably the same fox with fluffy orange fur, white face and chest, broad white-tipped tail, big brown eyes, dark paws, detailed soft 3D storybook game art, matching warm fox and blue-night rim lighting. No human-like running or two-leg pose. Entire full fox and all paws visible in each frame at consistent scale, camera angle and foot baseline; four equal-width nonoverlapping cells. GENUINE TRANSPARENT ALPHA background. No floor, ground shadow, snow, scenery, black backdrop, text, labels, or borders. Production game art.

| ファイルの `angle` 部分 | 伏せ歩き・走り用 `{angle}` |
| --- | --- |
| `side` | right-facing side profile |
| `front-right` | three-quarter front view, moving diagonally toward the camera and screen right |
| `front` | straight front view, moving toward the viewer |
| `back-right` | three-quarter rear view, moving diagonally away from the camera and screen right |
| `back` | straight rear view, moving away from the viewer |
