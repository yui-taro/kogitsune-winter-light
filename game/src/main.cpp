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

struct PrologueBeat {
    const char* firstLine;
    const char* secondLine;
    float seconds;
};

constexpr std::array<PrologueBeat, 4> kPrologue{{
    {"吹雪の夜、巣が壊れた。", "小さな弟と妹は、寒さに震えている。", 6.4f},
    {"暖めるための小枝はある。", "でも、火がない。", 3.9f},
    {"山の神社には灯りがある。", "けれど、狛犬がそれを守っている。", 5.7f},
    {"子狐は、火を巣まで持ち帰れるのか？", "", 3.8f},
}};
constexpr Rectangle kPrologueNext{1088, 28, 252, 62};
constexpr Rectangle kPrologueSkip{1360, 28, 274, 62};
constexpr Rectangle kTitleStart{666, 780, 340, 91};

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

void Glow(Point center, float pulse, float strength = 1.0f) {
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient({center.x, center.y}, 205.0f + pulse * 14.0f,
                       Fade(kGold, 0.34f * strength), Fade(kGold, 0.0f));
    DrawCircleGradient({center.x, center.y}, 100.0f + pulse * 7.0f,
                       Fade(kGold, 0.40f * strength), Fade(kGold, 0.0f));
    EndBlendMode();
}

const char* GuardianAction(foxgame::Intent intent) {
    switch (intent) {
        case foxgame::Intent::InspectBell: return "鈴を調べる";
        case foxgame::Intent::InspectTracks: return "足跡を追う";
        case foxgame::Intent::InspectLastSeen: return "見た場所へ";
        case foxgame::Intent::WatchExit: return "森の道を見張る";
        case foxgame::Intent::WatchToriiExit: return "鳥居を見張る";
        case foxgame::Intent::WatchLogExit: return "倒木を見張る";
        case foxgame::Intent::FollowVisibleClue: return "狐を追う";
        case foxgame::Intent::WaitAndWatch: return "様子を見る";
        case foxgame::Intent::GuardLantern: return "";
    }
    return "";
}

// The marker sits on the actual interaction/transition point, so its cue matches
// the distance checks in World.cpp. Keep it above the scenery, but below the HUD.
void DrawGoal(Font font, Point foot, const char* label, const char* action, float time,
              bool prominent = true) {
    float pulse = 0.5f + 0.5f * std::sin(time * 3.1f);
    float lift = 5.0f * std::sin(time * 2.5f);
    Color accent = prominent ? kGold : Color{176, 218, 255, 255};
    float radius = prominent ? 82.0f : 61.0f;
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient({foot.x, foot.y - 25.0f}, radius + pulse * 24.0f,
                       Fade(accent, prominent ? 0.25f : 0.16f), Fade(accent, 0.0f));
    DrawCircleGradient({foot.x, foot.y - 18.0f}, 34.0f + pulse * 10.0f,
                       Fade(accent, 0.26f), Fade(accent, 0.0f));
    EndBlendMode();
    DrawEllipseLines(static_cast<int>(foot.x), static_cast<int>(foot.y + 3.0f),
                     radius * 0.65f + pulse * 7.0f, 19.0f + pulse * 3.0f,
                     Fade(accent, 0.65f + pulse * 0.3f));
    for (int i = 0; i < 5; ++i) {
        float angle = time * (i % 2 ? -1.0f : 1.0f) + i * 1.2566f;
        float x = foot.x + std::cos(angle) * (radius * 0.57f);
        float y = foot.y - 29.0f + std::sin(angle) * 19.0f;
        DrawCircleV({x, y}, 2.0f + pulse * 1.6f, Fade(accent, 0.55f + pulse * 0.35f));
    }
    Vector2 labelSize = MeasureTextEx(font, label, 24.0f, 1.0f);
    Vector2 actionSize = action[0] ? MeasureTextEx(font, action, 20.0f, 1.0f) : Vector2{};
    float width = std::max(labelSize.x, actionSize.x) + 34.0f;
    float height = action[0] ? 72.0f : 43.0f;
    float x = std::clamp(foot.x - width * 0.5f, 16.0f, kWidth - width - 16.0f);
    float y = std::max(150.0f, foot.y - 153.0f + lift);
    DrawTriangle({foot.x, y + height + 13.0f}, {foot.x - 10.0f, y + height - 2.0f},
                 {foot.x + 10.0f, y + height - 2.0f}, accent);
    DrawRectangleRounded({x, y, width, height}, 0.28f, 8, Fade(kBlue, 0.92f));
    DrawRectangleRoundedLinesEx({x, y, width, height}, 0.28f, 8, 2.0f,
                                Fade(accent, 0.72f + pulse * 0.28f));
    Text(font, label, x + (width - labelSize.x) * 0.5f, y + 7.0f, 24.0f, kIvory);
    if (action[0]) Text(font, action, x + (width - actionSize.x) * 0.5f,
                        y + 40.0f, 20.0f, accent);
}

