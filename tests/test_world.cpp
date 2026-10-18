#include "World.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace foxgame;

// Keep checks active in CMake Release builds, where the standard assert is disabled.
#define assert(expression) do { if (!(expression)) { \
    throw std::runtime_error(std::string("line ") + std::to_string(__LINE__) + ": " #expression); \
} } while (false)

void WalkTo(Game& game, Point target, int frames, bool crouch = false, bool run = false) {
    Input input{};
    input.toggleCrouch = game.State().fox.crouching != crouch;
    input.toggleRun = !crouch && game.State().fox.running != run;
    Stage original = game.State().stage;
    for (int frame = 0; frame < frames; ++frame) {
        Point from = game.State().fox.position;
        float distance = Distance(from, target);
        if (distance < 12.0f) break;
        input.move = {(target.x - from.x) / distance, (target.y - from.y) / distance};
        game.Update(input, 1.0f / 60.0f);
        input.toggleCrouch = input.toggleRun = false;
        if (game.State().stage == Stage::Captured) {
            throw std::runtime_error("captured toward (" + std::to_string(target.x) + ", " +
                                     std::to_string(target.y) + ")");
        }
        if (game.State().stage != original) return;
    }
    if (Distance(game.State().fox.position, target) >= 15.0f) {
        Point current = game.State().fox.position;
        throw std::runtime_error("blocked at (" + std::to_string(current.x) + ", " +
                                 std::to_string(current.y) + ") toward (" +
                                 std::to_string(target.x) + ", " + std::to_string(target.y) + ")");
    }
}

void ShrineToLantern(Game& game) {
    WalkTo(game, {400, 665}, 140);
    WalkTo(game, {520, 595}, 140);
    WalkTo(game, {650, 550}, 140);
    WalkTo(game, {715, 535}, 80);
    WalkTo(game, {760, 505}, 80);
}

void LanternToLog(Game& game) {
    WalkTo(game, {650, 550}, 140, false, true);
    WalkTo(game, {850, 555}, 180, false, true);
    WalkTo(game, {1030, 520}, 180, false, true);
    WalkTo(game, {1230, 470}, 180, false, true);
    WalkTo(game, {1340, 430}, 140, true);
    WalkTo(game, {1480, 345}, 180, true);
}

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
    game.SetExternalDecisionMode(true);
    ShrineToLantern(game);
    Input take{};
    take.interact = true;
    game.Update(take, 1.0f / 60.0f);
    assert(game.State().lantern.owner == LanternOwner::Fox);
    LanternToLog(game);
    assert(game.State().stage == Stage::SliceComplete);
}

