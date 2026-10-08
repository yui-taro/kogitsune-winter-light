#include "World.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace foxgame;

// Keep checks active in CMake Release builds, where the standard assert is disabled.
#define assert(expression) do { if (!(expression)) { \
    throw std::runtime_error(std::string("line ") + std::to_string(__LINE__) + ": " #expression); \
} } while (false)

void ObservationBoundary() {
    Game game(StartMode::ShrinePractice);
    game.RecordObservation(ObservationKind::BellHeard, {350.0f, 500.0f});
    auto request = game.TakePendingDecision();
    assert(request);
    assert(request->knowledge.bellHeard);
    assert(!request->knowledge.foxVisible);
    assert(!request->knowledge.theftKnown);
    assert(!request->knowledge.hasLastSeen);
    for (Intent intent : request->legalIntents) {
        assert(intent != Intent::FollowVisibleClue);
        assert(intent != Intent::InspectLastSeen);
        assert(intent != Intent::WatchExit);
    }
    assert(!game.ApplyDecision({request->attemptId, request->decisionId, request->revision,
                                Intent::FollowVisibleClue}));
    assert(game.ApplyDecision({request->attemptId, request->decisionId, request->revision,
                               Intent::InspectBell}));
    assert(game.State().guardian.intent == Intent::InspectBell);
}

void DuplicateEventsDoNotReplan() {
    Game game(StartMode::ShrinePractice);
    game.RecordObservation(ObservationKind::BellHeard, {350.0f, 500.0f});
    auto first = game.TakePendingDecision();
    assert(first);
    game.RecordObservation(ObservationKind::BellHeard, {350.0f, 500.0f});
    assert(!game.TakePendingDecision());
    assert(game.State().knowledge.revision == first->revision);
    assert(game.ApplyDecision({first->attemptId, first->decisionId, first->revision,
                               Intent::InspectBell}));
}

void StaleResponsesAndCheckpointMemory() {
    Game game(StartMode::ShrinePractice);
    game.RecordObservation(ObservationKind::BellHeard, {350.0f, 500.0f});
    game.SaveCheckpointC();
    game.RecordObservation(ObservationKind::FoxSeen, {800.0f, 590.0f});
    auto old = game.TakePendingDecision();
    assert(old);
    game.Capture();
    game.RestoreCheckpoint();
    assert(!game.State().knowledge.bellHeard);
    assert(!game.State().knowledge.foxVisible);
    assert(!game.State().knowledge.hasLastSeen);
    assert(game.State().knowledge.revision == 0);
    assert(game.AttemptId() != old->attemptId);
    assert(!game.ApplyDecision({old->attemptId, old->decisionId, old->revision,
                                Intent::FollowVisibleClue}));
    game.RecordObservation(ObservationKind::TracksSeen, {560.0f, 570.0f});
    auto fresh = game.TakePendingDecision();
    assert(fresh);
    assert(!fresh->knowledge.bellHeard);
    assert(fresh->knowledge.tracksSeen);
    assert(!fresh->knowledge.foxVisible);
}

void NewObservationSupersedesInFlight() {
    Game game(StartMode::ShrinePractice);
    game.RecordObservation(ObservationKind::BellHeard, {350.0f, 500.0f});
    auto old = game.TakePendingDecision();
    assert(old);
    game.RecordObservation(ObservationKind::TracksSeen, {500.0f, 600.0f});
    assert(!game.ApplyDecision({old->attemptId, old->decisionId, old->revision,
                                Intent::InspectBell}));
    auto fresh = game.TakePendingDecision();
    assert(fresh && fresh->revision > old->revision);
    LocalPersonalityPolicy policy;
    assert(policy.Choose(*fresh) == Intent::InspectTracks);
}

void PlayableRightRoute() {
    Game game(StartMode::ShrinePractice);
    auto moveFor = [&](Point direction, float seconds, bool crouch, bool run) {
        Input input{};
        input.move = direction;
        input.toggleCrouch = crouch;
        input.toggleRun = run;
        for (int frame = 0; frame < static_cast<int>(seconds * 60.0f); ++frame) {
            game.Update(input, 1.0f / 60.0f);
            input.toggleCrouch = input.toggleRun = false;
            assert(game.State().stage != Stage::Captured);
        }
    };
    moveFor({1, 0}, 3.55f, false, true);
    moveFor({0, -1}, 3.75f, true, false);
    moveFor({-1, 0}, 1.65f, false, true);
    moveFor({-1, 1}, 0.45f, false, false);
    Input take{};
    take.interact = true;
    game.Update(take, 1.0f / 60.0f);
    assert(game.State().lantern.owner == LanternOwner::Fox);
    moveFor({1, 0}, 2.4f, false, false);
    moveFor({0, -1}, 0.8f, true, false);
    assert(game.State().stage == Stage::SliceComplete);
}