void DrawBushCover(Font font, const foxgame::CoverZone& zone, const foxgame::Fox& fox,
                   bool carryingLantern, float time) {
    float dx = std::max({zone.left - fox.position.x, 0.0f, fox.position.x - zone.right});
    float dy = std::max({zone.top - fox.position.y, 0.0f, fox.position.y - zone.bottom});
    float distance = std::sqrt(dx * dx + dy * dy);
    float nearby = std::clamp((260.0f - distance) / 170.0f, 0.0f, 1.0f);
    float center = (zone.left + zone.right) * 0.5f;
    Color snow{220, 237, 238, 255};
    if (nearby > 0.0f) {
        DrawRectangleRoundedLinesEx({zone.left + 6.0f, zone.top + 6.0f,
                                     zone.right - zone.left - 12.0f,
                                     zone.bottom - zone.top - 12.0f}, 0.38f, 8, 2.0f,
                                    Fade(snow, nearby * 0.40f));
    }
    for (int i = 0; i < 3; ++i) {
        float x = zone.left + (zone.right - zone.left) * (0.19f + i * 0.31f);
        float y = zone.top + 7.0f + (i % 2) * 5.0f;
        float shimmer = 0.5f + 0.5f * std::sin(time * 1.8f + i * 2.1f);
        float strength = 0.10f + nearby * (0.10f + shimmer * 0.18f);
        DrawCircleGradient({x, y}, 13.0f, Fade(snow, strength), Fade(snow, 0.0f));
        DrawCircleV({x, y}, 1.8f, Fade(snow, strength + 0.12f));
    }
    if (nearby <= 0.0f) return;
    const char* hint = zone.Contains(fox.position) && fox.crouching ?
        (carryingLantern ? "隠れ中・灯籠の光も隠れる" : "隠れ中") :
        fox.crouching ? "茂みへ進む" : "Cで伏せて隠れる";
    float size = 20.0f;
    Vector2 measured = MeasureTextEx(font, hint, size, 1.0f);
    float x = center - measured.x * 0.5f;
    float y = zone.top - 30.0f;
    Text(font, hint, x + 1.0f, y + 2.0f, size, Fade(kBlue, nearby * 0.80f));
    Text(font, hint, x, y, size, Fade(kIvory, nearby * 0.82f));
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
    if (world.stage == Stage::Complete) return "小さな火で、巣はあたたかい";
    if (world.stage == Stage::NestEnding) return world.endingLanternPlaced ?
        "灯りを分ける" : "小枝のそばに灯籠を置こう";
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
    if (world.stage == Stage::NestIntro) return world.nestTwigsTouched ? "触れる" : "小枝を寄せる";
    if (world.stage == Stage::NestEnding) return "灯籠を置く";
    if (world.stage == Stage::ForestOutbound) return "触れる";
    if (world.lantern.owner == LanternOwner::Fox) return "灯籠を置く";
    if (foxgame::Distance(world.fox.position, world.lantern.position) < 95.0f) return "灯籠をくわえる";
    if (foxgame::Distance(world.fox.position, {350.0f, 500.0f}) < 100.0f) return "鈴を鳴らす";
    return "触れる";
}

std::array<const char*, 2> CaptureHint(const foxgame::WorldState& world) {
    if (world.lantern.owner == LanternOwner::Guardian)
        return {"置いた灯籠は狛犬に拾われる", "再挑戦したら、灯籠を持ったまま巣へ"};
    if (world.capturedScene == Stage::ForestReturn)
        return {"灯籠の光で見つかりやすい", "Cで伏せて茂みを通り、左の巣へ帰ろう"};
    if (world.capturedScene == Stage::Escape)
        return {"灯籠を持つと、遠くからも見つかる", "Cで伏せて茂みに隠れ、出口へ進もう"};
    return {"Cで伏せ、茂みを通って灯籠へ近づこう", "足跡はQで消し、狛犬が離れたら進もう"};
}

void Snow(float time) {
    for (int i = 0; i < 42; ++i) {
        float seed = static_cast<float>((i * 137 + 59) % 991) / 991.0f;
        float x = std::fmod(seed * kWidth + time * (13.0f + i % 7), static_cast<float>(kWidth));
        float y = std::fmod(static_cast<float>((i * 271 + 17) % 941) + time * (20.0f + i % 11), static_cast<float>(kHeight));
        DrawCircleV({x, y}, i % 5 == 0 ? 2.3f : 1.25f, Fade(kIvory, 0.34f));
    }
}

void DrawTitle(Font font, Texture2D background, Texture2D logo, Vector2 mouse, float time) {
    ClearBackground({8, 19, 39, 255});
    if (background.id)
        DrawTexturePro(background, {0, 0, static_cast<float>(background.width), static_cast<float>(background.height)},
                       {0, 0, static_cast<float>(kWidth), static_cast<float>(kHeight)}, {0, 0}, 0, WHITE);
    if (logo.id)
        DrawTexturePro(logo, {0, 0, static_cast<float>(logo.width), static_cast<float>(logo.height)},
                       {426, 0, 820, 462}, {0, 0}, 0, WHITE);
    else {
        const char* title = "こぎつねと冬の灯り";
        Vector2 size = MeasureTextEx(font, title, 70, 1);
        Text(font, title, (kWidth - size.x) * 0.5f, 125, 70, kIvory);
    }
    Snow(time);
    bool hover = CheckCollisionPointRec(mouse, kTitleStart);
    DrawRectangleRounded({kTitleStart.x - 6, kTitleStart.y - 6,
                          kTitleStart.width + 12, kTitleStart.height + 12},
                         0.32f, 12, Fade(kGold, hover ? 0.25f : 0.13f));
    DrawRectangleRounded(kTitleStart, 0.30f, 12,
                         hover ? Color{42, 64, 83, 248} : Color{17, 38, 60, 239});
    DrawRectangleRoundedLinesEx(kTitleStart, 0.30f, 12, 3.0f, hover ? kGold : Fade(kGold, 0.83f));
    const char* label = "はじめる";
    Vector2 labelSize = MeasureTextEx(font, label, 46, 1);
    Text(font, label, kTitleStart.x + (kTitleStart.width - labelSize.x) * 0.5f,
         kTitleStart.y + (kTitleStart.height - labelSize.y) * 0.5f - 3, 46, kIvory);
}