void FullStoryReachesEnding() {
    Game game;
    assert(game.State().stage == Stage::NestIntro);
    Input touch{};
    touch.interact = true;
    game.Update(touch, 1.0f / 60.0f);
    assert(game.State().nestTwigsTouched);

    WalkTo(game, {280, 430}, 300, false, true);
    assert(game.State().stage == Stage::ForestOutbound);
    WalkTo(game, {530, 440}, 300);
    assert(game.State().stage == Stage::Approach);
    game.SetExternalDecisionMode(true);
    ShrineToLantern(game);
    Input take{};
    take.interact = true;
    game.Update(take, 1.0f / 60.0f);
    assert(game.State().lantern.owner == LanternOwner::Fox);
    LanternToLog(game);
    assert(game.State().stage == Stage::ForestReturn);
    WalkTo(game, {1240, 470}, 180, true);
    for (int frame = 0; frame < 240 && !game.State().guardianEnteredForest; ++frame)
        game.Update({}, 1.0f / 60.0f);
    assert(game.State().guardianEnteredForest);
    assert(game.State().knowledge.lightVisible);
    WalkTo(game, {1030, 515}, 180);
    WalkTo(game, {760, 555}, 180, false, true);
    WalkTo(game, {445, 555}, 180, false, true);
    WalkTo(game, {330, 665}, 100, false, true);
    assert(game.State().stage == Stage::NestEnding);
    assert(!game.State().endingLanternPlaced);
    assert(game.State().lantern.owner == LanternOwner::Fox);
    assert(Distance(game.State().fox.position, {280, 430}) < 1.0f);
    for (int frame = 0; frame < 960; ++frame) game.Update({}, 1.0f / 60.0f);
    assert(game.State().stage == Stage::NestEnding);
    assert(game.Sounds().fire == 0);
    game.Update(take, 1.0f / 60.0f);
    assert(!game.State().endingLanternPlaced);
    WalkTo(game, {385, 475}, 100);
    WalkTo(game, {545, 590}, 160);
    WalkTo(game, {690, 650}, 130);
    WalkTo(game, {790, 620}, 100);
    game.Update(take, 1.0f / 60.0f);
    assert(game.State().endingLanternPlaced);
    assert(game.State().lantern.owner == LanternOwner::Ground);
    for (int frame = 0; frame < 960; ++frame) game.Update({}, 1.0f / 60.0f);
    assert(game.State().stage == Stage::Complete);
    assert(game.Sounds().fire == 1);
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
    move.move = {1, 0};
    for (int i = 0; i < 180; ++i) shrine.Update(move, 1.0f / 60.0f);
    assert(shrine.State().fox.position.x < 470.0f); // The lower snowfield is outside the stone road.

    Game shrineBank(StartMode::ShrinePractice);
    shrineBank.SetExternalDecisionMode(true);
    move.move = {0, -1};
    for (int i = 0; i < 180; ++i) shrineBank.Update(move, 1.0f / 60.0f);
    assert(shrineBank.State().fox.position.y > 420.0f); // The torii's wooded bank is solid.
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
    game.SetExternalDecisionMode(true);
    ShrineToLantern(game);
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
        assert(!(p.x > 1350.0f && p.x < 1570.0f && p.y > 335.0f && p.y < 425.0f));
    }
    assert(Distance(game.State().guardian.position, clue) < 20.0f);
}

void GuardianFollowsCarriedLightOnRightRoad() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    ShrineToLantern(game);
    Input take{};
    take.interact = true;
    game.Update(take, 1.0f / 60.0f);
    assert(game.State().lantern.owner == LanternOwner::Fox);
    WalkTo(game, {650, 550}, 140);
    WalkTo(game, {850, 555}, 180);
    WalkTo(game, {1030, 520}, 180);
    WalkTo(game, {1230, 470}, 180, false, true);
    assert(game.State().guardian.form == GuardianForm::Stone);
    assert(!game.State().knowledge.foxVisible);
    assert(game.State().knowledge.lightVisible);
    assert(Distance(game.State().knowledge.lastSeen, game.State().fox.position) < 1.0f);
    auto request = game.TakePendingDecision();
    assert(request);
    assert(std::find(request->legalIntents.begin(), request->legalIntents.end(),
                     Intent::FollowVisibleClue) != request->legalIntents.end());
    assert(game.ApplyDecision({request->attemptId, request->decisionId, request->revision,
                               Intent::FollowVisibleClue}));
    for (int frame = 0; frame < 100; ++frame) game.Update({}, 1.0f / 60.0f);
    assert(game.State().guardian.form == GuardianForm::Animal);
    assert(game.State().guardian.position.x > 1030.0f);
    Input hide{};
    hide.toggleCrouch = true;
    game.Update(hide, 1.0f / 60.0f);
    assert(!game.State().knowledge.lightVisible);
    auto lost = game.TakePendingDecision();
    assert(lost);
    assert(std::find(lost->legalIntents.begin(), lost->legalIntents.end(),
                     Intent::FollowVisibleClue) == lost->legalIntents.end());
}

