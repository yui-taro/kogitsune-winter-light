#include "JevBridge.hpp"

#include "nlohmann/json.hpp"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <cstdlib>
#else
#include <curl/curl.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>

#ifdef __EMSCRIPTEN__
// Browser build: the page stores the relay login token in sessionStorage and exposes the
// relay base URL (no trailing slash) as window.KOGITSUNE_RELAY_URL. Requests run as fetch() calls polled per frame.
EM_JS(char*, kg_relay_url, (), {
    var url = "";
    try { if (sessionStorage.getItem("kogitsune-token")) url = window.KOGITSUNE_RELAY_URL || ""; } catch (e) {}
    return stringToNewUTF8(url ? url + "/decision" : "");
});
EM_JS(int, kg_relay_start, (const char* url, const char* body), {
    Module.kgRelay = Module.kgRelay || {next: 0, slots: {}};
    var id = ++Module.kgRelay.next;
    var slot = Module.kgRelay.slots[id] = {state: 0, text: ""};
    var token = "";
    try { token = sessionStorage.getItem("kogitsune-token") || ""; } catch (e) {}
    var controller = new AbortController();
    var timer = setTimeout(function () { controller.abort(); }, 5000);
    fetch(UTF8ToString(url), {method: "POST", body: UTF8ToString(body), signal: controller.signal,
                              headers: {"Content-Type": "application/json", "Authorization": "Bearer " + token}})
        .then(function (r) {
            if (r.status === 401) {
                try { sessionStorage.removeItem("kogitsune-token"); } catch (e) {}
                if (Module.onRelayLoggedOut) Module.onRelayLoggedOut();
            }
            return r.ok ? r.text() : Promise.reject(r.status);
        })
        .then(function (text) { clearTimeout(timer); slot.text = text; slot.state = 1; })
        .catch(function () { clearTimeout(timer); slot.state = 2; });
    return id;
});
EM_JS(int, kg_relay_state, (int id), {
    var slot = Module.kgRelay && Module.kgRelay.slots[id];
    return slot ? slot.state : 2;
});
EM_JS(char*, kg_relay_take, (int id), {
    var slot = Module.kgRelay.slots[id];
    delete Module.kgRelay.slots[id];
    return stringToNewUTF8(slot ? slot.text : "");
});
#endif

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
    // Only a logged-in browser session talks to the relay; otherwise the local policy decides.
    char* url = kg_relay_url();
    url_ = url;
    std::free(url);
    if (!url_.empty()) status_ = Status::Waiting;
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
    return Parse(responseBody, request);
}
#endif

std::optional<JevBridge::RelayReply> JevBridge::Parse(const std::string& responseBody, const DecisionRequest& request) {
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

void JevBridge::Finish(Game& game, std::optional<RelayReply> response) {
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

void JevBridge::Poll(Game& game) {
    if (!Configured()) return;
#ifdef __EMSCRIPTEN__
    if (webJob_ != 0) {
        int state = kg_relay_state(webJob_);
        if (state == 0) return;
        char* text = kg_relay_take(webJob_);
        std::string body = text;
        std::free(text);
        webJob_ = 0;
        Finish(game, state == 1 && active_ ? Parse(body, *active_) : std::nullopt);
    }
    if (webJob_ == 0) {
        active_ = game.TakePendingDecision();
        if (active_) webJob_ = kg_relay_start(url_.c_str(), RequestJson(*active_).dump().c_str());
    }
#else
    if (job_.valid() && job_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        Finish(game, job_.get());
    }
    if (!job_.valid()) {
        active_ = game.TakePendingDecision();
        if (active_) {
            auto request = *active_;
            job_ = std::async(std::launch::async, [url = url_, request]() { return Request(url, request); });
        }
    }
#endif
}

} // namespace foxgame
