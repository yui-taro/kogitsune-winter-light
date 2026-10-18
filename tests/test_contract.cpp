#include "World.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace foxgame;

namespace {

void Check(bool condition, const char* expression, int line) {
    if (!condition) throw std::runtime_error(std::string("line ") + std::to_string(line) + ": " + expression);
}
#define CHECK(expression) Check(static_cast<bool>(expression), #expression, __LINE__)

bool Has(const DecisionRequest& request, Intent intent) {
    return std::find(request.legalIntents.begin(), request.legalIntents.end(), intent) != request.legalIntents.end();
}

DecisionResponse Reply(const DecisionRequest& request, Intent choice) {
    return {request.attemptId, request.decisionId, request.revision, choice};
}

void Step(Game& game, Point direction, int frames, bool crouch = false, bool run = false) {
    Input input{};
    input.move = direction;
    input.toggleCrouch = crouch;
    input.toggleRun = run;
    for (int frame = 0; frame < frames; ++frame) {
        game.Update(input, 1.0f / 60.0f);
        input.toggleCrouch = input.toggleRun = false;
        CHECK(game.State().stage != Stage::Captured);
    }
}

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
        CHECK(game.State().stage != Stage::Captured);
        if (game.State().stage != original) return;
    }
    CHECK(Distance(game.State().fox.position, target) < 15.0f);
}

void ReachLantern(Game& game) {
    WalkTo(game, {400, 665}, 140);
    WalkTo(game, {520, 595}, 140);
    WalkTo(game, {650, 550}, 140);
    WalkTo(game, {715, 535}, 80);
    WalkTo(game, {760, 505}, 80);
    CHECK(Distance(game.State().fox.position, game.State().lantern.position) <= 95.0f);
}

void ReachLogExit(Game& game) {
    WalkTo(game, {650, 550}, 140);
    WalkTo(game, {850, 555}, 180);
    WalkTo(game, {1030, 520}, 180);
    WalkTo(game, {1230, 470}, 180);
    WalkTo(game, {1340, 430}, 140, true);
    WalkTo(game, {1480, 345}, 180, true);
}

void DecisionFixtures() {
    struct Scenario {
        const char* name;
        std::vector<std::pair<ObservationKind, Point>> observations;
        Intent expected;
        Intent forbidden;
    };
    const std::vector<Scenario> scenarios{
        {"J-01 bell only", {{ObservationKind::BellHeard, {350, 500}}},
         Intent::InspectBell, Intent::FollowVisibleClue},
        {"J-02 tracks only", {{ObservationKind::TracksSeen, {550, 550}}},
         Intent::InspectTracks, Intent::WatchExit},
        {"tracks investigated", {{ObservationKind::TracksSeen, {550, 550}},
                                  {ObservationKind::TracksInvestigated, {550, 550}}},
         Intent::GuardLantern, Intent::InspectTracks},
        {"J-03 conflicting clues", {{ObservationKind::BellHeard, {350, 500}},
                                     {ObservationKind::TracksSeen, {550, 550}}},
         Intent::InspectTracks, Intent::FollowVisibleClue},
        {"J-04 bell investigated empty", {{ObservationKind::BellHeard, {350, 500}},
                                           {ObservationKind::TracksSeen, {550, 550}},
                                           {ObservationKind::InvestigationEmpty, {350, 500}}},
         Intent::InspectTracks, Intent::FollowVisibleClue},
        {"J-05 theft witnessed", {{ObservationKind::EmptyPedestalSeen, {780, 445}}},
         Intent::WatchLogExit, Intent::FollowVisibleClue},
        {"visible fox", {{ObservationKind::FoxSeen, {700, 500}}},
         Intent::FollowVisibleClue, Intent::WatchExit},
        {"visible carried light", {{ObservationKind::LightSeen, {1220, 470}}},
         Intent::FollowVisibleClue, Intent::InspectTracks},
        {"fox lost", {{ObservationKind::FoxSeen, {700, 500}},
                       {ObservationKind::FoxLost, {700, 500}}},
         Intent::GuardLantern, Intent::FollowVisibleClue},
    };
    LocalPersonalityPolicy policy;
    for (const auto& scenario : scenarios) {
        Game game(StartMode::ShrinePractice);
        for (const auto& observation : scenario.observations)
            game.RecordObservation(observation.first, observation.second);
        auto request = game.TakePendingDecision();
        CHECK(request);
        CHECK(Has(*request, Intent::WaitAndWatch) ==
              (!request->knowledge.theftKnown && !request->knowledge.foxVisible &&
               !request->knowledge.lightVisible));
        CHECK(!Has(*request, scenario.forbidden));
        CHECK(Has(*request, scenario.expected));
        CHECK(policy.Choose(*request) == scenario.expected);
        CHECK(request->knowledge.revision == scenario.observations.size());
        (void)scenario.name;
    }
    Game exitGame(StartMode::ShrinePractice);
    exitGame.RecordObservation(ObservationKind::EmptyPedestalSeen, {780, 445});
    auto exitRequest = exitGame.TakePendingDecision();
    CHECK(exitRequest && Has(*exitRequest, Intent::WatchToriiExit) &&
          Has(*exitRequest, Intent::WatchLogExit));
    exitRequest->guardianPosition = {200, 735};
    CHECK(policy.Choose(*exitRequest) == Intent::WatchToriiExit);
}

