#include "World.hpp"
#include "JevBridge.hpp"
#include "Audio.hpp"

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>

#ifndef ASSET_DIR
#define ASSET_DIR "assets"
#endif

using foxgame::Game;
using foxgame::LanternOwner;
using foxgame::Point;
using foxgame::Stage;

namespace {

constexpr int kWidth = 1672;
constexpr int kHeight = 941;
constexpr Color kIvory{248, 239, 220, 255};
constexpr Color kBlue{20, 35, 51, 235};
constexpr Color kGold{244, 187, 98, 255};

struct Sprite {
    Texture2D texture{};
    bool valid = false;
};

std::string Asset(const char* name) {
    std::string bundled = std::string(GetApplicationDirectory()) + "../Resources/assets/" + name;
    if (FileExists(bundled.c_str())) return bundled;
    std::string besideExecutable = std::string(GetApplicationDirectory()) + "assets/" + name;
    if (FileExists(besideExecutable.c_str())) return besideExecutable;
    return std::string(ASSET_DIR) + "/" + name;
}

Sprite LoadSprite(const char* name) {
    Image image = LoadImage(Asset(name).c_str());
    if (!image.data) return {};
    Color* pixels = LoadImageColors(image);
    int left = image.width, top = image.height, right = 0, bottom = 0;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            if (pixels[y * image.width + x].a > 12) {
                left = std::min(left, x); top = std::min(top, y);
                right = std::max(right, x); bottom = std::max(bottom, y);
            }
        }
    }
    UnloadImageColors(pixels);
    if (right > left && bottom > top) ImageCrop(&image, {static_cast<float>(left), static_cast<float>(top),
                                                        static_cast<float>(right - left + 1), static_cast<float>(bottom - top + 1)});
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (texture.id) SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    return {texture, texture.id != 0};
}

Sprite LoadAnimation(const char* name) {
    Texture2D texture = LoadTexture(Asset(name).c_str());
    if (texture.id) SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    return {texture, texture.id != 0};
}

void DrawSprite(const Sprite& sprite, Point foot, float width, float height, bool flip = false, Color tint = WHITE) {
    if (!sprite.valid) return;
    Rectangle source{0, 0, static_cast<float>(sprite.texture.width) * (flip ? -1.0f : 1.0f),
                     static_cast<float>(sprite.texture.height)};
    Rectangle destination{foot.x - width * 0.5f, foot.y - height, width, height};
    DrawTexturePro(sprite.texture, source, destination, {0, 0}, 0.0f, tint);
}

void DrawFoxFrame(const Sprite& sheet, Point foot, int frame, bool flip, bool crouching) {
    if (!sheet.valid) return;
    constexpr int kFrames = 4;
    float frameWidth = static_cast<float>(sheet.texture.width) / kFrames;
    float height = crouching ? 96.0f : 130.0f;
    float width = height * frameWidth / static_cast<float>(sheet.texture.height);
    Rectangle source{frameWidth * frame, 0, frameWidth * (flip ? -1.0f : 1.0f),
                     static_cast<float>(sheet.texture.height)};
    DrawTexturePro(sheet.texture, source, {foot.x - width * 0.5f, foot.y - height, width, height},
                   {0, 0}, 0.0f, WHITE);
}

void Text(Font font, const char* text, float x, float y, float size, Color color) {
    DrawTextEx(font, text, {x, y}, size, 1.0f, color);
}

void Shadow(Point foot, float width, float strength) {
    DrawEllipse(static_cast<int>(foot.x), static_cast<int>(foot.y - 3), width, width * 0.18f,
                Fade(BLACK, strength));
}

void Glow(Point center, float pulse) {
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient({center.x, center.y}, 205.0f + pulse * 14.0f,
                       Fade(kGold, 0.34f), Fade(kGold, 0.0f));
    DrawCircleGradient({center.x, center.y}, 100.0f + pulse * 7.0f,
                       Fade(kGold, 0.40f), Fade(kGold, 0.0f));
    EndBlendMode();
}

