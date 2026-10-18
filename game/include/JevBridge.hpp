#pragma once

#include "World.hpp"

#include <future>
#include <optional>
#include <string>

namespace foxgame {

// The game talks only to a relay. The TypeSafe API key never enters this process.
class JevBridge {
public:
    enum class Status { Local, Waiting, Jev, Mock, Fallback };
    explicit JevBridge(std::string relayUrl);

    bool Configured() const { return !url_.empty(); }
    bool Degraded() const { return status_ == Status::Fallback; }
    Status CurrentStatus() const { return status_; }
    void Poll(Game& game);

private:
    struct RelayReply { DecisionResponse decision; Status source; };
    static std::optional<RelayReply> Request(const std::string& url, const DecisionRequest& request);
    static std::optional<RelayReply> Parse(const std::string& body, const DecisionRequest& request);
    void Finish(Game& game, std::optional<RelayReply> response);

    std::string url_;
    std::future<std::optional<RelayReply>> job_{};
#ifdef __EMSCRIPTEN__
    int webJob_ = 0;  // id of the pending fetch, 0 when idle
#endif
    std::optional<DecisionRequest> active_{};
    Status status_ = Status::Local;
};

} // namespace foxgame