void ObservationRevisionAndMemory() {
    Game game(StartMode::ShrinePractice);
    game.RecordObservation(ObservationKind::BellHeard, {350, 500});
    auto first = game.TakePendingDecision();
    CHECK(first);
    game.RecordObservation(ObservationKind::BellHeard, {350, 500});
    game.RecordObservation(ObservationKind::TracksSeen, {540, 550});
    game.RecordObservation(ObservationKind::TracksSeen, {600, 550});
    CHECK(!game.ApplyDecision(Reply(*first, Intent::InspectBell)));
    auto second = game.TakePendingDecision();
    CHECK(second && second->revision == 2);
    CHECK(second->decisionId > first->decisionId);
    CHECK(second->knowledge.bellHeard && second->knowledge.tracksSeen);
    CHECK(Distance(second->knowledge.lastTracks, {540, 550}) < 0.01f);
    game.RecordObservation(ObservationKind::InvestigationEmpty, {350, 500});
    auto third = game.TakePendingDecision();
    CHECK(third && third->knowledge.falseAlarm);
    CHECK(third->revision == 3);
    CHECK(!game.ApplyDecision(Reply(*second, Intent::InspectTracks)));
    CHECK(game.ApplyDecision(Reply(*third, Intent::InspectTracks)));
    CHECK(!game.ApplyDecision(Reply(*third, Intent::InspectTracks)));
}

void VisibleTargetDoesNotInvalidateReply() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    game.RecordObservation(ObservationKind::FoxSeen, {700, 500});
    auto request = game.TakePendingDecision();
    CHECK(request && request->revision == 1);
    game.RecordObservation(ObservationKind::FoxSeen, {770, 500});
    CHECK(game.State().knowledge.revision == 1);
    CHECK(Distance(game.State().knowledge.lastSeen, {770, 500}) < 0.01f);
    CHECK(game.ApplyDecision(Reply(*request, Intent::FollowVisibleClue)));
    CHECK(Distance(game.State().guardian.target, {770, 500}) < 0.01f);
}

void MovingLightDoesNotInvalidateReply() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    game.RecordObservation(ObservationKind::LightSeen, {1130, 500});
    auto request = game.TakePendingDecision();
    CHECK(request && request->revision == 1 && request->knowledge.lightVisible);
    game.RecordObservation(ObservationKind::LightSeen, {1230, 470});
    CHECK(game.State().knowledge.revision == 1);
    CHECK(Distance(game.State().knowledge.lastSeen, {1230, 470}) < 0.01f);
    CHECK(game.ApplyDecision(Reply(*request, Intent::FollowVisibleClue)));
    CHECK(Distance(game.State().guardian.target, {1230, 470}) < 0.01f);
    game.RecordObservation(ObservationKind::LightLost, {1230, 470});
    auto lost = game.TakePendingDecision();
    CHECK(lost && !lost->knowledge.lightVisible);
    CHECK(!Has(*lost, Intent::FollowVisibleClue));
    CHECK(Distance(lost->knowledge.lastSeen, {1230, 470}) < 0.01f);
}

