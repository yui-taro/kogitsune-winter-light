#pragma once

#include "World.hpp"

#include <future>
#include <optional>
#include <string>

namespace foxgame {

// The game talks only to a relay. The TypeSafe API key never enters this process.
class JevBridge {
public:
    explicit JevBridge(std::string relayUrl);

    bool Configured() const { return !url_.empty(); }
    bool Degraded() const { return degraded_; }
    void Poll(Game& game);

private:
    static std::optional<DecisionResponse> Request(const std::string& url, const DecisionRequest& request);

    std::string url_;
    std::future<std::optional<DecisionResponse>> job_{};
    std::optional<DecisionRequest> active_{};
    bool degraded_ = false;
};

} // namespace foxgame
