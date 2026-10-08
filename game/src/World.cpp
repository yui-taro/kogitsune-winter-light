#include "World.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace foxgame {
namespace {

constexpr Point kPedestal{780.0f, 445.0f};
constexpr Point kBell{350.0f, 500.0f};
constexpr Point kToriiExit{160.0f, 735.0f};
constexpr Point kLogExit{1480.0f, 345.0f};

Point Sub(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
Point Add(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
Point Mul(Point a, float amount) { return {a.x * amount, a.y * amount}; }
float Length(Point a) { return std::sqrt(a.x * a.x + a.y * a.y); }
Point Normalized(Point a) { return Length(a) > 0.0001f ? Mul(a, 1.0f / Length(a)) : Point{}; }
float Dot(Point a, Point b) { return a.x * b.x + a.y * b.y; }
bool Near(Point a, Point b, float radius) { return Distance(a, b) <= radius; }
bool Legal(const DecisionRequest& request, Intent intent) {
    return std::find(request.legalIntents.begin(), request.legalIntents.end(), intent) != request.legalIntents.end();
}

float DistanceToSegment(Point p, Point a, Point b) {
    Point edge = Sub(b, a);
    float lengthSquared = Dot(edge, edge);
    float t = lengthSquared > 0.0f ? std::clamp(Dot(Sub(p, a), edge) / lengthSquared, 0.0f, 1.0f) : 0.0f;
    return Distance(p, Add(a, Mul(edge, t)));
}

template <std::size_t N>
bool InPolygon(Point p, const std::array<Point, N>& vertices) {
    bool inside = false;
    for (std::size_t i = 0, j = N - 1; i < N; j = i++) {
        Point a = vertices[i], b = vertices[j];
        if ((a.y > p.y) != (b.y > p.y) &&
            p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
    }
    return inside;
}

bool OnForestTrail(Point p) {
    constexpr std::array<Point, 4> toriiTrail{{{345, 760}, {370, 680}, {445, 555}, {530, 440}}};
    constexpr std::array<Point, 5> logTrail{{{445, 555}, {760, 555}, {1030, 515}, {1270, 450}, {1420, 385}}};
    constexpr std::array<Point, 3> homeTrail{{{445, 555}, {330, 665}, {270, 730}}};
    auto nearTrail = [p](const auto& trail, float width) {
        for (std::size_t i = 1; i < trail.size(); ++i)
            if (DistanceToSegment(p, trail[i - 1], trail[i]) <= width) return true;
        return false;
    };
    return nearTrail(toriiTrail, 116.0f) || nearTrail(logTrail, 115.0f) ||
           nearTrail(homeTrail, 112.0f);
}

bool OnShrineGround(Point p) {
    if (p.y < 330.0f) return false;
    constexpr std::array<Point, 3> foreground{{{165, 745}, {760, 745}, {1480, 745}}};
    constexpr std::array<Point, 4> toriiWay{{{345, 745}, {410, 625}, {500, 520}, {850, 450}}};
    constexpr std::array<Point, 3> returnWay{{{850, 450}, {1180, 450}, {1450, 450}}};
    constexpr std::array<Point, 3> upperWay{{{850, 450}, {1060, 355}, {1480, 345}}};
    constexpr std::array<Point, 2> logWay{{{1450, 745}, {1450, 345}}};
    auto nearTrail = [p](const auto& trail, float width) {
        for (std::size_t i = 1; i < trail.size(); ++i)
            if (DistanceToSegment(p, trail[i - 1], trail[i]) <= width) return true;
        return false;
    };
    return nearTrail(foreground, 160.0f) || nearTrail(toriiWay, 145.0f) ||
           nearTrail(returnWay, 110.0f) ||
           nearTrail(upperWay, 92.0f) || nearTrail(logWay, 130.0f);
}

struct Barrier { float left, top, right, bottom; };

Barrier LowPassage(Stage stage) {
    return stage == Stage::ForestReturn || stage == Stage::ForestOutbound ?
        Barrier{1290.0f, 375.0f, 1540.0f, 465.0f} : Barrier{1350.0f, 455.0f, 1570.0f, 535.0f};
}

bool Inside(Point p, Barrier b) {
    return p.x > b.left && p.x < b.right && p.y > b.top && p.y < b.bottom;
}

bool CrossesBarrier(Point from, Point to, Barrier b) {
    float enter = 0.0f, leave = 1.0f;
    auto clip = [&](float start, float delta, float low, float high) {
        if (std::abs(delta) < 0.0001f) return start > low && start < high;
        float a = (low - start) / delta, c = (high - start) / delta;
        if (a > c) std::swap(a, c);
        enter = std::max(enter, a);
        leave = std::min(leave, c);
        return enter < leave;
    };
    return clip(from.x, to.x - from.x, b.left, b.right) &&
           clip(from.y, to.y - from.y, b.top, b.bottom) && enter < 1.0f && leave > 0.0f;
}

Point GuardianWaypoint(Point from, Point target, Barrier b) {
    constexpr float margin = 22.0f;
    if (Inside(target, b)) {
        std::array<Point, 4> exits{{{b.left - margin, target.y}, {b.right + margin, target.y},
                                    {target.x, b.top - margin}, {target.x, b.bottom + margin}}};
        target = *std::min_element(exits.begin(), exits.end(), [&](Point a, Point c) {
            return Distance(from, a) < Distance(from, c);
        });
    }
    if (!CrossesBarrier(from, target, b)) return target;
    std::array<Point, 6> nodes{{from, target, {b.left - margin, b.top - margin},
                                {b.right + margin, b.top - margin},
                                {b.left - margin, b.bottom + margin},
                                {b.right + margin, b.bottom + margin}}};
    std::array<float, 6> cost{};
    std::array<int, 6> previous{};
    std::array<bool, 6> visited{};
    cost.fill(std::numeric_limits<float>::infinity());
    previous.fill(-1);
    cost[0] = 0.0f;
    for (int step = 0; step < 6; ++step) {
        int current = -1;
        for (int i = 0; i < 6; ++i)
            if (!visited[i] && (current < 0 || cost[i] < cost[current])) current = i;
        if (current < 0 || !std::isfinite(cost[current]) || current == 1) break;
        visited[current] = true;
        for (int i = 1; i < 6; ++i) {
            if (visited[i] || CrossesBarrier(nodes[current], nodes[i], b)) continue;
            float candidate = cost[current] + Distance(nodes[current], nodes[i]);
            if (candidate < cost[i]) { cost[i] = candidate; previous[i] = current; }
        }
    }
    if (previous[1] < 0) return from;
    int waypoint = 1;
    while (previous[waypoint] > 0) waypoint = previous[waypoint];
    return nodes[waypoint];
}

} // namespace

float Distance(Point a, Point b) { return Length(Sub(a, b)); }

const char* IntentName(Intent intent) {
    switch (intent) {
        case Intent::GuardLantern: return "guard_lantern";
        case Intent::InspectBell: return "inspect_bell";
        case Intent::InspectTracks: return "inspect_tracks";
        case Intent::InspectLastSeen: return "inspect_last_seen";
        case Intent::WatchExit: return "watch_exit";
        case Intent::FollowVisibleClue: return "follow_visible_clue";
        case Intent::WaitAndWatch: return "wait_and_watch";
    }
    return "wait_and_watch";
}

Intent LocalPersonalityPolicy::Choose(const DecisionRequest& request) const {
    const auto& k = request.knowledge;
    // The guardian is dutiful but curious; it follows evidence, then returns to the lantern.
    if (k.foxVisible && Legal(request, Intent::FollowVisibleClue)) return Intent::FollowVisibleClue;
    if (k.theftKnown && k.hasLastSeen && Legal(request, Intent::InspectLastSeen)) return Intent::InspectLastSeen;
    if (k.tracksSeen && Legal(request, Intent::InspectTracks)) return Intent::InspectTracks;
    if (k.bellHeard && !k.falseAlarm && Legal(request, Intent::InspectBell)) return Intent::InspectBell;
    if (k.theftKnown && Legal(request, Intent::WatchExit)) return Intent::WatchExit;
    if (Legal(request, Intent::GuardLantern)) return Intent::GuardLantern;
    return request.legalIntents.empty() ? Intent::WaitAndWatch : request.legalIntents.front();
}

Game::Game(StartMode start) : practiceMode_(start == StartMode::ShrinePractice) {
    if (practiceMode_) {
        world_.stage = Stage::Approach;
        checkpointB_ = world_;
    } else {
        world_.stage = Stage::NestIntro;
        world_.fox.position = {690.0f, 650.0f};
        notice_ = "弟妹のそばの小枝に触れてみよう";
    }
}

bool Game::CanMoveTo(Point p) const {
    if (world_.stage == Stage::NestIntro) {
        constexpr std::array<Point, 15> clearing{{
            {260, 370}, {350, 370}, {440, 435}, {515, 500}, {630, 555},
            {800, 595}, {1240, 615}, {1380, 660}, {1380, 850}, {600, 850},
            {475, 715}, {350, 650}, {220, 565}, {200, 475}, {225, 410}
        }};
        return InPolygon(p, clearing);
    }
    if (world_.stage == Stage::ForestOutbound || world_.stage == Stage::ForestReturn) {
        if (p.x < 130.0f || p.x > 1540.0f || p.y < 310.0f || p.y > 850.0f) return false;
        if (!OnForestTrail(p)) return false;
        if (p.x > 1290.0f && p.y > 375.0f && p.y < 465.0f && !world_.fox.crouching) return false;
        return true;
    }
    if (p.x < 85.0f || p.x > 1570.0f || p.y < 278.0f || p.y > 850.0f) return false;
    if (!OnShrineGround(p)) return false;
    // The fox may duck under the fallen log on the right; the shrine and pedestal are solid.
    if (p.x > 690.0f && p.x < 940.0f && p.y < 365.0f) return false;
    if (Near(p, kPedestal, 34.0f)) return false;
    if (p.x > 1350.0f && p.y > 455.0f && p.y < 535.0f && !world_.fox.crouching) return false;
    return true;
}

bool Game::InCover(Point p) const {
    if (!world_.fox.crouching) return false;
    if (world_.stage == Stage::ForestReturn) {
        return (p.x > 300.0f && p.x < 500.0f && p.y > 470.0f && p.y < 700.0f) ||
               (p.x > 930.0f && p.x < 1150.0f && p.y > 390.0f && p.y < 650.0f) ||
               (p.x > 1280.0f && p.y > 375.0f && p.y < 465.0f);
    }
    return (p.x > 500.0f && p.x < 675.0f && p.y > 475.0f && p.y < 650.0f) ||
           (p.x > 1100.0f && p.x < 1290.0f && p.y > 420.0f && p.y < 570.0f) ||
           (p.x > 1340.0f && p.y > 455.0f && p.y < 535.0f);
}

bool Game::CanSeeFox() const {
    if (world_.stage == Stage::ForestReturn && !world_.guardianEnteredForest) return false;
    if (world_.guardian.form == GuardianForm::Stone || InCover(world_.fox.position)) return false;
    Point delta = Sub(world_.fox.position, world_.guardian.position);
    float range = world_.fox.running ? 390.0f : world_.fox.crouching ? 220.0f : 310.0f;
    if (world_.lantern.owner == LanternOwner::Fox) range += 90.0f;
    if (Length(delta) > range) return false;
    if (world_.stage == Stage::ForestReturn || world_.stage == Stage::Approach ||
        world_.stage == Stage::Courtyard || world_.stage == Stage::Escape) {
        if (CrossesBarrier(world_.guardian.position, world_.fox.position, LowPassage(world_.stage))) return false;
    }
    return Dot(Normalized(delta), Normalized(world_.guardian.facing)) > 0.12f;
}

void Game::UpdateFox(const Input& input, float dt) {
    world_.fox.moving = false;
    bool underLog = (world_.stage == Stage::ForestOutbound || world_.stage == Stage::ForestReturn ||
                     world_.stage == Stage::Approach || world_.stage == Stage::Courtyard ||
                     world_.stage == Stage::Escape) && Inside(world_.fox.position, LowPassage(world_.stage));
    if (input.toggleCrouch) {
        if (underLog && world_.fox.crouching) notice_ = "倒木の下では伏せたまま進もう";
        else {
            world_.fox.crouching = !world_.fox.crouching;
            if (world_.fox.crouching) world_.fox.running = false;
        }
    }
    if (input.toggleRun) {
        if (underLog) notice_ = "倒木の下では走れない";
        else {
            world_.fox.running = !world_.fox.running;
            if (world_.fox.running) world_.fox.crouching = false;
        }
    }
    if (world_.fox.sweepCooldown > 0.0f) world_.fox.sweepCooldown -= dt;
    if (input.sweep && world_.fox.sweepCooldown <= 0.0f) {
        bool swept = false;
        for (auto& step : world_.footprints) {
            if (!step.erased && Near(step.position, world_.fox.position, 94.0f)) {
                step.erased = true;
                swept = true;
            }
        }
        notice_ = swept ? "しっぽで足跡を払った" : "近くに払える足跡がない";
        if (swept) ++soundEvents_.sweep;
        world_.fox.sweepCooldown = 0.45f;
    }
    if (input.interact) Interact();
    if (input.ringBell && world_.stage != Stage::NestIntro && world_.stage != Stage::ForestOutbound &&
        world_.stage != Stage::ForestReturn) RingBell(false);

    Point direction = Normalized(input.move);
    if (Length(direction) == 0.0f) return;
    Point previous = world_.fox.position;
    float speed = world_.fox.crouching ? 105.0f : world_.fox.running ? 310.0f : 185.0f;
    if (world_.lantern.owner == LanternOwner::Fox) speed = std::min(speed, 235.0f);
    Point next = Add(world_.fox.position, Mul(direction, speed * dt));
    if (CanMoveTo(next)) {
        world_.fox.stepDistance += Distance(next, world_.fox.position);
        world_.fox.position = next;
    } else {
        Point xOnly{next.x, world_.fox.position.y};
        Point yOnly{world_.fox.position.x, next.y};
        if (CanMoveTo(xOnly)) {
            world_.fox.stepDistance += Distance(xOnly, world_.fox.position);
            world_.fox.position = xOnly;
        } else if (CanMoveTo(yOnly)) {
            world_.fox.stepDistance += Distance(yOnly, world_.fox.position);
            world_.fox.position = yOnly;
        }
    }
    Point displacement = Sub(world_.fox.position, previous);
    world_.fox.moving = Length(displacement) > 0.01f;
    if (world_.fox.moving) {
        Point facing = Normalized(displacement);
        world_.fox.facingX = facing.x;
        world_.fox.facingY = facing.y;
        world_.fox.walkDistance += Length(displacement);
    }
    if (world_.stage != Stage::NestIntro && world_.fox.stepDistance >= (world_.fox.running ? 27.0f : 40.0f)) {
        world_.fox.stepDistance = 0.0f;
        world_.footprints.push_back({world_.fox.position, world_.time});
        ++soundEvents_.step;
        if (world_.footprints.size() > 100) world_.footprints.erase(world_.footprints.begin());
    }
}

void Game::Interact() {
    if (world_.stage == Stage::NestIntro) {
        if (Near(world_.fox.position, {820.0f, 620.0f}, 140.0f)) {
            world_.nestTwigsTouched = true;
            notice_ = "小枝に火はつかない。山の神社の灯りを持って帰ろう";
        } else notice_ = "まず巣の前の小枝に近づこう";
        return;
    }
    if (world_.stage == Stage::ForestOutbound) {
        notice_ = "鳥居の先に灯籠が見える";
        return;
    }
    if (world_.lantern.owner == LanternOwner::Fox) {
        world_.lantern.owner = LanternOwner::Ground;
        world_.lantern.position = world_.fox.position;
        ++soundEvents_.lantern;
        notice_ = "灯籠を雪の上に置いた";
        return;
    }
    if ((world_.lantern.owner == LanternOwner::Pedestal || world_.lantern.owner == LanternOwner::Ground) &&
        Near(world_.fox.position, world_.lantern.position, 95.0f)) {
        if (world_.lantern.owner == LanternOwner::Pedestal && !checkpointC_) SaveCheckpointC();
        world_.lantern.owner = LanternOwner::Fox;
        ++soundEvents_.lantern;
        if (world_.stage != Stage::ForestReturn) {
            world_.stage = Stage::Escape;
            world_.stageTime = 0.0f;
            notice_ = "灯籠をくわえた。ふたつの出口から森へ戻ろう";
        } else notice_ = "灯籠をくわえた。巣へ戻ろう";
        return;
    }
    if (world_.stage != Stage::ForestReturn && Near(world_.fox.position, kBell, 100.0f)) {
        RingBell(false);
        return;
    }
    notice_ = "近くに触れられるものがない";
}

void Game::RingBell(bool byWind) {
    if (world_.stage == Stage::ForestReturn || world_.stage == Stage::ForestOutbound ||
        world_.stage == Stage::NestIntro) return;
    if (world_.bellCooldown > 0.0f) return;
    world_.bellCooldown = 2.0f;
    ++soundEvents_.bell;
    notice_ = byWind ? "風が鈴を鳴らした" : "鈴が鳴った";
    if (Near(world_.guardian.position, kBell, 860.0f)) RecordObservation(ObservationKind::BellHeard, kBell);
}

std::vector<Intent> Game::LegalIntents() const {
    std::vector<Intent> result{Intent::GuardLantern, Intent::WaitAndWatch};
    const auto& k = world_.knowledge;
    if (k.bellHeard && world_.stage != Stage::ForestReturn) result.push_back(Intent::InspectBell);
    if (k.tracksSeen && (world_.stage != Stage::ForestReturn || k.tracksSeenInForest))
        result.push_back(Intent::InspectTracks);
    if (k.hasLastSeen && (world_.stage != Stage::ForestReturn || k.lastSeenInForest))
        result.push_back(Intent::InspectLastSeen);
    if (k.foxVisible) result.push_back(Intent::FollowVisibleClue);
    if (k.theftKnown) result.push_back(Intent::WatchExit);
    return result;
}

void Game::QueueDecision() {
    pending_ = DecisionRequest{attemptId_, nextDecisionId_++, world_.knowledge.revision,
                               world_.knowledge, LegalIntents()};
    inFlight_.reset();
}

void Game::RecordObservation(ObservationKind kind, Point where) {
    auto& k = world_.knowledge;
    bool changed = false;
    switch (kind) {
        case ObservationKind::BellHeard:
            if (!k.bellHeard) { k.bellHeard = true; k.bellPosition = where; k.falseAlarm = false; changed = true; }
            break;
        case ObservationKind::TracksSeen:
            if (!k.tracksSeen || Distance(k.lastTracks, where) > 100.0f) {
                k.tracksSeen = true; k.lastTracks = where; changed = true;
            }
            if (world_.stage == Stage::ForestReturn && !k.tracksSeenInForest) {
                k.tracksSeenInForest = true; changed = true;
            }
            break;
        case ObservationKind::FoxSeen:
            if (!k.foxVisible || !k.hasLastSeen || Distance(k.lastSeen, where) > 95.0f) {
                k.foxVisible = true; k.hasLastSeen = true; k.lastSeen = where; changed = true;
            }
            if (world_.stage == Stage::ForestReturn && !k.lastSeenInForest) {
                k.lastSeenInForest = true; changed = true;
            }
            break;
        case ObservationKind::FoxLost:
            if (k.foxVisible) { k.foxVisible = false; changed = true; }
            break;
        case ObservationKind::EmptyPedestalSeen:
        case ObservationKind::LightSeen:
            if (!k.theftKnown) { k.theftKnown = true; changed = true; }
            if (kind == ObservationKind::LightSeen &&
                (!k.hasLastSeen || Distance(k.lastSeen, where) > 95.0f)) {
                k.hasLastSeen = true; k.lastSeen = where; changed = true;
            }
            if (kind == ObservationKind::LightSeen && world_.stage == Stage::ForestReturn &&
                !k.lastSeenInForest) { k.lastSeenInForest = true; changed = true; }
            break;
        case ObservationKind::InvestigationEmpty:
            if (!k.falseAlarm) { k.falseAlarm = true; changed = true; }
            break;
    }
    if (changed) { ++k.revision; QueueDecision(); }
}

std::optional<DecisionRequest> Game::TakePendingDecision() {
    if (!pending_) return std::nullopt;
    inFlight_ = pending_;
    pending_.reset();
    return inFlight_;
}

bool Game::ApplyDecision(const DecisionResponse& response) {
    if (!inFlight_ || response.attemptId != attemptId_ ||
        response.attemptId != inFlight_->attemptId || response.decisionId != inFlight_->decisionId ||
        response.revision != world_.knowledge.revision || response.revision != inFlight_->revision ||
        !Legal(*inFlight_, response.choice)) return false;
    SetIntent(response.choice);
    inFlight_.reset();
    return true;
}

void Game::ResolvePendingLocally() {
    auto request = TakePendingDecision();
    if (!request) return;
    ApplyDecision({request->attemptId, request->decisionId, request->revision, localPolicy_.Choose(*request)});
}

void Game::SetIntent(Intent intent) {
    world_.guardian.intent = intent;
    bool leavesPost = intent != Intent::GuardLantern && intent != Intent::WaitAndWatch;
    if (world_.guardian.form == GuardianForm::Stone && leavesPost) {
        world_.guardian.form = GuardianForm::Awakening;
        world_.guardian.transformTimer = 1.1f;
        ++soundEvents_.stone;
    }
    if (intent == Intent::FollowVisibleClue || intent == Intent::InspectLastSeen) world_.guardian.target = world_.knowledge.lastSeen;
    else if (intent == Intent::InspectTracks) world_.guardian.target = world_.knowledge.lastTracks;
    else if (intent == Intent::InspectBell) world_.guardian.target = world_.knowledge.bellPosition;
    else if (intent == Intent::WatchExit) world_.guardian.target = world_.stage == Stage::ForestReturn ?
        Point{450.0f, 610.0f} : Distance(world_.guardian.position, kToriiExit) <
        Distance(world_.guardian.position, kLogExit) ? kToriiExit : kLogExit;
    else if (intent == Intent::WaitAndWatch) world_.guardian.target = world_.guardian.position;
    else world_.guardian.target = world_.stage == Stage::ForestReturn ? Point{780.0f, 380.0f} : kPedestal;
}

void Game::UpdateGuardian(float dt) {
    auto& g = world_.guardian;
    if (world_.stage == Stage::ForestReturn && !world_.guardianEnteredForest) return;
    if (g.form == GuardianForm::Awakening) {
        g.transformTimer -= dt;
        if (g.transformTimer <= 0.0f) g.form = GuardianForm::Animal;
        return;
    }
    if (g.form != GuardianForm::Animal) return;
    if (g.warningTimer > 0.0f) g.warningTimer -= dt;
    if (g.intent == Intent::FollowVisibleClue && world_.knowledge.foxVisible) g.target = world_.knowledge.lastSeen;
    Point navigationTarget = GuardianWaypoint(g.position, g.target, LowPassage(world_.stage));
    Point delta = Sub(navigationTarget, g.position);
    if (Length(delta) > 15.0f) {
        Point direction = Normalized(delta);
        g.facing = direction;
        float speed = g.intent == Intent::FollowVisibleClue ? 205.0f : 120.0f;
        g.position = Add(g.position, Mul(direction, std::min(speed * dt, Length(delta))));
    } else if (g.intent == Intent::GuardLantern && world_.stage != Stage::ForestReturn &&
               world_.lantern.owner == LanternOwner::Pedestal) {
        g.form = GuardianForm::Stone;
    } else if (g.intent == Intent::InspectBell && !world_.knowledge.foxVisible &&
               !world_.knowledge.tracksSeen) {
        RecordObservation(ObservationKind::InvestigationEmpty, g.position);
        g.intent = Intent::GuardLantern;
        g.target = kPedestal;
    }
    if (world_.knowledge.foxVisible && Near(g.position, world_.fox.position, 64.0f) &&
        g.warningTimer <= 0.0f) Capture();
    if (world_.stage == Stage::ForestReturn && world_.lantern.owner == LanternOwner::Ground &&
        Near(g.position, world_.lantern.position, 66.0f)) {
        world_.lantern.owner = LanternOwner::Guardian;
        Capture();
    }
}

void Game::UpdatePerception() {
    auto& g = world_.guardian;
    if (world_.stage == Stage::ForestReturn && !world_.guardianEnteredForest) return;
    if (g.form == GuardianForm::Stone || g.form == GuardianForm::Awakening) return;
    if (CanSeeFox()) {
        if (!world_.knowledge.foxVisible) { g.warningTimer = 1.0f; notice_ = "狛犬の目がこちらを向いた"; }
        RecordObservation(ObservationKind::FoxSeen, world_.fox.position);
    } else RecordObservation(ObservationKind::FoxLost, world_.fox.position);

    for (auto& step : world_.footprints) {
        if (step.erased || step.seenByGuardian || world_.time - step.createdAt > 22.0f) continue;
        Point delta = Sub(step.position, g.position);
        if (Length(delta) < 200.0f && Dot(Normalized(delta), Normalized(g.facing)) > 0.05f) {
            step.seenByGuardian = true;
            RecordObservation(ObservationKind::TracksSeen, step.position);
        }
    }
    if (world_.lantern.owner != LanternOwner::Pedestal) {
        Point delta = Sub(kPedestal, g.position);
        if (world_.stage != Stage::ForestReturn && Length(delta) < 240.0f &&
            Dot(Normalized(delta), Normalized(g.facing)) > -0.15f)
            RecordObservation(ObservationKind::EmptyPedestalSeen, kPedestal);
        Point glow = world_.lantern.owner == LanternOwner::Fox ? world_.fox.position : world_.lantern.position;
        delta = Sub(glow, g.position);
        if (Length(delta) < 380.0f && Dot(Normalized(delta), Normalized(g.facing)) > -0.05f)
            RecordObservation(ObservationKind::LightSeen, glow);
    }
}

void Game::EnterShrine(bool throughLog) {
    world_.forestFootprints = world_.footprints;
    world_.footprints.clear();
    world_.stage = Stage::Approach;
    world_.stageTime = 0.0f;
    world_.fox.position = throughLog ? Point{1450.0f, 385.0f} : Point{345.0f, 745.0f};
    world_.fox.crouching = throughLog;
    world_.fox.running = false;
    world_.fox.stepDistance = 0.0f;
    world_.guardian = Guardian{};
    world_.lantern = Lantern{};
    world_.knowledge = Knowledge{};
    world_.bellCooldown = 0.0f;
    world_.windBellFired = false;
    pending_.reset();
    inFlight_.reset();
    checkpointB_ = world_;
    notice_ = "風の鈴と狛犬の動きを見て、灯籠へ進もう";
}

void Game::EnterForestReturn(bool throughLog) {
    world_.stage = Stage::ForestReturn;
    world_.stageTime = 0.0f;
    world_.footprints = world_.forestFootprints;
    world_.fox.position = throughLog ? Point{1400.0f, 415.0f} : Point{565.0f, 500.0f};
    world_.fox.crouching = throughLog;
    world_.fox.running = false;
    world_.fox.stepDistance = 0.0f;
    world_.guardianEnteredForest = false;
    world_.guardian.position = {780.0f, 380.0f};
    world_.guardian.target = world_.guardian.position;
    world_.guardian.form = GuardianForm::Stone;
    world_.knowledge.foxVisible = false;
    world_.knowledge.lastSeenInForest = false;
    world_.knowledge.tracksSeenInForest = false;
    pending_.reset();
    inFlight_.reset();
    notice_ = "灯籠の光を抱えて、森を通り巣へ戻ろう";
}

void Game::SaveCheckpointC() {
    if (checkpointC_) return;
    checkpointC_ = world_;
    checkpointC_->stage = Stage::Courtyard;
    notice_ = "境内の手前を記憶した";
}

void Game::Capture() {
    if (world_.stage == Stage::Captured || world_.stage == Stage::SliceComplete ||
        world_.stage == Stage::NestEnding || world_.stage == Stage::Complete) return;
    captureCheckpointC_ = checkpointC_.has_value() && world_.lantern.owner != LanternOwner::Pedestal;
    world_.capturedScene = world_.stage;
    world_.stage = Stage::Captured;
    ++soundEvents_.capture;
    world_.fox.crouching = false;
    world_.fox.running = false;
    pending_.reset();
    inFlight_.reset();
    notice_ = "狛犬に見つかった。Rでチェックポイントからやり直す";
}

void Game::RestoreCheckpoint() {
    bool fromC = captureCheckpointC_ && checkpointC_.has_value();
    world_ = fromC ? *checkpointC_ : checkpointB_;
    if (!fromC) checkpointC_.reset();
    ++attemptId_;
    pending_.reset();
    inFlight_.reset();
    captureCheckpointC_ = false;
    world_.fox.crouching = false;
    world_.fox.running = false;
    notice_ = fromC ? "境内の手前から再開" : "鳥居の手前から再開";
}

void Game::Update(const Input& input, float dt) {
    if (input.restart && (world_.stage == Stage::Captured || world_.stage == Stage::SliceComplete ||
                          world_.stage == Stage::Complete)) {
        if (world_.stage == Stage::Complete) {
            bool external = externalDecisions_;
            std::uint64_t newAttempt = attemptId_ + 1;
            std::uint64_t newDecision = nextDecisionId_;
            SoundEvents previousSounds = soundEvents_;
            *this = Game(practiceMode_ ? StartMode::ShrinePractice : StartMode::FullStory);
            externalDecisions_ = external;
            attemptId_ = newAttempt;
            nextDecisionId_ = newDecision;
            soundEvents_ = previousSounds;
            return;
        }
        RestoreCheckpoint();
        return;
    }
    if (world_.stage == Stage::Captured || world_.stage == Stage::SliceComplete ||
        world_.stage == Stage::Complete) return;
    dt = std::clamp(dt, 0.0f, 1.0f / 20.0f);
    world_.time += dt;
    world_.stageTime += dt;
    world_.bellCooldown = std::max(0.0f, world_.bellCooldown - dt);
    if (world_.stage == Stage::NestEnding) {
        if (world_.stageTime - dt < 7.0f && world_.stageTime >= 7.0f) ++soundEvents_.fire;
        if (world_.stageTime > 15.0f) {
            world_.stage = Stage::Complete;
            notice_ = "小枝に火が灯り、灯籠は神社へ戻った";
        }
        return;
    }
    if (world_.stage == Stage::NestIntro) {
        UpdateFox(input, dt);
        if (world_.nestTwigsTouched && Near(world_.fox.position, {280.0f, 430.0f}, 115.0f)) {
            world_.stage = Stage::ForestOutbound;
            world_.stageTime = 0.0f;
            world_.fox.position = {345.0f, 760.0f};
            world_.fox.crouching = false;
            world_.fox.running = false;
            notice_ = "雪には足跡が残る。神社の灯りを目指そう";
        }
        return;
    }
    if (world_.stage == Stage::ForestOutbound) {
        UpdateFox(input, dt);
        if (Near(world_.fox.position, {530.0f, 440.0f}, 105.0f)) EnterShrine(false);
        else if (world_.fox.crouching && Near(world_.fox.position, {1420.0f, 385.0f}, 105.0f))
            EnterShrine(true);
        return;
    }
    if ((world_.stage == Stage::Approach || world_.stage == Stage::Courtyard || world_.stage == Stage::Escape) &&
        !world_.windBellFired && world_.stageTime > 2.5f) {
        world_.windBellFired = true;
        RingBell(true);
    }
    UpdateFox(input, dt);
    if (world_.stage == Stage::ForestReturn && !world_.guardianEnteredForest && world_.stageTime > 3.0f) {
        world_.guardianEnteredForest = true;
        RecordObservation(ObservationKind::EmptyPedestalSeen, kPedestal);
    }
    if (world_.stage != Stage::ForestReturn && !checkpointC_ &&
        world_.fox.position.x > 530.0f && world_.fox.position.y < 620.0f)
        SaveCheckpointC();
    UpdatePerception();
    if (!externalDecisions_) ResolvePendingLocally();
    UpdateGuardian(dt);
    UpdatePerception();
    if (!externalDecisions_) ResolvePendingLocally();
    if (world_.stage == Stage::Captured) return;
    if (world_.stage == Stage::ForestReturn && world_.lantern.owner == LanternOwner::Fox &&
        Near(world_.fox.position, {270.0f, 730.0f}, 115.0f)) {
        world_.stage = Stage::NestEnding;
        world_.stageTime = 0.0f;
        world_.fox.position = {690.0f, 650.0f};
        world_.guardian.position = {450.0f, 520.0f};
        world_.guardian.form = GuardianForm::Animal;
        world_.guardian.target = world_.guardian.position;
        world_.lantern.position = {820.0f, 620.0f};
        notice_ = "弟妹のいる巣に灯籠が届いた";
        return;
    }
    if (world_.stage != Stage::ForestReturn && world_.lantern.owner == LanternOwner::Fox &&
        (Near(world_.fox.position, kToriiExit, 95.0f) ||
         (world_.fox.crouching && Near(world_.fox.position, kLogExit, 110.0f)))) {
        if (practiceMode_) {
            world_.stage = Stage::SliceComplete;
            notice_ = "神社を抜けた。灯籠を巣へ運ぶ旅が続く";
        } else EnterForestReturn(Near(world_.fox.position, kLogExit, 110.0f));
    }
    if (world_.stage == Stage::Approach && world_.fox.position.x > 530.0f) world_.stage = Stage::Courtyard;
}

} // namespace foxgame