void InvestigatedTracksReturnToOriginalPost() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    Point post = game.State().guardian.post;
    game.RecordObservation(ObservationKind::TracksSeen, {880, 500});
    auto tracks = game.TakePendingDecision();
    CHECK(tracks && tracks->knowledge.tracksPending);
    CHECK(game.ApplyDecision(Reply(*tracks, Intent::InspectTracks)));
    for (int frame = 0; frame < 240 && game.State().knowledge.tracksPending; ++frame)
        game.Update({}, 1.0f / 60.0f);
    CHECK(game.State().knowledge.tracksSeen);
    CHECK(!game.State().knowledge.tracksPending);
    CHECK(game.State().guardian.form == GuardianForm::Animal);
    auto completed = game.TakePendingDecision();
    CHECK(completed && !completed->knowledge.tracksPending);
    CHECK(!Has(*completed, Intent::InspectTracks));
    CHECK(Distance(completed->knowledge.lastTracks, {880, 500}) < 0.01f);
    LocalPersonalityPolicy policy;
    CHECK(policy.Choose(*completed) == Intent::GuardLantern);
    CHECK(game.ApplyDecision(Reply(*completed, Intent::GuardLantern)));
    CHECK(game.State().guardian.form == GuardianForm::Returning);
    CHECK(Distance(game.State().guardian.target, post) < 0.01f);
    for (int frame = 0; frame < 240 && game.State().guardian.form != GuardianForm::Stone; ++frame)
        game.Update({}, 1.0f / 60.0f);
    CHECK(game.State().guardian.form == GuardianForm::Stone);
    CHECK(Distance(game.State().guardian.position, post) < 0.01f);
}

void FreshTracksInterruptReturn() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    game.RecordObservation(ObservationKind::TracksSeen, {970, 470});
    auto first = game.TakePendingDecision();
    CHECK(first && game.ApplyDecision(Reply(*first, Intent::InspectTracks)));
    for (int frame = 0; frame < 180 && game.State().knowledge.tracksPending; ++frame)
        game.Update({}, 1.0f / 60.0f);
    CHECK(!game.State().knowledge.tracksPending);
    auto completed = game.TakePendingDecision();
    CHECK(completed && game.ApplyDecision(Reply(*completed, Intent::GuardLantern)));
    CHECK(game.State().guardian.form == GuardianForm::Returning);
    game.RecordObservation(ObservationKind::TracksSeen, {850, 520});
    auto fresh = game.TakePendingDecision();
    CHECK(fresh && fresh->knowledge.tracksPending && Has(*fresh, Intent::InspectTracks));
    CHECK(game.ApplyDecision(Reply(*fresh, Intent::InspectTracks)));
    CHECK(game.State().guardian.form == GuardianForm::Animal);
    CHECK(Distance(game.State().guardian.target, {850, 520}) < 0.01f);
}

void OlderBellFinishesBeforeReturning() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    Point post = game.State().guardian.post;
    game.RecordObservation(ObservationKind::BellHeard, {350, 500});
    game.RecordObservation(ObservationKind::TracksSeen, {900, 500});
    auto tracks = game.TakePendingDecision();
    CHECK(tracks && game.ApplyDecision(Reply(*tracks, Intent::InspectTracks)));
    for (int frame = 0; frame < 240 && game.State().knowledge.tracksPending; ++frame)
        game.Update({}, 1.0f / 60.0f);
    CHECK(!game.State().knowledge.tracksPending);
    auto bell = game.TakePendingDecision();
    CHECK(bell && Has(*bell, Intent::InspectBell));
    LocalPersonalityPolicy policy;
    CHECK(policy.Choose(*bell) == Intent::InspectBell);
    CHECK(game.ApplyDecision(Reply(*bell, Intent::InspectBell)));
    for (int frame = 0; frame < 500 && !game.State().knowledge.falseAlarm; ++frame)
        game.Update({}, 1.0f / 60.0f);
    CHECK(game.State().knowledge.falseAlarm);
    auto completed = game.TakePendingDecision();
    CHECK(completed && !Has(*completed, Intent::InspectBell) && !Has(*completed, Intent::InspectTracks));
    CHECK(policy.Choose(*completed) == Intent::GuardLantern);
    CHECK(game.ApplyDecision(Reply(*completed, Intent::GuardLantern)));
    for (int frame = 0; frame < 500 && game.State().guardian.form != GuardianForm::Stone; ++frame)
        game.Update({}, 1.0f / 60.0f);
    CHECK(game.State().guardian.form == GuardianForm::Stone);
    CHECK(Distance(game.State().guardian.position, post) < 0.01f);
}