void FullStoryReachesEnding() {
    Game game;
    assert(game.State().stage == Stage::NestIntro);
    Input touch{};
    touch.interact = true;
    game.Update(touch, 1.0f / 60.0f);
    assert(game.State().nestTwigsTouched);

    auto toward = [&](Point target, int frames, bool crouch = false, bool run = false) {
        Input input{};
        input.toggleCrouch = crouch;
        input.toggleRun = run;
        Stage original = game.State().stage;
        for (int frame = 0; frame < frames; ++frame) {
            Point from = game.State().fox.position;
            float distance = Distance(from, target);
            if (distance < 15.0f) break;
            input.move = {(target.x - from.x) / distance, (target.y - from.y) / distance};
            game.Update(input, 1.0f / 60.0f);
            input.toggleCrouch = input.toggleRun = false;
            assert(game.State().stage != Stage::Captured);
            if (game.State().stage != original) break;
        }
    };

    toward({280, 430}, 300, false, true);
    assert(game.State().stage == Stage::ForestOutbound);
    toward({530, 440}, 300);
    assert(game.State().stage == Stage::Approach);
    toward({1450, 745}, 360, false, true);
    toward({1450, 345}, 300, true);
    toward({940, 345}, 180, false, true);
    toward({850, 450}, 60);
    Input take{};
    take.interact = true;
    game.Update(take, 1.0f / 60.0f);
    assert(game.State().lantern.owner == LanternOwner::Fox);
    toward({1400, 450}, 210);
    toward({1480, 345}, 120, true);
    assert(game.State().stage == Stage::ForestReturn);
    toward({1240, 470}, 180);
    toward({270, 730}, 540, false, true);
    assert(game.State().stage == Stage::NestEnding);
    for (int frame = 0; frame < 960; ++frame) game.Update({}, 1.0f / 60.0f);
    assert(game.State().stage == Stage::Complete);
}

void OffTrailMovementIsBlocked() {
    Game game;
    Input move{};
    move.move = {0, -1};
    for (int i = 0; i < 180; ++i) game.Update(move, 1.0f / 60.0f);
    assert(game.State().stage == Stage::NestIntro);
    assert(game.State().fox.position.y > 560.0f); // The nest bank is above the clearing.

    Game forest;
    Input touch{};
    touch.interact = true;
    forest.Update(touch, 1.0f / 60.0f);
    for (int i = 0; i < 260 && forest.State().stage == Stage::NestIntro; ++i) {
        Point p = forest.State().fox.position;
        float dx = 280.0f - p.x, dy = 430.0f - p.y;
        float length = std::sqrt(dx * dx + dy * dy);
        move.move = {dx / length, dy / length};
        forest.Update(move, 1.0f / 60.0f);
    }
    assert(forest.State().stage == Stage::ForestOutbound);
    move.move = {1, 0};
    for (int i = 0; i < 180; ++i) forest.Update(move, 1.0f / 60.0f);
    assert(forest.State().fox.position.x < 500.0f); // Snowy brush is not a shortcut.

    Game shrine(StartMode::ShrinePractice);
    shrine.SetExternalDecisionMode(true);
    move.move = {0, -1};
    for (int i = 0; i < 180; ++i) shrine.Update(move, 1.0f / 60.0f);
    assert(shrine.State().fox.position.y > 420.0f); // The torii's wooded bank is solid.
}

void FoxAnimationFollowsActualMovement() {
    Game game;
    Input input{};
    input.move = {-1, 0};
    game.Update(input, 1.0f / 60.0f);
    assert(game.State().fox.moving);
    assert(game.State().fox.facingX < -0.9f);
    float walked = game.State().fox.walkDistance;
    assert(walked > 0.0f);

    game.Update({}, 1.0f / 60.0f);
    assert(!game.State().fox.moving);
    assert(game.State().fox.walkDistance == walked);

    input.move = {0, 1};
    game.Update(input, 1.0f / 60.0f);
    assert(game.State().fox.moving);
    assert(std::abs(game.State().fox.facingX) < 0.1f);
    assert(game.State().fox.facingY > 0.9f);
}

void TheftRetryRestoresPreTheftWorld() {
    Game game(StartMode::ShrinePractice);
    auto moveFor = [&](Point direction, float seconds, bool crouch = false, bool run = false) {
        Input input{};
        input.move = direction;
        input.toggleCrouch = crouch;
        input.toggleRun = run;
        for (int frame = 0; frame < static_cast<int>(seconds * 60.0f); ++frame) {
            game.Update(input, 1.0f / 60.0f);
            input.toggleCrouch = input.toggleRun = false;
        }
    };
    moveFor({1, 0}, 3.55f, false, true);
    moveFor({0, -1}, 3.75f, true);
    moveFor({-1, 0}, 1.65f, false, true);
    moveFor({-1, 1}, 0.45f);
    assert(game.HasCheckpointC());
    Input take{};
    take.interact = true;
    game.Update(take, 1.0f / 60.0f);
    assert(game.State().lantern.owner == LanternOwner::Fox);
    game.RecordObservation(ObservationKind::FoxSeen, game.State().fox.position);
    std::uint64_t oldAttempt = game.AttemptId();
    game.Capture();
    game.RestoreCheckpoint();
    assert(game.AttemptId() > oldAttempt);
    assert(game.State().stage == Stage::Courtyard);
    assert(game.State().lantern.owner == LanternOwner::Pedestal);
    assert(!game.State().knowledge.foxVisible);
}

void GuardianWalksAroundLowPassage() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    Point clue{1530.0f, 550.0f};
    game.RecordObservation(ObservationKind::FoxSeen, clue);
    auto request = game.TakePendingDecision();
    assert(request);
    assert(game.ApplyDecision({request->attemptId, request->decisionId, request->revision,
                               Intent::FollowVisibleClue}));
    for (int frame = 0; frame < 800; ++frame) {
        game.Update({}, 1.0f / 60.0f);
        Point p = game.State().guardian.position;
        assert(!(p.x > 1350.0f && p.x < 1570.0f && p.y > 455.0f && p.y < 535.0f));
    }
    assert(Distance(game.State().guardian.position, clue) < 20.0f);
}

int main() {
    ObservationBoundary();
    DuplicateEventsDoNotReplan();
    StaleResponsesAndCheckpointMemory();
    NewObservationSupersedesInFlight();
    PlayableRightRoute();
    FullStoryReachesEnding();
    OffTrailMovementIsBlocked();
    FoxAnimationFollowsActualMovement();
    TheftRetryRestoresPreTheftWorld();
    GuardianWalksAroundLowPassage();
    std::cout << "world invariants: pass\n";
}