void CarriedLanternCanHideInBush() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    ShrineToLantern(game);
    WalkTo(game, {715, 500}, 80);
    Input take{};
    take.interact = true;
    game.Update(take, 1.0f / 60.0f);
    assert(game.State().lantern.owner == LanternOwner::Fox);
    assert(game.State().knowledge.lightVisible);
    auto sighting = game.TakePendingDecision();
    assert(sighting);
    assert(game.ApplyDecision({sighting->attemptId, sighting->decisionId, sighting->revision,
                               Intent::FollowVisibleClue}));
    assert(game.State().guardian.form == GuardianForm::Awakening);
    WalkTo(game, {650, 520}, 80, true);
    assert(game.FoxHidden());
    assert(!game.State().knowledge.lightVisible);
    assert(!game.State().knowledge.foxVisible);
    auto hidden = game.TakePendingDecision();
    assert(hidden);
    assert(std::find(hidden->legalIntents.begin(), hidden->legalIntents.end(),
                     Intent::FollowVisibleClue) == hidden->legalIntents.end());
    assert(game.Notice().find("隠れた") != std::string::npos);
    for (int frame = 0; frame < 240; ++frame) game.Update({}, 1.0f / 60.0f);
    assert(game.State().stage != Stage::Captured);
}

void LocalGuardianStartsChasingVisibleLight() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    ShrineToLantern(game);
    game.SetExternalDecisionMode(false);
    Input take{};
    take.interact = true;
    game.Update(take, 1.0f / 60.0f);
    assert(game.State().lantern.owner == LanternOwner::Fox);
    assert(game.State().knowledge.lightVisible);
    assert(game.State().guardian.intent == Intent::FollowVisibleClue);
    assert(game.State().guardian.form == GuardianForm::Awakening);
}

void LocalGuardianReturnsAfterInspectingTracks() {
    Game game(StartMode::ShrinePractice);
    Point post = game.State().guardian.post;
    game.RecordObservation(ObservationKind::TracksSeen, {960, 470});
    for (int frame = 0; frame < 180; ++frame) {
        game.Update({}, 1.0f / 60.0f);
        if (game.State().knowledge.tracksSeen && !game.State().knowledge.tracksPending &&
            game.State().guardian.form == GuardianForm::Stone) break;
    }
    assert(game.State().knowledge.tracksSeen);
    assert(!game.State().knowledge.tracksPending);
    assert(game.State().guardian.form == GuardianForm::Stone);
    assert(Distance(game.State().guardian.position, post) < 0.01f);
}

void TheftTriggersFiniteExitWatch() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    Point post = game.State().guardian.post;
    game.RecordObservation(ObservationKind::EmptyPedestalSeen, post);
    auto request = game.TakePendingDecision();
    assert(request);
    assert(std::find(request->legalIntents.begin(), request->legalIntents.end(),
                     Intent::GuardLantern) == request->legalIntents.end());
    assert(std::find(request->legalIntents.begin(), request->legalIntents.end(),
                     Intent::WatchLogExit) != request->legalIntents.end());
    assert(game.ApplyDecision({request->attemptId, request->decisionId, request->revision,
                               Intent::WatchLogExit}));
    bool reachedExit = false;
    for (int frame = 0; frame < 600 && !game.State().knowledge.watchCompleted; ++frame) {
        game.Update({}, 1.0f / 60.0f);
        reachedExit |= Distance(game.State().guardian.position, {1308.0f, 345.0f}) < 15.0f;
    }
    assert(reachedExit);
    assert(game.State().knowledge.watchCompleted);
    auto finished = game.TakePendingDecision();
    assert(finished);
    assert(std::find(finished->legalIntents.begin(), finished->legalIntents.end(),
                     Intent::WatchLogExit) == finished->legalIntents.end());
    assert(game.ApplyDecision({finished->attemptId, finished->decisionId, finished->revision,
                               Intent::GuardLantern}));
    for (int frame = 0; frame < 360; ++frame) game.Update({}, 1.0f / 60.0f);
    assert(game.State().guardian.intent == Intent::GuardLantern);
    assert(Distance(game.State().guardian.position, post) < 0.01f);
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
    GuardianFollowsCarriedLightOnRightRoad();
    CarriedLanternCanHideInBush();
    LocalGuardianStartsChasingVisibleLight();
    LocalGuardianReturnsAfterInspectingTracks();
    TheftTriggersFiniteExitWatch();
    std::cout << "world invariants: pass\n";
}