void BellRepetitionAndFiniteWatch() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    Input ring{};
    ring.ringBell = true;
    game.Update(ring, 1.0f / 60.0f);
    auto first = game.TakePendingDecision();
    CHECK(first && first->knowledge.bellCount == 1);
    CHECK(game.ApplyDecision(Reply(*first, Intent::WaitAndWatch)));
    for (int i = 0; i < 130; ++i) game.Update({}, 1.0f / 60.0f);
    auto afterWatch = game.TakePendingDecision();
    CHECK(afterWatch && afterWatch->knowledge.watchCompleted);
    CHECK(!Has(*afterWatch, Intent::WaitAndWatch));
    game.Update(ring, 1.0f / 60.0f);
    auto repeated = game.TakePendingDecision();
    CHECK(repeated && repeated->knowledge.bellCount >= 2);
    CHECK(!repeated->knowledge.watchCompleted);
    CHECK(repeated->decisionId > afterWatch->decisionId);
}

void DecisionIdentityAndLegality() {
    Game game(StartMode::ShrinePractice);
    game.RecordObservation(ObservationKind::BellHeard, {350, 500});
    auto request = game.TakePendingDecision();
    CHECK(request);
    CHECK(!game.ApplyDecision({request->attemptId + 1, request->decisionId, request->revision,
                               Intent::InspectBell}));
    CHECK(!game.ApplyDecision({request->attemptId, request->decisionId + 1, request->revision,
                               Intent::InspectBell}));
    CHECK(!game.ApplyDecision({request->attemptId, request->decisionId, request->revision + 1,
                               Intent::InspectBell}));
    CHECK(!game.ApplyDecision(Reply(*request, Intent::WatchExit)));
    CHECK(game.State().guardian.form == GuardianForm::Stone);
    CHECK(game.ApplyDecision(Reply(*request, Intent::WaitAndWatch)));
    CHECK(game.State().guardian.form == GuardianForm::Stone);
    CHECK(!game.ApplyDecision(Reply(*request, Intent::InspectBell)));
}

void StoneNeedsChosenDeparture() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    game.RecordObservation(ObservationKind::BellHeard, {350, 500});
    CHECK(game.State().guardian.form == GuardianForm::Stone);
    auto request = game.TakePendingDecision();
    CHECK(request && game.ApplyDecision(Reply(*request, Intent::InspectBell)));
    CHECK(game.State().guardian.form == GuardianForm::Awakening);
    CHECK(game.Sounds().stone == 1);
    Point original = game.State().guardian.position;
    Step(game, {}, 20);
    CHECK(game.State().guardian.form == GuardianForm::Awakening);
    CHECK(Distance(game.State().guardian.position, original) < 0.01f);
    Step(game, {}, 60);
    CHECK(game.State().guardian.form == GuardianForm::Animal);
    CHECK(Distance(game.State().guardian.position, original) > 0.0f);
}

