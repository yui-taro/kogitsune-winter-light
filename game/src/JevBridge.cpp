#include "JevBridge.hpp"

#include "nlohmann/json.hpp"
#ifndef __EMSCRIPTEN__
#include <curl/curl.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>

namespace foxgame {
namespace {

using Json = nlohmann::json;

size_t Receive(char* data, size_t size, size_t count, void* user) {
    auto& body = *static_cast<std::string*>(user);
    size_t bytes = size * count;
    if (body.size() + bytes > 8192) return 0;
    body.append(data, bytes);
    return bytes;
}

Json PointJson(Point point) { return {{"x", point.x}, {"y", point.y}}; }
const char* SceneName(Stage stage) { return stage == Stage::ForestReturn ? "forest" : "shrine"; }
const char* FormName(GuardianForm form) {
    switch (form) {
        case GuardianForm::Stone: return "stone";
        case GuardianForm::Awakening: return "awakening";
        case GuardianForm::Animal: return "animal";
        case GuardianForm::Returning: return "returning";
    }
    return "stone";
}

Json RequestJson(const DecisionRequest& request) {
    const auto& k = request.knowledge;
    Json knowledge = {
        {"bellHeard", k.bellHeard}, {"tracksSeen", k.tracksSeen},
        {"tracksPending", k.tracksPending},
        {"foxVisible", k.foxVisible}, {"lightVisible", k.lightVisible},
        {"theftKnown", k.theftKnown},
        {"falseAlarm", k.falseAlarm}, {"watchCompleted", k.watchCompleted},
        {"bellCount", k.bellCount}, {"hasLastSeen", k.hasLastSeen},
        {"lastSeenInForest", k.lastSeenInForest}, {"tracksSeenInForest", k.tracksSeenInForest},
    };
    if (k.bellHeard) {
        knowledge["bellPosition"] = PointJson(k.bellPosition);
        knowledge["bellAge"] = (std::max)(0.0f, request.gameTime - k.lastBellAt);
    }
    if (k.tracksSeen) {
        knowledge["lastTracks"] = PointJson(k.lastTracks);
        knowledge["tracksAge"] = (std::max)(0.0f, request.gameTime - k.lastTracksAt);
    }
    if (k.hasLastSeen) {
        knowledge["lastSeen"] = PointJson(k.lastSeen);
        knowledge["lastSeenAge"] = (std::max)(0.0f, request.gameTime - k.lastSeenAt);
    }
    Json legal = Json::array();
    for (Intent intent : request.legalIntents) legal.push_back(IntentName(intent));
    return {{"attemptId", request.attemptId}, {"decisionId", request.decisionId},
            {"revision", request.revision}, {"knowledge", knowledge}, {"legalIntents", legal},
            {"guardian", {{"scene", SceneName(request.stage)},
                           {"form", FormName(request.guardianForm)},
                           {"currentIntent", IntentName(request.currentIntent)},
                           {"position", PointJson(request.guardianPosition)}}}};
}

} // namespace

JevBridge::JevBridge(std::string relayUrl) : url_(std::move(relayUrl)) {
#ifdef __EMSCRIPTEN__
    // The browser build has no relay: the guardian always uses the local policy.
    url_.clear();
#else
    if (!url_.empty()) { curl_global_init(CURL_GLOBAL_DEFAULT); status_ = Status::Waiting; }
#endif
}

#ifdef __EMSCRIPTEN__
std::optional<JevBridge::RelayReply> JevBridge::Request(const std::string&, const DecisionRequest&) {
    return std::nullopt;
}
#else
std::optional<JevBridge::RelayReply> JevBridge::Request(const std::string& url, const DecisionRequest& request) {
    CURL* curl = curl_easy_init();
    if (!curl) return std::nullopt;
    std::string requestBody = RequestJson(request).dump();
    std::string responseBody;
    curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(requestBody.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Receive);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 800L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 3500L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK || status != 200) return std::nullopt;
    try {
        Json response = Json::parse(responseBody);
        std::string name = response.at("choice").get<std::string>();
        std::string mode = response.at("mode").get<std::string>();
        if (mode != "jev" && mode != "mock") return std::nullopt;
        auto found = std::find_if(request.legalIntents.begin(), request.legalIntents.end(),
                                  [&](Intent intent) { return name == IntentName(intent); });
        if (found == request.legalIntents.end()) return std::nullopt;
        return RelayReply{DecisionResponse{response.at("attemptId").get<std::uint64_t>(),
                                response.at("decisionId").get<std::uint64_t>(),
                                response.at("revision").get<unsigned>(), *found},
                          mode == "jev" ? Status::Jev : Status::Mock};
    } catch (...) { return std::nullopt; }
}
#endif

void JevBridge::Poll(Game& game) {
    if (!Configured()) return;
    if (job_.valid() && job_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        auto response = job_.get();
        if (response) {
            bool applied = game.ApplyDecision(response->decision);
            if (applied) status_ = response->source;
            std::cerr << "guardian decision " << active_->decisionId << ' '
                      << (applied ? "applied " : "stale ") << IntentName(response->decision.choice) << '\n';
        } else if (active_) {
            LocalPersonalityPolicy fallback;
            Intent choice = fallback.Choose(*active_);
            bool applied = game.ApplyDecision({active_->attemptId, active_->decisionId, active_->revision, choice});
            status_ = Status::Fallback;
            std::cerr << "guardian decision " << active_->decisionId << ' '
                      << (applied ? "fallback " : "stale fallback ") << IntentName(choice) << '\n';
        }
        active_.reset();
    }
    if (!job_.valid()) {
        active_ = game.TakePendingDecision();
        if (active_) {
            auto request = *active_;
            job_ = std::async(std::launch::async, [url = url_, request]() { return Request(url, request); });
        }
    }
}

} // namespace foxgame
