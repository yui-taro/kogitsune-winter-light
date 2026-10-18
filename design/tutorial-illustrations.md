# 操作説明イラスト

プロローグの後に2枚表示する。絵は背景と子狐の見本画像を参照して imagegen の built-in モードで制作し、操作キー・説明文・茂みの印はゲーム内で描画する。画面内の文字を画像に焼き込まない。

## 1. 動かす・触れる

出力: `game/assets/tutorial-move-interact-v1.png`

使用したプロンプト:

> Use case: illustration-story. Asset type: full-screen illustrated background for the FIRST of two tutorial pages inside the existing Japanese winter fox game. Image 1 is the edit target and exact environment/composition reference: snowy fox den at night, winding path at left, two fox cubs in the den at right, pile of twigs in the lower middle. Image 2 is the character design reference for the player fox. Preserve Image 1's overall camera, positions, den, cubs, path, twig pile, colors, lighting, and richly rendered storybook 3D appearance. Add exactly one small player fox (matching Image 2) in the foreground just left of the twig pile, walking toward the twigs. Let the twigs have a subtle warm golden sparkle so they read as the first interactive objective, without making them burn. Keep the upper 18% and lower 24% of the frame visually quiet enough for game-drawn text and controls. No letters, numbers, keyboard art, interface, labels, borders, arrows, watermark, extra characters, or second lantern. The resulting image should be a polished landscape 16:9 tutorial illustration.

参照: `game/assets/nest-cold-empty-v1.png`（編集対象）、`game/assets/fox-idle-v1.png`（子狐の造形）。

## 2. 隠れる・足跡

出力: `game/assets/tutorial-hide-tracks-v1.png`

使用したプロンプト:

> Use case: illustration-story. Asset type: full-screen illustrated background for the SECOND of two tutorial pages inside the same Japanese winter fox game. Image 1 is the edit target and exact environment/composition reference: a snow-covered shrine courtyard at night with torii gate at left, shrine at upper center, low snow-covered shrubs along the left path around x=500-675 and y=475-650 of a 1672x941 canvas, and fallen log passage at right. Image 2 is the player fox design reference, Image 3 the guardian animal design reference. Preserve Image 1's camera, geometry, shrine, torii, log, bushes, snowy path, materials, cool blue palette, and 3D storybook style. Add exactly one small player fox crouching LOW in the left path-side snow-covered shrub near x=600,y=570, partly surrounded by shrub branches but visually readable. Its bushy tail is gently brushing away a few small fox footprints immediately behind it in the snow; show a tiny puff of snow where the tail touches tracks. Add one guardian animal matching Image 3 farther away near the right side of the courtyard, looking away from the fox. The image must visibly teach that the shrub is cover and nearby footprints can be brushed away. Keep the upper 18% and lower 24% quiet enough for game-drawn text and controls. No letters, numbers, keyboard art, interface, labels, arrows, borders, watermark, second fox, or extra animals. Polished landscape 16:9 illustration.

参照: `game/assets/shrine-empty-v1.png`（編集対象）、`game/assets/fox-idle-v1.png`（子狐の造形）、`game/assets/guardian-animal-v1.png`（狛犬の造形）。