void StoneSeesFoxBeforeMoving() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    ReachLantern(game);
    WalkTo(game, {790, 470}, 80);
    CHECK(game.State().guardian.form == GuardianForm::Stone);
    CHECK(game.State().knowledge.foxVisible);
    auto request = game.TakePendingDecision();
    CHECK(request && Has(*request, Intent::FollowVisibleClue));
    CHECK(request->guardianForm == GuardianForm::Stone);
    CHECK(Distance(request->guardianPosition, game.State().guardian.position) < 0.01f);
    CHECK(game.ApplyDecision(Reply(*request, Intent::FollowVisibleClue)));
    CHECK(game.State().guardian.form == GuardianForm::Awakening);
}

void MovementTracksAndSweep() {
    Game game(StartMode::ShrinePractice);
    Point start = game.State().fox.position;
    Step(game, {-1, 0}, 60);
    float walked = Distance(game.State().fox.position, start);
    CHECK(walked > 180 && walked < 190);
    CHECK(!game.State().footprints.empty());
    CHECK(game.Sounds().step > 0);
    Input sweep{};
    sweep.sweep = true;
    game.Update(sweep, 1.0f / 60.0f);
    CHECK(game.Sounds().sweep == 1);
    CHECK(std::any_of(game.State().footprints.begin(), game.State().footprints.end(),
                      [](const Footprint& step) { return step.erased; }));
    game.Update(sweep, 1.0f / 60.0f);
    CHECK(game.Sounds().sweep == 1);
    Step(game, {}, 30);
    CHECK(game.State().fox.sweepCooldown <= 0.0f);
    Step(game, {1, 0}, 20);
    game.Update(sweep, 1.0f / 60.0f);
    CHECK(game.Sounds().sweep == 2);
}

void MovementModesAndFrameClamp() {
    Game game(StartMode::ShrinePractice);
    Step(game, {-1, 0}, 1, true);
    CHECK(game.State().fox.crouching && !game.State().fox.running);
    Step(game, {-1, 0}, 1, false, true);
    CHECK(!game.State().fox.crouching && game.State().fox.running);
    Point before = game.State().fox.position;
    Input move{};
    move.move = {-1, 0};
    game.Update(move, 1.0f);
    CHECK(Distance(game.State().fox.position, before) <= 15.51f);
    before = game.State().fox.position;
    game.Update(move, -1.0f);
    CHECK(Distance(game.State().fox.position, before) < 0.01f);
    Step(game, {}, 1, true);
    CHECK(game.State().fox.crouching && !game.State().fox.running);
}

void LowPassageRequiresCrouch() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    ReachLantern(game);
    WalkTo(game, {650, 550}, 140);
    WalkTo(game, {850, 555}, 180);
    WalkTo(game, {1030, 520}, 180);
    WalkTo(game, {1230, 470}, 180);
    Step(game, {170, -60}, 180);
    CHECK(game.State().fox.position.x <= 1331.0f);
    WalkTo(game, {1400, 410}, 180, true);
    CHECK(game.State().fox.position.x > 1350.0f);
    CHECK(game.State().fox.position.y > 335.0f && game.State().fox.position.y < 425.0f);
    CHECK(game.State().fox.crouching);
    Input run{};
    run.toggleRun = true;
    game.Update(run, 1.0f / 60.0f);
    CHECK(game.State().fox.crouching && !game.State().fox.running);
    CHECK(game.Notice() == "倒木の下では走れない");
    Input stand{};
    stand.toggleCrouch = true;
    game.Update(stand, 1.0f / 60.0f);
    CHECK(game.State().fox.crouching);
    CHECK(game.Notice() == "倒木の下では伏せたまま進もう");
}