void DrawPrologue(Font font, Texture2D den, Texture2D shrineView, int beat, float elapsed, bool focused) {
    ClearBackground({8, 19, 32, 255});
    Texture2D still = beat < 2 ? den : shrineView;
    if (still.id) {
        Rectangle source = beat == 1 ? Rectangle{160, 120, 1440, 810} :
                                       Rectangle{0, 0, static_cast<float>(still.width), static_cast<float>(still.height)};
        DrawTexturePro(still, source, {0, 0, static_cast<float>(kWidth), static_cast<float>(kHeight)},
                       {0, 0}, 0, WHITE);
    }
    float edge = beat == 0 ? (kPrologue[beat].seconds - elapsed) / 0.35f :
        std::min(elapsed / 0.35f, (kPrologue[beat].seconds - elapsed) / 0.35f);
    if (edge < 1.0f) DrawRectangle(0, 0, kWidth, kHeight, Fade(BLACK, 1.0f - std::clamp(edge, 0.0f, 1.0f)));
    DrawRectangleGradientV(0, 610, kWidth, 331, Fade({3, 12, 24, 255}, 0.0f), Fade({3, 12, 24, 255}, 0.88f));
    DrawRectangleRounded({29, 27, 390, 66}, 0.28f, 8, Fade(kBlue, 0.76f));
    Text(font, "こぎつねと冬の灯り", 51, 43, 29, kIvory);
    DrawRectangleRounded(kPrologueNext, 0.24f, 8, Fade(kBlue, 0.86f));
    Text(font, beat == static_cast<int>(kPrologue.size()) - 1 ? "Enter 操作説明へ" : "Enter 次の場面",
         kPrologueNext.x + 18, kPrologueNext.y + 19, 22, kIvory);
    DrawRectangleRounded(kPrologueSkip, 0.24f, 8, Fade(kBlue, 0.86f));
    Text(font, "Space 導入を飛ばす", kPrologueSkip.x + 18, kPrologueSkip.y + 19, 22, kIvory);
    for (int i = 0; i < static_cast<int>(kPrologue.size()); ++i)
        DrawRectangleRounded({702.0f + i * 69.0f, 705.0f, 50.0f, 6.0f}, 1.0f, 4,
                             i == beat ? kGold : Fade(kIvory, 0.35f));
    DrawRectangleRounded({165, 733, 1342, 162}, 0.15f, 8, Fade(kBlue, 0.87f));
    const PrologueBeat& current = kPrologue[beat];
    auto centered = [&](const char* line, float y, float size) {
        Vector2 width = MeasureTextEx(font, line, size, 1.0f);
        Text(font, line, (kWidth - width.x) * 0.5f, y, size, kIvory);
    };
    if (current.secondLine[0]) {
        centered(current.firstLine, 756, 39);
        centered(current.secondLine, 815, 39);
    } else centered(current.firstLine, 785, 42);
    if (!focused) {
        DrawRectangle(0, 0, kWidth, kHeight, Fade(kBlue, 0.68f));
        centered("ゲーム画面をクリックして続ける", 445, 39);
    }
}

void TutorialKey(Font font, Rectangle bounds, const char* key, const char* label) {
    DrawRectangleRounded(bounds, 0.22f, 8, Fade(kBlue, 0.96f));
    DrawRectangleRoundedLinesEx(bounds, 0.22f, 8, 2.0f, Fade(kGold, 0.78f));
    Text(font, key, bounds.x + 15.0f, bounds.y + 8.0f, 25.0f, kGold);
    Text(font, label, bounds.x + 15.0f, bounds.y + 41.0f, 23.0f, kIvory);
}

