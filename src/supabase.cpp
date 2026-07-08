#include "supabase.hpp"

#include <nlohmann/json.hpp>

#include "http.hpp"

namespace supabase {

namespace {

std::string trimSlash(std::string url) {
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    return url;
}

}  // namespace

Client::Client(SupabaseConfig config) : config_(std::move(config)) {}

bool Client::logLoginEvent(const std::string& username, const std::string& ip, bool authSuccess,
                           std::string* error) {
    if (!configured()) {
        if (error) {
            *error = "supabase not configured";
        }
        return false;
    }

    const std::string endpoint =
        trimSlash(config_.url) + "/rest/v1/" + config_.loginEventsTable;

    nlohmann::json row;
    row["username"] = username;
    row["ip"] = ip;
    row["success"] = authSuccess;
    const std::string body = nlohmann::json::array({row}).dump();

    const std::vector<std::string> headers = {
        "apikey: " + config_.anonKey,
        "Authorization: Bearer " + config_.anonKey,
        "Prefer: return=minimal",
    };

    const net::HttpResponse resp = net::httpPost(endpoint, body, "application/json", headers);
    if (!resp.ok) {
        if (error) {
            *error = "network error: " + resp.error;
        }
        return false;
    }
    if (resp.status < 200 || resp.status >= 300) {
        if (error) {
            *error = "supabase returned status " + std::to_string(resp.status) + ": " + resp.body;
        }
        return false;
    }
    return true;
}

std::string Client::fetchStatus() {
    if (!configured()) {
        return {};
    }

    const std::string endpoint = trimSlash(config_.url) + "/rest/v1/" + config_.statusTable +
                                 "?select=message&order=created_at.desc&limit=1";

    const std::vector<std::string> headers = {
        "apikey: " + config_.anonKey,
        "Authorization: Bearer " + config_.anonKey,
    };

    const net::HttpResponse resp = net::httpGet(endpoint, headers);
    if (!resp.ok || resp.status < 200 || resp.status >= 300) {
        return {};
    }

    try {
        const auto json = nlohmann::json::parse(resp.body);
        if (json.is_array() && !json.empty() && json[0].contains("message")) {
            return json[0]["message"].get<std::string>();
        }
    } catch (...) {
        // ignore parse errors, treat as no status
    }
    return {};
}

}  // namespace supabase