void BellAndTheftObservation() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    Input bell{};
    bell.ringBell = true;
    game.Update(bell, 1.0f / 60.0f);
    CHECK(game.Sounds().bell == 1);
    CHECK(game.State().knowledge.bellHeard);
    CHECK(!game.State().knowledge.theftKnown);
    game.Update(bell, 1.0f / 60.0f);
    CHECK(game.Sounds().bell == 1);
    auto request = game.TakePendingDecision();
    CHECK(request && Has(*request, Intent::InspectBell));
    CHECK(!Has(*request, Intent::WatchExit));
    CHECK(game.ApplyDecision(Reply(*request, Intent::WaitAndWatch)));
    ReachLantern(game);
    CHECK(game.State().lantern.owner == LanternOwner::Pedestal);
    CHECK(!game.State().knowledge.theftKnown);
    Input interact{};
    interact.interact = true;
    game.Update(interact, 1.0f / 60.0f);
    CHECK(game.State().lantern.owner == LanternOwner::Fox);
    CHECK(game.State().stage == Stage::Escape);
    CHECK(game.State().knowledge.theftKnown);
    Point held = game.State().fox.position;
    game.Update(interact, 1.0f / 60.0f);
    CHECK(game.State().lantern.owner == LanternOwner::Ground);
    CHECK(Distance(game.State().lantern.position, held) < 0.01f);
    game.Update(interact, 1.0f / 60.0f);
    CHECK(game.State().lantern.owner == LanternOwner::Fox);
    CHECK(game.Sounds().lantern == 3);
}

void CheckpointRestoresWholeSnapshot() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    game.RecordObservation(ObservationKind::BellHeard, {350, 500});
    auto bell = game.TakePendingDecision();
    CHECK(bell && game.ApplyDecision(Reply(*bell, Intent::WaitAndWatch)));
    game.SaveCheckpointC();
    CHECK(game.HasCheckpointC());
    const WorldState saved = game.State();
    ReachLantern(game);
    game.RecordObservation(ObservationKind::InvestigationEmpty, {350, 500});
    game.RecordObservation(ObservationKind::FoxSeen, game.State().fox.position);
    auto obsolete = game.TakePendingDecision();
    CHECK(obsolete);
    Input take{};
    take.interact = true;
    game.Update(take, 1.0f / 60.0f);
    CHECK(game.State().lantern.owner == LanternOwner::Fox);
    game.RecordObservation(ObservationKind::EmptyPedestalSeen, {780, 445});
    CHECK(game.State().knowledge.theftKnown);
    const auto oldAttempt = game.AttemptId();
    game.Capture();
    CHECK(game.State().stage == Stage::Captured);
    Input restart{};
    restart.restart = true;
    game.Update(restart, 1.0f / 60.0f);
    CHECK(game.AttemptId() == oldAttempt + 1);
    CHECK(game.State().stage == Stage::Courtyard);
    CHECK(game.State().lantern.owner == LanternOwner::Pedestal);
    CHECK(game.State().knowledge.bellHeard == saved.knowledge.bellHeard);
    CHECK(game.State().knowledge.revision == saved.knowledge.revision);
    CHECK(!game.State().knowledge.falseAlarm && !game.State().knowledge.foxVisible);
    CHECK(!game.State().knowledge.theftKnown);
    CHECK(game.State().footprints.size() == saved.footprints.size());
    CHECK(Distance(game.State().guardian.position, saved.guardian.position) < 0.01f);
    CHECK(game.State().guardian.form == saved.guardian.form);
    CHECK(!game.ApplyDecision(Reply(*obsolete, Intent::FollowVisibleClue)));
    CHECK(!game.TakePendingDecision());
}

void CheckpointBAndCapturedFreeze() {
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    game.RecordObservation(ObservationKind::BellHeard, {350, 500});
    auto obsolete = game.TakePendingDecision();
    CHECK(obsolete);
    game.Capture();
    auto captured = game.State();
    Input blocked{};
    blocked.move = {1, 0};
    blocked.interact = true;
    game.Update(blocked, 1.0f);
    CHECK(game.State().stage == Stage::Captured);
    CHECK(Distance(game.State().fox.position, captured.fox.position) < 0.01f);
    Input restart{};
    restart.restart = true;
    game.Update(restart, 1.0f / 60.0f);
    CHECK(game.State().stage == Stage::Approach);
    CHECK(!game.State().knowledge.bellHeard);
    CHECK(!game.HasCheckpointC());
    CHECK(!game.ApplyDecision(Reply(*obsolete, Intent::InspectBell)));
}