void DrawTutorial(Font font, Texture2D art, int page, float time, bool focused) {
    ClearBackground({8, 19, 32, 255});
    if (art.id)
        DrawTexturePro(art, {0, 0, static_cast<float>(art.width), static_cast<float>(art.height)},
                       {0, 0, static_cast<float>(kWidth), static_cast<float>(kHeight)}, {0, 0}, 0, WHITE);
    DrawRectangleGradientV(0, 0, kWidth, 160, Fade(kBlue, 0.82f), Fade(kBlue, 0.0f));
    DrawRectangleGradientV(0, 610, kWidth, 331, Fade(kBlue, 0.0f), Fade(kBlue, 0.65f));
    DrawRectangleRounded({29, 27, 600, 67}, 0.25f, 8, Fade(kBlue, 0.90f));
    Text(font, page == 0 ? "あそびかた 1 / 2  動かす・触れる" :
                           "あそびかた 2 / 2  隠れる・足跡", 49, 44, 26, kIvory);
    DrawRectangleRounded(kPrologueNext, 0.24f, 8, Fade(kBlue, 0.92f));
    Text(font, page == 0 ? "Enter 次の説明" : "Enter ゲーム開始",
         kPrologueNext.x + 16, kPrologueNext.y + 19, 22, kIvory);
    DrawRectangleRounded(kPrologueSkip, 0.24f, 8, Fade(kBlue, 0.92f));
    Text(font, "Space 説明を飛ばす", kPrologueSkip.x + 18, kPrologueSkip.y + 19, 22, kIvory);

    float pulse = 0.5f + 0.5f * std::sin(time * 2.8f);
    if (page == 0) {
        DrawEllipseLines(835, 620, 105.0f + pulse * 12.0f, 36.0f + pulse * 5.0f,
                         Fade(kGold, 0.55f + pulse * 0.40f));
    } else {
        DrawEllipseLines(645, 497, 195.0f + pulse * 9.0f, 119.0f + pulse * 6.0f,
                         Fade(kGold, 0.65f + pulse * 0.30f));
        DrawRectangleRounded({484, 309, 266, 55}, 0.25f, 8, Fade(kBlue, 0.90f));
        Text(font, "ここが隠れる茂み", 502, 319, 24, kIvory);
        DrawLineEx({615, 365}, {645, 390}, 3.0f, kGold);
    }

    DrawRectangleRounded({39, 694, 1594, 217}, 0.10f, 8, Fade(kBlue, 0.94f));
    DrawRectangleRoundedLinesEx({39, 694, 1594, 217}, 0.10f, 8, 2.0f, Fade(kGold, 0.46f));
    if (page == 0) {
        Text(font, "WASD / 矢印 で子狐を動かそう", 70, 715, 32, kIvory);
        Text(font, "光る小枝や灯籠のそばで E を押そう", 70, 773, 28, kGold);
        Text(font, "右下の4つのボタンはクリックでも使える", 70, 830, 24, kIvory);
    } else {
        Text(font, "Cで伏せ、雪をかぶった茂みに入ろう", 70, 710, 27, kIvory);
        Text(font, "Qで近くの足跡をしっぽで消そう", 70, 760, 27, kIvory);
        Text(font, "Shiftで走れる。見つかりやすくなるので注意", 70, 810, 25, kGold);
        Text(font, "C・Shiftは押すたび切替。倒木は伏せて通る", 70, 857, 23, kIvory);
    }
    TutorialKey(font, {1040, 708, 260, 84}, "C", "伏せる");
    TutorialKey(font, {1320, 708, 260, 84}, "Shift", "走る");
    TutorialKey(font, {1040, 803, 260, 84}, "Q", "足跡を払う");
    TutorialKey(font, {1320, 803, 260, 84}, "E", "触れる");
    for (int i = 0; i < 2; ++i)
        DrawCircleV({814.0f + i * 44.0f, 920.0f}, 7.0f, i == page ? kGold : Fade(kIvory, 0.4f));
    if (!focused) {
        DrawRectangle(0, 0, kWidth, kHeight, Fade(kBlue, 0.67f));
        Text(font, "ゲーム画面をクリックして続ける", 530, 435, 39, kIvory);
    }
}

