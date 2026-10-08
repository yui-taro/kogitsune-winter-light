#include "JevBridge.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace foxgame;

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    const char* url = std::getenv("JEV_RELAY_URL");
    Check(url != nullptr, "JEV_RELAY_URL is required");
    const std::string mode = argc > 1 ? argv[1] : "success";
    Game game(StartMode::ShrinePractice);
    game.SetExternalDecisionMode(true);
    JevBridge bridge(url);
    game.RecordObservation(ObservationKind::BellHeard, {350.0f, 500.0f});
    bridge.Poll(game);
    if (mode == "stale") game.RecordObservation(ObservationKind::TracksSeen, {500.0f, 600.0f});
    const Intent expected = mode == "stale" ? Intent::InspectTracks : Intent::InspectBell;
    for (int i = 0; i < 400 && game.State().guardian.intent != expected; ++i) {
        bridge.Poll(game);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Check(game.State().guardian.intent == expected, "expected legal intent was not applied");
    Check(game.State().guardian.form == GuardianForm::Awakening, "departure did not begin");
    Check(bridge.Degraded() == (mode == "fallback"), "bridge health state is wrong");
    std::cout << "bridge integration: pass\n";
}
