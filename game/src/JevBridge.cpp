#include "JevBridge.hpp"

#include "nlohmann/json.hpp"
#include <curl/curl.h>

#include <algorithm>
#include <chrono>

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

Json RequestJson(const DecisionRequest& request) {
    const auto& k = request.knowledge;
    Json knowledge = {
        {"bellHeard", k.bellHeard}, {"tracksSeen", k.tracksSeen},
        {"foxVisible", k.foxVisible}, {"theftKnown", k.theftKnown},
        {"falseAlarm", k.falseAlarm}, {"hasLastSeen", k.hasLastSeen},
        {"lastSeenInForest", k.lastSeenInForest}, {"tracksSeenInForest", k.tracksSeenInForest},
    };
    if (k.bellHeard) knowledge["bellPosition"] = PointJson(k.bellPosition);
    if (k.tracksSeen) knowledge["lastTracks"] = PointJson(k.lastTracks);
    if (k.hasLastSeen) knowledge["lastSeen"] = PointJson(k.lastSeen);
    Json legal = Json::array();
    for (Intent intent : request.legalIntents) legal.push_back(IntentName(intent));
    return {{"attemptId", request.attemptId}, {"decisionId", request.decisionId},
            {"revision", request.revision}, {"knowledge", knowledge}, {"legalIntents", legal}};
}

} // namespace

JevBridge::JevBridge(std::string relayUrl) : url_(std::move(relayUrl)) {
    if (!url_.empty()) curl_global_init(CURL_GLOBAL_DEFAULT);
}

std::optional<DecisionResponse> JevBridge::Request(const std::string& url, const DecisionRequest& request) {
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
        auto found = std::find_if(request.legalIntents.begin(), request.legalIntents.end(),
                                  [&](Intent intent) { return name == IntentName(intent); });
        if (found == request.legalIntents.end()) return std::nullopt;
        return DecisionResponse{response.at("attemptId").get<std::uint64_t>(),
                                response.at("decisionId").get<std::uint64_t>(),
                                response.at("revision").get<unsigned>(), *found};
    } catch (...) { return std::nullopt; }
}

void JevBridge::Poll(Game& game) {
    if (!Configured()) return;
    if (job_.valid() && job_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        auto response = job_.get();
        if (response) {
            if (game.ApplyDecision(*response)) degraded_ = false;
        } else if (active_) {
            LocalPersonalityPolicy fallback;
            game.ApplyDecision({active_->attemptId, active_->decisionId, active_->revision,
                                fallback.Choose(*active_)});
            degraded_ = true;
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
