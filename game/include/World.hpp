#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace foxgame {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

float Distance(Point a, Point b);

struct CoverZone {
    float left, top, right, bottom;
    bool Contains(Point p) const {
        return p.x > left && p.x < right && p.y > top && p.y < bottom;
    }
};

enum class Stage {
    NestIntro, ForestOutbound, Approach, Courtyard, Escape, ForestReturn,
    NestEnding, Captured, SliceComplete, Complete
};
std::array<CoverZone, 2> BushCoverZones(Stage stage);
enum class StartMode { FullStory, ShrinePractice };
enum class GuardianForm { Stone, Awakening, Animal, Returning };
enum class LanternOwner { Pedestal, Fox, Ground, Guardian };
enum class Intent { GuardLantern, InspectBell, InspectTracks, InspectLastSeen, WatchExit,
                    WatchToriiExit, WatchLogExit, FollowVisibleClue, WaitAndWatch };
enum class ObservationKind { BellHeard, TracksSeen, FoxSeen, FoxLost, EmptyPedestalSeen,
                             LightSeen, LightLost, TracksInvestigated, InvestigationEmpty };

const char* IntentName(Intent intent);

struct Footprint {
    Point position;
    float createdAt = 0.0f;
    bool erased = false;
    bool seenByGuardian = false;
};

struct Knowledge {
    bool bellHeard = false;
    unsigned bellCount = 0;
    float lastBellAt = 0.0f;
    bool tracksSeen = false;
    bool tracksPending = false;
    bool foxVisible = false;
    bool lightVisible = false;
    bool theftKnown = false;
    bool falseAlarm = false;
    bool watchCompleted = false;
    bool hasLastSeen = false;
    bool lastSeenInForest = false;
    bool tracksSeenInForest = false;
    Point lastSeen{};
    Point lastTracks{};
    Point bellPosition{};
    float lastSeenAt = 0.0f;
    float lastTracksAt = 0.0f;
    float lastTracksDecisionAt = -1000.0f;
    unsigned revision = 0;
};

struct Fox {
    Point position{345.0f, 745.0f};
    bool crouching = false;
    bool running = false;
    float facingX = 1.0f;
    float facingY = 0.0f;
    bool moving = false;
    float walkDistance = 0.0f;
    float stepDistance = 0.0f;
    float sweepCooldown = 0.0f;
};

struct Guardian {
    Point position{1005.0f, 470.0f};
    Point post{1005.0f, 470.0f};
    Point facing{-1.0f, 0.35f};
    Point target{1005.0f, 470.0f};
    GuardianForm form = GuardianForm::Stone;
    Intent intent = Intent::GuardLantern;
    float transformTimer = 0.0f;
    float warningTimer = 0.0f;
    float waitTimer = 0.0f;
    bool moving = false;
    float walkDistance = 0.0f;
};

struct Lantern {
    LanternOwner owner = LanternOwner::Pedestal;
    Point position{780.0f, 445.0f};
};

struct WorldState {
    Stage stage = Stage::NestIntro;
    Stage capturedScene = Stage::Approach;
    Fox fox{};
    Guardian guardian{};
    Lantern lantern{};
    Knowledge knowledge{};
    std::vector<Footprint> footprints{};
    std::vector<Footprint> forestFootprints{};
    float time = 0.0f;
    float stageTime = 0.0f;
    float bellCooldown = 0.0f;
    bool windBellFired = false;
    bool nestTwigsTouched = false;
    bool guardianEnteredForest = false;
    bool endingLanternPlaced = false;
};

struct Input {
    Point move{};
    bool toggleCrouch = false;
    bool toggleRun = false;
    bool sweep = false;
    bool interact = false;
    bool ringBell = false;
    bool restart = false;
};

struct DecisionRequest {
    std::uint64_t attemptId = 0;
    std::uint64_t decisionId = 0;
    unsigned revision = 0;
    Knowledge knowledge{};
    std::vector<Intent> legalIntents{};
    Stage stage = Stage::Approach;
    GuardianForm guardianForm = GuardianForm::Stone;
    Intent currentIntent = Intent::GuardLantern;
    Point guardianPosition{};
    float gameTime = 0.0f;
};

struct DecisionResponse {
    std::uint64_t attemptId = 0;
    std::uint64_t decisionId = 0;
    unsigned revision = 0;
    Intent choice = Intent::WaitAndWatch;
};

struct SoundEvents {
    unsigned bell = 0;
    unsigned step = 0;
    unsigned sweep = 0;
    unsigned lantern = 0;
    unsigned stone = 0;
    unsigned capture = 0;
    unsigned fire = 0;
};

// A deterministic local stand-in for Jev, used only while the external relay is absent.
// Its input is the guardian's knowledge and legal intents, never ground truth.
class LocalPersonalityPolicy {
public:
    Intent Choose(const DecisionRequest& request) const;
};

class Game {
public:
    explicit Game(StartMode start = StartMode::FullStory);

    const WorldState& State() const { return world_; }
    std::uint64_t AttemptId() const { return attemptId_; }
    const std::string& Notice() const { return notice_; }
    const SoundEvents& Sounds() const { return soundEvents_; }
    bool HasCheckpointC() const { return checkpointC_.has_value(); }
    bool FoxHidden() const { return InCover(world_.fox.position); }
    void SetExternalDecisionMode(bool enabled) { externalDecisions_ = enabled; }

    void Update(const Input& input, float dt);
    void RecordObservation(ObservationKind kind, Point where);
    void SaveCheckpointC();
    void Capture();
    void RestoreCheckpoint();

    std::optional<DecisionRequest> TakePendingDecision();
    bool ApplyDecision(const DecisionResponse& response);
    void ResolvePendingLocally();

private:
    bool CanMoveTo(Point position) const;
    bool InCover(Point position) const;
    bool CanSeeFox() const;
    void UpdateGuardian(float dt);
    void UpdatePerception();
    void UpdateFox(const Input& input, float dt);
    void Interact();
    void RingBell(bool byWind);
    void QueueDecision();
    void SetIntent(Intent intent);
    std::vector<Intent> LegalIntents() const;
    void EnterShrine(bool throughLog);
    void EnterForestReturn(bool throughLog);

    WorldState world_{};
    WorldState checkpointB_{};
    std::optional<WorldState> checkpointC_{};
    std::optional<DecisionRequest> pending_{};
    std::optional<DecisionRequest> inFlight_{};
    LocalPersonalityPolicy localPolicy_{};
    std::uint64_t attemptId_ = 1;
    std::uint64_t nextDecisionId_ = 1;
    bool captureCheckpointC_ = false;
    bool externalDecisions_ = false;
    bool practiceMode_ = false;
    SoundEvents soundEvents_{};
    std::string notice_ = "神社の灯籠を取りに行こう";
};

} // namespace foxgame