void Present(RenderTexture2D canvas) {
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
    Texture2D nestFireWatched = LoadTexture(Asset("nest-fire-watched-v2.png").c_str());
    Texture2D nestGuardianDeparts = LoadTexture(Asset("nest-guardian-departs-v2.png").c_str());
    Texture2D nestSafeNight = LoadTexture(Asset("nest-safe-night-v1.png").c_str());
    Texture2D prologueDen = LoadTexture(Asset("prologue-cold-den-v1.png").c_str());
    Texture2D prologueShrine = LoadTexture(Asset("prologue-shrine-from-den-v1.png").c_str());
    Texture2D titleBackground = LoadTexture(Asset("title-background-v1.png").c_str());
    Texture2D titleLogo = LoadTexture(Asset("title-logo-v1.png").c_str());
    Texture2D tutorialMove = LoadTexture(Asset("tutorial-move-interact-v1.png").c_str());
    Texture2D tutorialHide = LoadTexture(Asset("tutorial-hide-tracks-v1.png").c_str());
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
        "狛犬に見つかった狛犬が灯籠の光を見つけたでチェックポイントからやり直す境内の手前から再開鳥居の手前から再開"
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
        "巣の小道を進み小枝のそばに灯籠を置こう小枝のそばまで灯籠を運び"
        "小枝に火が灯る狛犬は少し離れて見守った狛犬は灯籠を持ち静かに神社へ帰った"
        "小さな火で巣はあたたかい狐たちは安心して寄り添った"
        "灯籠を小枝のそばに置いた森から足音が近づく灯籠の光をたどった狛犬が弟妹を見る"
        "森の高台で灯籠の光が見えた光をたどった狛犬が弟妹を見つめる"
        "はじめるタイトルへ戻る一時停止導入を飛ばす次の場面"
        "ゲーム画面をクリックして操作クリック後に移動文字入力中なら画面を選んでください"
        "倒木の下では伏せたまま進もう走れない"
        "ローカル試作Jev中継接続待ち代替判断模擬応答確認中"
        "鈴を調べる足跡を追う見た場所へ森の道を見張る鳥居倒木狐様子を見る"
        "吹雪の夜、巣が壊れた。小さな弟と妹は、寒さに震えている。"
        "暖めるための小枝はある。でも、火がない。"
        "山の神社には灯りがある。けれど、狛犬がそれを守っている。"
        "子狐は、火を巣まで持ち帰れるのか？開始"
        "移動保存操作姿を隠して進もう右の倒木は伏せて通れる再挑戦続きは次の場面へ"
        "小枝Eで触れる森への小道神社へ鳥居倒木Cで伏せる巣へ帰る灯籠Eでくわえる"
        "次のヒント置いた灯籠は狛犬に拾われる再挑戦したら持ったまま巣へ"
        "灯籠の光で見つかりやすい伏せて茂みを通り左の巣へ帰ろう"
        "灯籠を持つと遠くからも見つかる茂みに隠れ出口へ進もう"
        "茂みを通って灯籠へ近づこう足跡は消し狛犬が離れたら進もう"
        "あそびかた動かす隠れる足跡操作説明へ次の説明ゲーム開始説明を飛ばす"
        "で子狐を動かそう光る小枝や灯籠のそばで押そう右下の4つのボタンはクリックでも使える"
        "ここが隠れる茂み雪をかぶった茂みに入ろう近くの足跡をしっぽで消そう"
        "見つかりやすくなるので注意押すたび切替倒木は伏せて通る"
        "隠れている茂みへ進むCで伏せて隠れる"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!?:・、。←→"
        "  -./_·ぽ印矢";
    int codepointCount = 0;
    int* codepoints = LoadCodepoints(glyphs.c_str(), &codepointCount);
    Font font = LoadFontEx(Asset("NotoSansCJKjp-Regular.otf").c_str(), 44, codepoints, codepointCount);
    UnloadCodepoints(codepoints);
    if (font.texture.id) SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    Game game;
    foxgame::GameAudio audio;
    audio.LoadNarration({Asset("prologue-01.ogg"), Asset("prologue-02.ogg"),
                         Asset("prologue-03.ogg"), Asset("prologue-04.ogg")});
    if (!audio.LoadMusicPack(Asset("music.pak"))) TraceLog(LOG_WARNING, "BGM pack could not be loaded");
    const char* relayUrl = std::getenv("JEV_RELAY_URL");
    foxgame::JevBridge bridge(relayUrl ? relayUrl : "");
    game.SetExternalDecisionMode(bridge.Configured());
    bool paused = false;
    bool titleActive = true;
    bool prologueActive = false;
    bool tutorialActive = false;
    int tutorialPage = 0;
    bool narrationStarted = false;
    bool prologueInputArmed = false;
    int prologueBeat = 0;
    float prologueElapsed = 0.0f;
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
        Vector2 mouse = ToCanvas(GetMousePosition());
        float frameTime = GetFrameTime();
        Stage musicStage = game.State().stage == Stage::Captured ? game.State().capturedScene : game.State().stage;
        foxgame::MusicCue musicCue = titleActive ? foxgame::MusicCue::Title :
            prologueActive || musicStage == Stage::NestIntro ? foxgame::MusicCue::Prologue :
            musicStage == Stage::ForestOutbound || musicStage == Stage::ForestReturn ? foxgame::MusicCue::Forest :
            musicStage == Stage::NestEnding || musicStage == Stage::Complete ? foxgame::MusicCue::Ending :
            foxgame::MusicCue::Shrine;
        audio.UpdateMusic(musicCue, frameTime, focused, paused && !titleActive && !prologueActive, prologueActive);
        if (titleActive) {
            bool clickStart = CheckCollisionPointRec(mouse, kTitleStart) &&
                              (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT));
            bool start = (focused && (justPressed(KEY_SPACE) || justPressed(KEY_ENTER))) || clickStart;
            if (start) {
                titleActive = false;
                prologueActive = true;
                tutorialActive = false;
                tutorialPage = 0;
                narrationStarted = false;
                prologueInputArmed = false;
                prologueBeat = 0;
                prologueElapsed = 0.0f;
            } else {
                BeginTextureMode(canvas);
                DrawTitle(font, titleBackground, titleLogo, mouse, static_cast<float>(GetTime()));
                EndTextureMode();
                Present(canvas);
            }
            continue;
        }
        if (prologueActive) {
            if (!focused) audio.PauseNarration();
            else {
                if (!prologueInputArmed) {
                    // The press that started the game must be released before it can advance the story.
                    prologueInputArmed = !IsKeyDown(KEY_ENTER) && !IsKeyDown(KEY_SPACE) &&
                        !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
                        !justPressed(KEY_ENTER) && !justPressed(KEY_SPACE) &&
                        !IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
                } else {
                    if (!narrationStarted) {
                        audio.PlayNarration(prologueBeat);
                        narrationStarted = true;
                    } else audio.ResumeNarration();
                    bool skip = justPressed(KEY_SPACE) ||
                                (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                                 CheckCollisionPointRec(mouse, kPrologueSkip));
                    bool advance = justPressed(KEY_ENTER) ||
                                   (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                                    CheckCollisionPointRec(mouse, kPrologueNext));
                    if (skip) {
                        audio.StopNarration();
                        prologueActive = false;
                    } else if (advance) {
                        prologueElapsed = 0.0f;
                        if (++prologueBeat == static_cast<int>(kPrologue.size())) {
                            audio.StopNarration();
                            prologueActive = false;
                            tutorialActive = true;
                            tutorialPage = 0;
                        } else audio.PlayNarration(prologueBeat);
                    } else {
                        prologueElapsed += std::clamp(frameTime, 0.0f, 0.05f);
                        if (prologueElapsed >= kPrologue[prologueBeat].seconds) {
                            prologueElapsed = 0.0f;
                            if (++prologueBeat == static_cast<int>(kPrologue.size())) {
                                audio.StopNarration();
                                prologueActive = false;
                                tutorialActive = true;
                                tutorialPage = 0;
                            } else audio.PlayNarration(prologueBeat);
                        }
                    }
                }
            }
            if (prologueActive) {
                BeginTextureMode(canvas);
                DrawPrologue(font, prologueDen, prologueShrine, prologueBeat, prologueElapsed, focused);
                EndTextureMode();
                Present(canvas);
            }
            continue;
        }
        if (tutorialActive) {
            if (focused) {
                bool skip = justPressed(KEY_SPACE) ||
                            (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                             CheckCollisionPointRec(mouse, kPrologueSkip));
                bool next = justPressed(KEY_ENTER) ||
                            (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                             CheckCollisionPointRec(mouse, kPrologueNext));
                if (skip) tutorialActive = false;
                else if (next) {
                    if (tutorialPage == 0) tutorialPage = 1;
                    else tutorialActive = false;
                }
            }
            if (tutorialActive) {
                BeginTextureMode(canvas);
                DrawTutorial(font, tutorialPage == 0 ? tutorialMove : tutorialHide,
                             tutorialPage, static_cast<float>(GetTime()), focused);
                EndTextureMode();
                Present(canvas);
                continue;
            }
        }
        if (justPressed(KEY_ESCAPE)) paused = !paused;
        const auto& world = game.State();
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
                        !(world.stage == Stage::NestEnding && world.endingLanternPlaced) &&
                        world.stage != Stage::Complete && !paused && focused;
        bool clicked[4]{};
        if (playable && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            for (int i = 0; i < 4; ++i) clicked[i] = CheckCollisionPointRec(mouse, buttons[i]);
        }
        input.toggleCrouch |= clicked[0];
        input.toggleRun |= clicked[1];
        input.sweep |= clicked[2];
        input.interact |= clicked[3];
        if (!paused && focused) {
            bool restartFromEnding = world.stage == Stage::Complete && input.restart;
            game.Update(input, frameTime);
            bridge.Poll(game);
            audio.Poll(game.Sounds());
            if (restartFromEnding && game.State().stage == Stage::NestIntro) {
                titleActive = true;
                prologueActive = false;
                tutorialActive = false;
                tutorialPage = 0;
                narrationStarted = false;
                prologueInputArmed = false;
                prologueBeat = 0;
                prologueElapsed = 0.0f;
                movementPulse.fill(0.0f);
                continue;
            }
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
        bool endingWalk = scene == Stage::NestEnding && !state.endingLanternPlaced;
        bool hidden = game.FoxHidden() && (shrineScene || returnScene);
        Point light = state.lantern.owner == LanternOwner::Fox ?
            Point{state.fox.position.x + 44.0f * state.fox.facingX, state.fox.position.y - 76.0f} :
            state.lantern.owner == LanternOwner::Guardian ?
            Point{state.guardian.position.x, state.guardian.position.y - 70.0f} :
            state.lantern.position;

        BeginTextureMode(canvas);
        ClearBackground({12, 26, 40, 255});
        Texture2D* plate = &shrineBackground;
        if (scene == Stage::NestIntro || endingScene) plate = &nestColdBackground;
        else if (scene == Stage::ForestOutbound) plate = &forestOutboundBackground;
        else if (returnScene) plate = &forestReturnBackground;
        if (plate->id) DrawTexturePro(*plate, {0, 0, static_cast<float>(plate->width), static_cast<float>(plate->height)},
                                      {0, 0, static_cast<float>(kWidth), static_cast<float>(kHeight)}, {0, 0}, 0, WHITE);
        bool liveLantern = shrineScene || returnScene ||
            (scene == Stage::NestEnding && (endingWalk || state.stageTime < 7.0f));
        if (liveLantern) Glow({light.x, light.y - (state.lantern.owner == LanternOwner::Fox ? 10.0f : 58.0f)},
                              std::sin(time * 2.3f), hidden && state.lantern.owner == LanternOwner::Fox ? 0.10f : 1.0f);
        if (returnScene || shrineScene) {
            for (const auto& zone : foxgame::BushCoverZones(scene))
                DrawBushCover(font, zone, state.fox, state.lantern.owner == LanternOwner::Fox, time);
        }

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
            float guardianFade = scene == Stage::NestEnding ?
                std::clamp((state.stageTime - 1.5f) / 1.5f, 0.0f, 1.0f) : 1.0f;
            float gait = state.guardian.walkDistance / 19.0f;
            float bob = isStone ? 0.0f : state.guardian.moving ?
                -2.8f * std::abs(std::sin(gait)) : 0.0f;
            Shadow(guardianFoot, isStone ? 70.0f : 87.0f, 0.22f * guardianFade);
            if (!isStone && state.guardian.moving) {
                float step = std::sin(gait);
                DrawEllipse(static_cast<int>(guardianFoot.x - 26 + 9 * step),
                            static_cast<int>(guardianFoot.y - 4), 12, 4, Fade(kBlue, 0.34f * guardianFade));
                DrawEllipse(static_cast<int>(guardianFoot.x + 26 - 9 * step),
                            static_cast<int>(guardianFoot.y - 4), 12, 4, Fade(kBlue, 0.34f * guardianFade));
            }
            DrawSprite(isStone ? stone : animal, {guardianFoot.x, guardianFoot.y + bob}, isStone ? 188.0f : 215.0f,
                       isStone ? 181.0f : 148.0f, state.guardian.facing.x < 0.0f,
                       Fade(WHITE, guardianFade));
            if (isStone && state.guardian.form == foxgame::GuardianForm::Stone) {
                float pulse = 0.5f + 0.5f * std::sin(time * 2.2f);
                DrawCircleGradient({guardianFoot.x - 25, guardianFoot.y - 121}, 12.0f + pulse * 5,
                                   Fade(kGold, state.knowledge.bellHeard ? 0.72f : 0.34f), Fade(kGold, 0.0f));
            }
            const char* action = state.guardian.form == foxgame::GuardianForm::Returning ?
                "持ち場へ戻る" : GuardianAction(state.guardian.intent);
            if (action[0] && state.stage != Stage::Captured && scene != Stage::NestEnding) {
                Vector2 label = MeasureTextEx(font, action, 20, 1);
                float labelX = guardianFoot.x - label.x * 0.5f;
                float labelY = guardianFoot.y - (isStone ? 220.0f : 187.0f);
                DrawRectangleRounded({labelX - 11, labelY - 5, label.x + 22, 31},
                                     0.35f, 6, Fade(kBlue, 0.78f));
                Text(font, action, labelX, labelY, 20, kIvory);
            }
            if (state.knowledge.foxVisible && state.stage != Stage::Captured && !endingScene) {
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
        bool showFox = !endingScene || endingWalk || (scene == Stage::NestEnding && state.stageTime < 7.0f);
        bool showGuardian = shrineScene || (returnScene && state.guardianEnteredForest) ||
                            (scene == Stage::NestEnding && state.endingLanternPlaced &&
                             state.stageTime >= 2.0f && state.stageTime < 7.0f);
        if (showFox && showGuardian) {
            if (guardianFoot.y < foxFoot.y) { drawGuardian(); drawFox(); }
            else { drawFox(); drawGuardian(); }
        } else {
            if (showFox) drawFox();
            if (showGuardian) drawGuardian();
        }
        if (liveLantern && (state.lantern.owner == LanternOwner::Pedestal ||
            state.lantern.owner == LanternOwner::Ground || state.lantern.owner == LanternOwner::Fox ||
            state.lantern.owner == LanternOwner::Guardian)) {
            Point lanternFoot = state.lantern.owner == LanternOwner::Fox ?
                Point{foxFoot.x + 49.0f * state.fox.facingX, foxFoot.y - 31.0f} :
                state.lantern.owner == LanternOwner::Guardian ? guardianFoot : state.lantern.position;
            if (state.lantern.owner != LanternOwner::Fox) Shadow(lanternFoot, 23.0f, 0.18f);
            DrawSprite(lantern, lanternFoot, 64.0f, 86.0f);
        }
        if (endingScene && (scene == Stage::Complete ||
                            (!endingWalk && state.stageTime >= 7.0f))) {
            if (nestFireWatched.id) {
                DrawTexturePro(nestFireWatched,
                               {0, 0, static_cast<float>(nestFireWatched.width), static_cast<float>(nestFireWatched.height)},
                               {0, 0, static_cast<float>(kWidth), static_cast<float>(kHeight)}, {0, 0}, 0,
                               WHITE);
            }
            float departureFade = scene == Stage::Complete ? 1.0f :
                std::clamp((state.stageTime - 10.6f) / 0.8f, 0.0f, 1.0f);
            if (nestGuardianDeparts.id && departureFade > 0.0f) {
                DrawTexturePro(nestGuardianDeparts,
                               {0, 0, static_cast<float>(nestGuardianDeparts.width), static_cast<float>(nestGuardianDeparts.height)},
                               {0, 0, static_cast<float>(kWidth), static_cast<float>(kHeight)}, {0, 0}, 0,
                               Fade(WHITE, departureFade));
            }
            float safeNightFade = scene == Stage::Complete ? 1.0f :
                std::clamp((state.stageTime - 14.0f) / 0.9f, 0.0f, 1.0f);
            if (nestSafeNight.id && safeNightFade > 0.0f) {
                DrawTexturePro(nestSafeNight,
                               {0, 0, static_cast<float>(nestSafeNight.width), static_cast<float>(nestSafeNight.height)},
                               {0, 0, static_cast<float>(kWidth), static_cast<float>(kHeight)}, {0, 0}, 0,
                               Fade(WHITE, safeNightFade));
            }
        }
        if (scene == Stage::NestEnding && state.endingLanternPlaced && state.stageTime >= 6.4f &&
            state.stageTime < 8.1f) {
            float dark = state.stageTime < 7.0f ?
                (state.stageTime - 6.4f) / 0.6f : 1.0f - (state.stageTime - 7.0f) / 1.1f;
            DrawRectangle(0, 0, kWidth, kHeight, Fade(kBlue, std::clamp(dark, 0.0f, 1.0f)));
        }
        if (!endingScene || endingWalk || state.stageTime < 7.0f) Snow(time);

        if (state.stage != Stage::Captured && state.stage != Stage::SliceComplete &&
            (!endingScene || endingWalk)) {
            if (endingWalk) {
                Point twigs{820.0f, 620.0f};
                bool near = foxgame::Distance(state.fox.position, twigs) <= 95.0f;
                DrawGoal(font, twigs, "小枝", near ? "E で灯籠を置く" : "近づこう", time);
            } else if (scene == Stage::NestIntro) {
                if (!state.nestTwigsTouched) {
                    Point twigs{820.0f, 620.0f};
                    bool near = foxgame::Distance(state.fox.position, twigs) <= 140.0f;
                    DrawGoal(font, twigs, "小枝", near ? "E で触れる" : "近づこう", time);
                } else DrawGoal(font, {280.0f, 430.0f}, "森への小道", "", time);
            } else if (scene == Stage::ForestOutbound) {
                DrawGoal(font, {530.0f, 440.0f}, "鳥居から神社へ", "", time);
                DrawGoal(font, {1420.0f, 385.0f}, "倒木から神社へ", "C で伏せる", time, false);
            } else if (returnScene) {
                if (state.lantern.owner == LanternOwner::Fox)
                    DrawGoal(font, {270.0f, 730.0f}, "巣へ帰る", "", time);
                else if (state.lantern.owner == LanternOwner::Ground) {
                    bool near = foxgame::Distance(state.fox.position, state.lantern.position) <= 95.0f;
                    DrawGoal(font, state.lantern.position, "灯籠", near ? "E でくわえる" : "近づこう", time);
                }
            } else if (shrineScene) {
                if (state.lantern.owner == LanternOwner::Pedestal ||
                    state.lantern.owner == LanternOwner::Ground) {
                    bool near = foxgame::Distance(state.fox.position, state.lantern.position) <= 95.0f;
                    DrawGoal(font, state.lantern.position, "灯籠", near ? "E でくわえる" : "近づこう", time);
                } else if (state.lantern.owner == LanternOwner::Fox) {
                    DrawGoal(font, {160.0f, 735.0f}, "鳥居から森へ", "", time);
                    DrawGoal(font, {1480.0f, 345.0f}, "倒木から森へ", "C で伏せる", time, false);
                }
            }
        }

        DrawRectangleRounded({31, 24, 605, 104}, 0.18f, 8, Fade(kBlue, 0.87f));
        Text(font, "こぎつねと冬の灯り", 52, 31, 34, kIvory);
        Text(font, Objective(state), 53, 78, 25, kGold);
        const char* mode = "ローカル試作";
        switch (bridge.CurrentStatus()) {
            case foxgame::JevBridge::Status::Local: break;
            case foxgame::JevBridge::Status::Waiting: mode = "応答確認中"; break;
            case foxgame::JevBridge::Status::Jev: mode = "Jev中継"; break;
            case foxgame::JevBridge::Status::Mock: mode = "模擬応答"; break;
            case foxgame::JevBridge::Status::Fallback: mode = "代替判断"; break;
        }
        Vector2 modeWidth = MeasureTextEx(font, mode, 22, 1);
        Text(font, mode, kWidth - modeWidth.x - 35, 37, 22, Fade(kIvory, 0.87f));
        if (state.stage != Stage::Captured && state.stage != Stage::SliceComplete &&
            (!endingScene || endingWalk)) {
            DrawRectangleRounded({31, 851, 680, 59}, 0.26f, 8, Fade(kBlue, 0.80f));
            float noticeSize = MeasureTextEx(font, game.Notice().c_str(), 24, 1).x > 640 ? 21.0f : 24.0f;
            Text(font, game.Notice().c_str(), 49, 863, noticeSize, kIvory);
        }
        Button(font, buttons[0], "伏せる", "C", state.fox.crouching, playable, mouse);
        Button(font, buttons[1], "走る", "Shift", state.fox.running, playable, mouse);
        Button(font, buttons[2], "尾で払う", "Q", false, playable, mouse);
        Button(font, buttons[3], Interaction(state), "E", false, playable, mouse);

        if (state.stage == Stage::NestEnding && state.endingLanternPlaced) {
            const char* story = state.stageTime < 3.0f ? "森の高台で、灯籠の光が見えた" :
                                state.stageTime < 7.0f ? "光をたどった狛犬が、弟妹を見つめる" :
                                state.stageTime < 11.0f ? "小枝に火が灯る。狛犬は、少し離れて見守った" :
                                state.stageTime < 14.0f ? "狛犬は灯籠を持ち、静かに神社へ帰った" :
                                                          "狐たちは、安心して寄り添った";
            DrawRectangleRounded({270, 730, 1140, 78}, 0.20f, 8, Fade(kBlue, 0.83f));
            Vector2 size = MeasureTextEx(font, story, 34, 1);
            Text(font, story, (kWidth - size.x) * 0.5f, 750, 34, kIvory);
        }

        if (state.stage == Stage::Complete && focused && !paused) {
            DrawRectangleRounded({1120, 837, 495, 70}, 0.25f, 8, Fade(kBlue, 0.80f));
            Text(font, "R でタイトルへ戻る", 1150, 852, 28, kIvory);
        } else if (!focused || paused || state.stage == Stage::Captured ||
                   state.stage == Stage::SliceComplete) {
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
                       state.stage == Stage::Complete ? "R でタイトルへ戻る" : "Esc で再開",
                       493, 423, 31, kGold);
            if (state.stage == Stage::Captured && focused) {
                auto hint = CaptureHint(state);
                Text(font, "次のヒント", 493, 502, 25, kGold);
                Text(font, hint[0], 493, 542, 27, kIvory);
                Text(font, hint[1], 493, 586, 27, kIvory);
            } else {
                Text(font, "移動 WASD / 矢印  ·  伏せる C  ·  走る Shift", 493, 515, 24, kIvory);
                Text(font, "足跡を払う Q  ·  触れる E", 493, 562, 24, kIvory);
            }
        }
        EndTextureMode();

        Present(canvas);
    }

    if (font.texture.id) UnloadFont(font);
    if (shrineBackground.id) UnloadTexture(shrineBackground);
    if (nestColdBackground.id) UnloadTexture(nestColdBackground);
    if (forestOutboundBackground.id) UnloadTexture(forestOutboundBackground);
    if (forestReturnBackground.id) UnloadTexture(forestReturnBackground);
    if (nestFireWatched.id) UnloadTexture(nestFireWatched);
    if (nestGuardianDeparts.id) UnloadTexture(nestGuardianDeparts);
    if (nestSafeNight.id) UnloadTexture(nestSafeNight);
    if (prologueDen.id) UnloadTexture(prologueDen);
    if (prologueShrine.id) UnloadTexture(prologueShrine);
    if (titleBackground.id) UnloadTexture(titleBackground);
    if (titleLogo.id) UnloadTexture(titleLogo);
    if (tutorialMove.id) UnloadTexture(tutorialMove);
    if (tutorialHide.id) UnloadTexture(tutorialHide);
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