Vector2 ToCanvas(Vector2 screen) {
    float scale = std::min(static_cast<float>(GetScreenWidth()) / kWidth,
                           static_cast<float>(GetScreenHeight()) / kHeight);
    return {(screen.x - (GetScreenWidth() - kWidth * scale) * 0.5f) / scale,
            (screen.y - (GetScreenHeight() - kHeight * scale) * 0.5f) / scale};
}

bool Button(Font font, Rectangle bounds, const char* label, const char* key, bool active, bool enabled, Vector2 mouse) {
    bool hover = CheckCollisionPointRec(mouse, bounds);
    Color panel = active ? Color{107, 75, 40, 237} : hover ? Color{39, 64, 82, 242} : kBlue;
    DrawRectangleRounded(bounds, 0.25f, 8, panel);
    DrawRectangleRoundedLinesEx(bounds, 0.25f, 8, 2.0f,
                                active ? kGold : enabled ? Fade(kIvory, 0.7f) : Fade(kIvory, 0.25f));
    Text(font, key, bounds.x + 13, bounds.y + 9, 21, kGold);
    float labelSize = 26.0f;
    while (labelSize > 15.0f && MeasureTextEx(font, label, labelSize, 1.0f).x > bounds.width - 20.0f)
        labelSize -= 1.0f;
    Text(font, label, bounds.x + 12, bounds.y + 43, labelSize,
         enabled ? kIvory : Fade(kIvory, 0.48f));
    return hover && enabled && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

const char* Objective(const foxgame::WorldState& world) {
    if (world.stage == Stage::Captured) return "見つかってしまった";
    if (world.stage == Stage::SliceComplete) return "神社を抜けた";
    if (world.stage == Stage::Complete) return "冬の夜に、ふたつの灯りが残った";
    if (world.stage == Stage::NestEnding) return "灯りを分ける";
    if (world.stage == Stage::NestIntro) return world.nestTwigsTouched ?
        "神社の灯りを探しに行こう" : "巣の前の小枝に触れよう";
    if (world.stage == Stage::ForestOutbound) return "雪の小道を進み、鳥居か倒木へ";
    if (world.stage == Stage::ForestReturn) return world.lantern.owner == LanternOwner::Fox ?
        "灯籠を巣へ持ち帰ろう" : "灯籠を拾って巣へ戻ろう";
    if (world.lantern.owner == LanternOwner::Fox) return "灯籠を持って、鳥居か倒木から抜けよう";
    if (world.lantern.owner == LanternOwner::Ground) return "灯籠をもう一度くわえよう";
    return "社の前の灯籠へ近づこう";
}

const char* Interaction(const foxgame::WorldState& world) {
    if (world.stage == Stage::NestIntro) return "小枝を寄せる";
    if (world.stage == Stage::ForestOutbound) return "触れる";
    if (world.lantern.owner == LanternOwner::Fox) return "灯籠を置く";
    if (foxgame::Distance(world.fox.position, world.lantern.position) < 95.0f) return "灯籠をくわえる";
    if (foxgame::Distance(world.fox.position, {350.0f, 500.0f}) < 100.0f) return "鈴を鳴らす";
    return "触れる";
}

void Snow(float time) {
    for (int i = 0; i < 42; ++i) {
        float seed = static_cast<float>((i * 137 + 59) % 991) / 991.0f;
        float x = std::fmod(seed * kWidth + time * (13.0f + i % 7), static_cast<float>(kWidth));
        float y = std::fmod(static_cast<float>((i * 271 + 17) % 941) + time * (20.0f + i % 11), static_cast<float>(kHeight));
        DrawCircleV({x, y}, i % 5 == 0 ? 2.3f : 1.25f, Fade(kIvory, 0.34f));
    }
}

} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1360, 765, "こぎつねと冬の灯り");
    SetWindowMinSize(960, 540);
    SetWindowFocused();
    SetExitKey(KEY_NULL);  // Escape opens the pause overlay instead of closing the window.
    SetTargetFPS(60);

    Texture2D shrineBackground = LoadTexture(Asset("shrine-empty-v1.png").c_str());
    Texture2D nestColdBackground = LoadTexture(Asset("nest-cold-empty-v1.png").c_str());
    Texture2D forestOutboundBackground = LoadTexture(Asset("forest-outbound-empty-v1.png").c_str());
    Texture2D forestReturnBackground = LoadTexture(Asset("forest-return-empty-v1.png").c_str());
    Texture2D nestWarmBackground = LoadTexture(Asset("nest-warm-ending-v1.png").c_str());
    std::array<Sprite, 5> foxWalk{{
        LoadAnimation("fox-walk-side-v1.png"),
        LoadAnimation("fox-walk-front-right-v1.png"),
        LoadAnimation("fox-walk-front-v1.png"),
        LoadAnimation("fox-walk-back-right-v1.png"),
        LoadAnimation("fox-walk-back-v1.png"),
    }};
    std::array<Sprite, 5> foxRun{{
        LoadAnimation("fox-run-side-v1.png"),
        LoadAnimation("fox-run-front-right-v1.png"),
        LoadAnimation("fox-run-front-v1.png"),
        LoadAnimation("fox-run-back-right-v1.png"),
        LoadAnimation("fox-run-back-v1.png"),
    }};
    std::array<Sprite, 5> foxCrouch{{
        LoadAnimation("fox-crouch-side-v1.png"),
        LoadAnimation("fox-crouch-front-right-v1.png"),
        LoadAnimation("fox-crouch-front-v1.png"),
        LoadAnimation("fox-crouch-back-right-v1.png"),
        LoadAnimation("fox-crouch-back-v1.png"),
    }};
    Sprite foxFallback = LoadSprite("fox-idle-v1.png");
    Sprite stone = LoadSprite("guardian-stone-v1.png");
    Sprite animal = LoadSprite("guardian-animal-v1.png");
    Sprite lantern = LoadSprite("lantern-v1.png");
    RenderTexture2D canvas = LoadRenderTexture(kWidth, kHeight);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);

    const std::string glyphs =
        "こぎつねと冬の灯り神社の夜社の前の灯籠へ近づこう灯籠を持って鳥居か倒木から抜けよう"
        "灯籠をもう一度くわえよう見つかってしまった神社を抜けた伏せる走る尾で払う"
        "触れる灯籠を置く灯籠をくわえる鈴を鳴らす足跡を払った近くに払える足跡がない"
        "風が鈴を鳴らした狛犬の目がこちらを向いた境内の手前を記憶した"
        "狛犬に見つかったでチェックポイントからやり直す境内の手前から再開鳥居の手前から再開"
        "灯籠を雪の上に置いた灯籠をくわえたふたつの出口から森へ戻ろう"
        "近くに触れられるものがない神社を抜けた灯籠を巣へ運ぶ旅が続く"
        "弟妹のそばの小枝に触れてみよう小枝に火はつかない山の神社の灯りを持って帰ろう"
        "まず巣の前の小枝に近づこう鳥居の先に灯籠が見える雪には足跡が残る"
        "神社の灯りを目指そう風の鈴と狛犬の動きを見て灯籠へ進もう"
        "灯籠の光を抱えて森を通り巣へ戻ろう灯籠を拾って巣へ戻ろう"
        "弟妹のいる巣に灯籠が届いた小枝に火が灯り灯籠は神社へ戻った"
        "冬の夜にふたつの灯りが残った灯りを分ける小枝を寄せる"
        "神社の灯りを探しに行こう巣の前の小枝に触れよう雪の小道を進み鳥居か倒木へ"
        "灯籠を巣へ持ち帰ろう巣に灯りが届いた弟妹が顔を上げる"
        "狛犬は守るべきものを見つめた小さな火を小枝に分け灯籠を神社へ"
        "はじめからもう一度一時停止"
        "ゲーム画面をクリックして操作クリック後に移動文字入力中なら画面を選んでください"
        "倒木の下では伏せたまま進もう走れない"
        "ローカル試作Jev中継接続待ち代替判断"
        "移動保存操作姿を隠して進もう右の倒木は伏せて通れる再挑戦続きは次の場面へ"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!?:・、。←→"
        "  -./_·ぽ印矢";
    int codepointCount = 0;
    int* codepoints = LoadCodepoints(glyphs.c_str(), &codepointCount);
    Font font = LoadFontEx(Asset("NotoSansCJKjp-Regular.otf").c_str(), 44, codepoints, codepointCount);
    UnloadCodepoints(codepoints);
    if (font.texture.id) SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    Game game;
    foxgame::GameAudio audio;
    const char* relayUrl = std::getenv("JEV_RELAY_URL");
    foxgame::JevBridge bridge(relayUrl ? relayUrl : "");
    game.SetExternalDecisionMode(bridge.Configured());
    bool paused = false;
    std::array<float, 4> movementPulse{};
    while (!WindowShouldClose()) {
        bool focused = IsWindowFocused();
        // raylib keeps key presses in a queue even when press and release happen
        // between two frames. IsKeyDown() alone misses those short taps.
        std::array<bool, 512> pressed{};
        for (int key = GetKeyPressed(); key != 0; key = GetKeyPressed()) {
            if (key > 0 && key < static_cast<int>(pressed.size())) pressed[key] = true;
        }
        auto justPressed = [&](int key) { return IsKeyPressed(key) || pressed[key]; };
        if (justPressed(KEY_ESCAPE)) paused = !paused;
        const auto& world = game.State();
        Vector2 mouse = ToCanvas(GetMousePosition());
        float frameTime = GetFrameTime();
        constexpr float tapDuration = 0.18f;
        if (justPressed(KEY_A) || justPressed(KEY_LEFT)) movementPulse[0] = tapDuration;
        if (justPressed(KEY_D) || justPressed(KEY_RIGHT)) movementPulse[1] = tapDuration;
        if (justPressed(KEY_W) || justPressed(KEY_UP)) movementPulse[2] = tapDuration;
        if (justPressed(KEY_S) || justPressed(KEY_DOWN)) movementPulse[3] = tapDuration;
        foxgame::Input input{};
        input.move.x = static_cast<float>(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT) || movementPulse[1] > 0.0f) -
                       static_cast<float>(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT) || movementPulse[0] > 0.0f);
        input.move.y = static_cast<float>(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) || movementPulse[3] > 0.0f) -
                       static_cast<float>(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) || movementPulse[2] > 0.0f);
        input.toggleCrouch = justPressed(KEY_LEFT_CONTROL) || justPressed(KEY_C);
        input.toggleRun = justPressed(KEY_LEFT_SHIFT);
        input.sweep = justPressed(KEY_Q);
        input.interact = justPressed(KEY_E);
        input.restart = justPressed(KEY_R);

        // The button row stays in the same place in every game state.
        std::array<Rectangle, 4> buttons{{{1080, 817, 132, 94}, {1220, 817, 132, 94},
                                           {1360, 817, 132, 94}, {1500, 817, 132, 94}}};
        bool playable = world.stage != Stage::Captured && world.stage != Stage::SliceComplete &&
                        world.stage != Stage::NestEnding && world.stage != Stage::Complete && !paused && focused;
        bool clicked[4]{};
        if (playable && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            for (int i = 0; i < 4; ++i) clicked[i] = CheckCollisionPointRec(mouse, buttons[i]);
        }
        input.toggleCrouch |= clicked[0];
        input.toggleRun |= clicked[1];
        input.sweep |= clicked[2];
        input.interact |= clicked[3];
        if (!paused && focused) {
            game.Update(input, frameTime);
            bridge.Poll(game);
            audio.Poll(game.Sounds());
        }
        for (float& pulse : movementPulse) pulse = paused || !focused ? 0.0f :
            std::max(0.0f, pulse - std::clamp(frameTime, 0.0f, 0.05f));

        const auto& state = game.State();
        float time = static_cast<float>(GetTime());
        Stage scene = state.stage == Stage::Captured ? state.capturedScene : state.stage;
        bool shrineScene = scene == Stage::Approach || scene == Stage::Courtyard ||
                           scene == Stage::Escape || scene == Stage::SliceComplete;
        bool returnScene = scene == Stage::ForestReturn;
        bool endingScene = scene == Stage::NestEnding || scene == Stage::Complete;
        Point light = state.lantern.owner == LanternOwner::Fox ?
            Point{state.fox.position.x + 44.0f * state.fox.facingX, state.fox.position.y - 76.0f} :
            state.lantern.position;

        BeginTextureMode(canvas);
        ClearBackground({12, 26, 40, 255});
        Texture2D* plate = &shrineBackground;
        if (scene == Stage::NestIntro || endingScene) plate = &nestColdBackground;
        else if (scene == Stage::ForestOutbound) plate = &forestOutboundBackground;
        else if (returnScene) plate = &forestReturnBackground;
        if (plate->id) DrawTexturePro(*plate, {0, 0, static_cast<float>(plate->width), static_cast<float>(plate->height)},
                                      {0, 0, static_cast<float>(kWidth), static_cast<float>(kHeight)}, {0, 0}, 0, WHITE);
        bool liveLantern = shrineScene || returnScene || (endingScene && scene == Stage::NestEnding && state.stageTime < 7.0f);
        if (liveLantern) Glow({light.x, light.y - (state.lantern.owner == LanternOwner::Fox ? 10.0f : 58.0f)},
                              std::sin(time * 2.3f));

        for (const auto& step : state.footprints) {
            if (step.erased) continue;
            float age = state.time - step.createdAt;
            DrawEllipse(static_cast<int>(step.position.x - 8), static_cast<int>(step.position.y), 6, 3,
                        Fade({49, 63, 82, 255}, std::max(0.0f, 0.52f - age * 0.013f)));
            DrawEllipse(static_cast<int>(step.position.x + 8), static_cast<int>(step.position.y + 5), 6, 3,
                        Fade({49, 63, 82, 255}, std::max(0.0f, 0.52f - age * 0.013f)));
        }
        if (shrineScene && state.lantern.owner != LanternOwner::Pedestal)
            DrawEllipse(780, 449, 37, 12, Fade(BLACK, 0.40f));

        Point guardianFoot = state.guardian.position;
        Point foxFoot = state.fox.position;
        auto drawGuardian = [&]() {
            bool isStone = state.guardian.form == foxgame::GuardianForm::Stone ||
                           state.guardian.form == foxgame::GuardianForm::Awakening;
            Shadow(guardianFoot, isStone ? 70.0f : 87.0f, 0.22f);
            DrawSprite(isStone ? stone : animal, guardianFoot, isStone ? 188.0f : 215.0f,
                       isStone ? 181.0f : 148.0f, state.guardian.facing.x < 0.0f);
            if (state.knowledge.foxVisible && state.stage != Stage::Captured) {
                DrawCircleLines(static_cast<int>(guardianFoot.x), static_cast<int>(guardianFoot.y - 180), 16, kGold);
                Text(font, "!", guardianFoot.x - 5, guardianFoot.y - 195, 30, kGold);
            }
        };
        auto drawFox = [&]() {
            float x = state.fox.facingX;
            float y = state.fox.facingY;
            int view = std::abs(y) < 0.25f ? 0 :
                       std::abs(x) < 0.25f ? (y > 0.0f ? 2 : 4) :
                       y > 0.0f ? 1 : 3;
            bool flip = x < -0.25f;
            float stride = state.fox.crouching ? 12.0f : state.fox.running ? 20.0f : 16.0f;
            int frame = state.fox.moving ? static_cast<int>(state.fox.walkDistance / stride) % 4 : 0;
            float bob = state.fox.moving ? std::sin(state.fox.walkDistance / stride * 1.5708f) *
                                             (state.fox.running ? 2.0f : 1.2f) :
                                             std::sin(time * 2.8f) * 0.65f;
            const auto& motion = state.fox.crouching ? foxCrouch :
                                 state.fox.running ? foxRun : foxWalk;
            const Sprite& sheet = motion[view].valid ? motion[view] : foxWalk[view];
            float width = sheet.valid ? (state.fox.crouching ? 96.0f : 130.0f) *
                                          (static_cast<float>(sheet.texture.width) / 4.0f) /
                                          static_cast<float>(sheet.texture.height) : 193.0f;
            Shadow(foxFoot, std::max(43.0f, width * 0.36f), 0.18f);
            if (sheet.valid) DrawFoxFrame(sheet, {foxFoot.x, foxFoot.y + bob}, frame, flip,
                                          state.fox.crouching);
            else DrawSprite(foxFallback, {foxFoot.x, foxFoot.y + bob}, 193.0f,
                            state.fox.crouching ? 96.0f : 130.0f, flip);
        };
        bool showFox = !endingScene || (scene == Stage::NestEnding && state.stageTime < 7.0f);
        bool showGuardian = shrineScene || (returnScene && state.guardianEnteredForest) ||
                            (scene == Stage::NestEnding && state.stageTime > 2.0f && state.stageTime < 7.0f);
        if (showFox && showGuardian) {
            if (guardianFoot.y < foxFoot.y) { drawGuardian(); drawFox(); }
            else { drawFox(); drawGuardian(); }
        } else {
            if (showFox) drawFox();
            if (showGuardian) drawGuardian();
        }
        if (liveLantern && (state.lantern.owner == LanternOwner::Pedestal ||
            state.lantern.owner == LanternOwner::Ground || state.lantern.owner == LanternOwner::Fox)) {
            Point lanternFoot = state.lantern.owner == LanternOwner::Fox ?
                Point{foxFoot.x + 49.0f * state.fox.facingX, foxFoot.y - 31.0f} : state.lantern.position;
            if (endingScene) lanternFoot = {820.0f, 620.0f};
            if (state.lantern.owner != LanternOwner::Fox) Shadow(lanternFoot, 23.0f, 0.18f);
            DrawSprite(lantern, lanternFoot, 64.0f, 86.0f);
        }
        if (endingScene && nestWarmBackground.id) {
            float warmAlpha = scene == Stage::Complete ? 1.0f :
                std::clamp((state.stageTime - 6.0f) / 3.0f, 0.0f, 1.0f);
            DrawTexturePro(nestWarmBackground,
                           {0, 0, static_cast<float>(nestWarmBackground.width), static_cast<float>(nestWarmBackground.height)},
                           {0, 0, static_cast<float>(kWidth), static_cast<float>(kHeight)}, {0, 0}, 0,
                           Fade(WHITE, warmAlpha));
        }
        if (!endingScene || state.stageTime < 9.0f) Snow(time);

        DrawRectangleRounded({31, 24, 605, 104}, 0.18f, 8, Fade(kBlue, 0.87f));
        Text(font, "こぎつねと冬の灯り", 52, 31, 34, kIvory);
        Text(font, Objective(state), 53, 78, 25, kGold);
        const char* mode = !bridge.Configured() ? "ローカル試作" :
                           bridge.Degraded() ? "接続待ち・代替判断" : "Jev中継";
        Vector2 modeWidth = MeasureTextEx(font, mode, 22, 1);
        Text(font, mode, kWidth - modeWidth.x - 35, 37, 22, Fade(kIvory, 0.87f));
        if (state.stage != Stage::Captured && state.stage != Stage::SliceComplete && !endingScene) {
            DrawRectangleRounded({31, 851, 680, 59}, 0.26f, 8, Fade(kBlue, 0.80f));
            float noticeSize = MeasureTextEx(font, game.Notice().c_str(), 24, 1).x > 640 ? 21.0f : 24.0f;
            Text(font, game.Notice().c_str(), 49, 863, noticeSize, kIvory);
        }
        Button(font, buttons[0], "伏せる", "C", state.fox.crouching, playable, mouse);
        Button(font, buttons[1], "走る", "Shift", state.fox.running, playable, mouse);
        Button(font, buttons[2], "尾で払う", "Q", false, playable, mouse);
        Button(font, buttons[3], Interaction(state), "E", false, playable, mouse);

        if (state.stage == Stage::NestEnding) {
            const char* story = state.stageTime < 3.0f ? "巣に灯りが届いた。弟妹が顔を上げる" :
                                state.stageTime < 7.0f ? "狛犬は、守るべきものを見つめた" :
                                state.stageTime < 11.0f ? "小さな火を小枝に分け、灯籠を神社へ" :
                                                        "冬の夜に、ふたつの灯りが残った";
            DrawRectangleRounded({270, 730, 1140, 78}, 0.20f, 8, Fade(kBlue, 0.83f));
            Vector2 size = MeasureTextEx(font, story, 34, 1);
            Text(font, story, (kWidth - size.x) * 0.5f, 750, 34, kIvory);
        }

        if (!focused || paused || state.stage == Stage::Captured || state.stage == Stage::SliceComplete ||
            state.stage == Stage::Complete) {
            DrawRectangle(0, 0, kWidth, kHeight, Fade({3, 11, 18, 255}, 0.65f));
            DrawRectangleRounded({389, 266, 894, 398}, 0.06f, 8, Fade(kBlue, 0.98f));
            Text(font, !focused ? "ゲーム画面をクリックして操作" :
                       state.stage == Stage::Captured ? "見つかってしまった" :
                       state.stage == Stage::SliceComplete ? "神社を抜けた" :
                       state.stage == Stage::Complete ? "こぎつねと冬の灯り" : "一時停止",
                       491, 326, 53, kIvory);
            Text(font, !focused ? "クリック後に WASD / 矢印で移動" :
                       state.stage == Stage::Captured ? "R でチェックポイントから再挑戦" :
                       state.stage == Stage::SliceComplete ? "灯籠を巣へ運ぶ旅が続く" :
                       state.stage == Stage::Complete ? "R ではじめからもう一度" : "Esc で再開",
                       493, 423, 31, kGold);
            Text(font, "移動 WASD / 矢印  ·  伏せる C  ·  走る Shift", 493, 515, 24, kIvory);
            Text(font, "足跡を払う Q  ·  触れる E", 493, 562, 24, kIvory);
        }
        EndTextureMode();

        BeginDrawing();
        ClearBackground({6, 15, 24, 255});
        float scale = std::min(static_cast<float>(GetScreenWidth()) / kWidth,
                               static_cast<float>(GetScreenHeight()) / kHeight);
        Rectangle destination{(GetScreenWidth() - kWidth * scale) * 0.5f,
                              (GetScreenHeight() - kHeight * scale) * 0.5f,
                              kWidth * scale, kHeight * scale};
        DrawTexturePro(canvas.texture, {0, 0, static_cast<float>(kWidth), -static_cast<float>(kHeight)},
                       destination, {0, 0}, 0, WHITE);
        EndDrawing();
    }

    if (font.texture.id) UnloadFont(font);
    if (shrineBackground.id) UnloadTexture(shrineBackground);
    if (nestColdBackground.id) UnloadTexture(nestColdBackground);
    if (forestOutboundBackground.id) UnloadTexture(forestOutboundBackground);
    if (forestReturnBackground.id) UnloadTexture(forestReturnBackground);
    if (nestWarmBackground.id) UnloadTexture(nestWarmBackground);
    for (const Sprite& sheet : foxWalk) if (sheet.valid) UnloadTexture(sheet.texture);
    for (const Sprite& sheet : foxRun) if (sheet.valid) UnloadTexture(sheet.texture);
    for (const Sprite& sheet : foxCrouch) if (sheet.valid) UnloadTexture(sheet.texture);
    if (foxFallback.valid) UnloadTexture(foxFallback.texture);
    if (stone.valid) UnloadTexture(stone.texture);
    if (animal.valid) UnloadTexture(animal.texture);
    if (lantern.valid) UnloadTexture(lantern.texture);
    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