void NestEndingNeverAsksForDecision() {
    Game game;
    Input touch{};
    touch.interact = true;
    game.Update(touch, 1.0f / 60.0f);
    WalkTo(game, {280, 430}, 300, false, true);
    WalkTo(game, {530, 440}, 300);
    game.SetExternalDecisionMode(true);
    ReachLantern(game);
    game.Update(touch, 1.0f / 60.0f);
    CHECK(game.State().lantern.owner == LanternOwner::Fox);
    ReachLogExit(game);
    WalkTo(game, {1240, 470}, 180, true);
    WalkTo(game, {1030, 515}, 180);
    WalkTo(game, {760, 555}, 180, false, true);
    WalkTo(game, {445, 555}, 180, false, true);
    WalkTo(game, {330, 665}, 100, false, true);
    CHECK(game.State().stage == Stage::NestEnding);
    CHECK(!game.State().endingLanternPlaced);
    CHECK(Distance(game.State().fox.position, {280, 430}) < 1.0f);
    CHECK(!game.TakePendingDecision());
    WalkTo(game, {385, 475}, 100);
    WalkTo(game, {545, 590}, 160);
    WalkTo(game, {690, 650}, 130);
    WalkTo(game, {790, 620}, 100);
    game.Update(touch, 1.0f / 60.0f);
    CHECK(game.State().endingLanternPlaced);
    Point entrance = game.State().guardian.position;
    for (int frame = 0; frame < 60; ++frame) game.Update({}, 1.0f / 60.0f);
    CHECK(Distance(game.State().guardian.position, entrance) < 0.01f);
    CHECK(game.Sounds().fire == 0);
    for (int frame = 0; frame < 150; ++frame) game.Update({}, 1.0f / 60.0f);
    CHECK(Distance(game.State().guardian.position, entrance) > 100.0f);
    CHECK(game.Sounds().fire == 0);
    game.TakePendingDecision();
    const auto decided = game.State().guardian.intent;
    for (int frame = 0; frame < 960 && game.State().stage == Stage::NestEnding; ++frame) {
        game.Capture();
        game.Update({}, 1.0f / 60.0f);
        CHECK(!game.TakePendingDecision());
        CHECK(game.State().guardian.intent == decided);
    }
    CHECK(game.State().stage == Stage::Complete);
    CHECK(game.Sounds().fire == 1);
    CHECK(game.Sounds().capture == 0);
}

void Run(const char* name, const std::function<void()>& body) {
    try { body(); std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& error) {
        std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        throw;
    }
}

} // namespace

int main() {
    Run("J-01..05 and observed clue choices", DecisionFixtures);
    Run("A-07 observation revision and same-state deduplication", ObservationRevisionAndMemory);
    Run("Jev reply survives moving visible clue", VisibleTargetDoesNotInvalidateReply);
    Run("Jev reply follows moving carried light", MovingLightDoesNotInvalidateReply);
    Run("investigated tracks return to the original post", InvestigatedTracksReturnToOriginalPost);
    Run("fresh tracks interrupt the return", FreshTracksInterruptReturn);
    Run("older bell finishes before returning to post", OlderBellFinishesBeforeReturning);
    Run("repeated bell and finite watch request new decisions", BellRepetitionAndFiniteWatch);
    Run("A-03/A-05 decision identity, legality, and replay", DecisionIdentityAndLegality);
    Run("A-08 chosen departure precedes transformation", StoneNeedsChosenDeparture);
    Run("stone sees fox and can begin pursuit", StoneSeesFoxBeforeMoving);
    Run("F-01/F-02 movement, tracks, and sweep cooldown", MovementTracksAndSweep);
    Run("F-01/N-02 movement modes and frame clamp", MovementModesAndFrameClamp);
    Run("F-01/N-02 low passage requires crouch", LowPassageRequiresCrouch);
    Run("F-03/F-04 bell, theft boundary, drop, and pickup", BellAndTheftObservation);
    Run("G-02/D-18/J-11 CP-C restores world and memory", CheckpointRestoresWholeSnapshot);
    Run("G-02 CP-B and captured-state freeze", CheckpointBAndCapturedFreeze);
    Run("J-06/P-05 nest ending is fixed and never asks for a decision", NestEndingNeverAsksForDecision);
}
