#include "keyauth.hpp"

#include <sodium.h>

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <string>

#include <nlohmann/json.hpp>

#include "http.hpp"
#include "hwid.hpp"

namespace keyauth {

namespace {

// KeyAuth's Ed25519 public key (same value used by the official SDK) used to
// verify the authenticity of every API response.
constexpr const char* kApiPublicKey =
    "5586b4bc69c7a4b487e4563a4cd96afd39140f919bd31cea7d1c6a1e8439422b";

bool ensureSodium() {
    static const bool ok = (sodium_init() >= 0);
    return ok;
}

// Verifies that `body` was signed by KeyAuth. The signed message is
// (timestamp + body), matching the server implementation.
bool verifySignature(const std::string& signature, const std::string& timestamp,
                     const std::string& body) {
    if (!ensureSodium()) {
        return false;
    }
    if (signature.size() != 128 || timestamp.empty()) {
        return false;
    }

    unsigned char sig[64];
    unsigned char pk[32];
    if (sodium_hex2bin(sig, sizeof(sig), signature.c_str(), signature.size(), nullptr, nullptr,
                       nullptr) != 0) {
        return false;
    }
    if (sodium_hex2bin(pk, sizeof(pk), kApiPublicKey, std::char_traits<char>::length(kApiPublicKey),
                       nullptr, nullptr, nullptr) != 0) {
        return false;
    }

    const std::string message = timestamp + body;
    return crypto_sign_verify_detached(
               sig, reinterpret_cast<const unsigned char*>(message.data()), message.size(), pk) == 0;
}

}  // namespace

Client::Client(KeyAuthConfig config) : config_(std::move(config)) {}

Result Client::request(const std::string& data) {
    const net::HttpResponse http =
        net::httpPost(config_.url, data, "application/x-www-form-urlencoded");
    if (!http.ok) {
        return {false, "network error: " + http.error};
    }
    if (http.body == "KeyAuth_Invalid") {
        return {false, "application not found - check KeyAuth name/ownerid"};
    }

    std::string signature;
    std::string timestamp;
    if (auto it = http.headers.find("x-signature-ed25519"); it != http.headers.end()) {
        signature = it->second;
    }
    if (auto it = http.headers.find("x-signature-timestamp"); it != http.headers.end()) {
        timestamp = it->second;
    }
    if (!verifySignature(signature, timestamp, http.body)) {
        return {false, "response signature verification failed"};
    }

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(http.body);
    } catch (const std::exception& e) {
        return {false, std::string("invalid server response: ") + e.what()};
    }

    if (json.contains("ownerid") && json["ownerid"] != config_.ownerid) {
        return {false, "owner id mismatch"};
    }

    Result result;
    result.success = json.value("success", false);
    result.message = json.value("message", std::string());

    if (result.success && json.contains("sessionid")) {
        sessionId_ = json["sessionid"].get<std::string>();
    }

    if (result.success && json.contains("info") && json["info"].is_object()) {
        const auto& info = json["info"];
        user_ = UserData{};
        user_.username = info.value("username", std::string());
        user_.ip = info.value("ip", std::string());
        user_.hwid = info.contains("hwid") && info["hwid"].is_string() ? info["hwid"] : "none";
        user_.createdate = info.value("createdate", std::string());
        user_.lastlogin = info.value("lastlogin", std::string());
        if (info.contains("subscriptions") && info["subscriptions"].is_array()) {
            for (const auto& sub : info["subscriptions"]) {
                Subscription s;
                s.name = sub.value("subscription", std::string());
                s.expiry = sub.value("expiry", std::string());
                user_.subscriptions.push_back(std::move(s));
            }
        }
    }

    return result;
}

Result Client::init() {
    if (!config_.configured()) {
        return {false, "KeyAuth is not configured"};
    }
    const std::string data = "type=init&ver=" + net::urlEncode(config_.version) +
                             "&name=" + net::urlEncode(config_.name) +
                             "&ownerid=" + net::urlEncode(config_.ownerid);
    Result result = request(data);
    if (result.success) {
        initialized_ = true;
    } else if (result.message == "invalidver") {
        result.message = "version mismatch - update the version on the KeyAuth dashboard";
    }
    return result;
}

Result Client::login(const std::string& username, const std::string& password,
                     const std::string& code) {
    if (!initialized_) {
        return {false, "session not initialized"};
    }
    const std::string data = "type=login&username=" + net::urlEncode(username) +
                             "&pass=" + net::urlEncode(password) +
                             "&code=" + net::urlEncode(code) +
                             "&hwid=" + net::urlEncode(hwid::get()) +
                             "&sessionid=" + net::urlEncode(sessionId_) +
                             "&name=" + net::urlEncode(config_.name) +
                             "&ownerid=" + net::urlEncode(config_.ownerid);
    return request(data);
}

Result Client::license(const std::string& key, const std::string& code) {
    if (!initialized_) {
        return {false, "session not initialized"};
    }
    const std::string data = "type=license&key=" + net::urlEncode(key) +
                             "&code=" + net::urlEncode(code) +
                             "&hwid=" + net::urlEncode(hwid::get()) +
                             "&sessionid=" + net::urlEncode(sessionId_) +
                             "&name=" + net::urlEncode(config_.name) +
                             "&ownerid=" + net::urlEncode(config_.ownerid);
    return request(data);
}

std::string Client::formatTimestamp(const std::string& unixTimestamp) {
    if (unixTimestamp.empty()) {
        return "-";
    }
    long long value = 0;
    try {
        value = std::stoll(unixTimestamp);
    } catch (...) {
        return unixTimestamp;
    }
    const std::time_t t = static_cast<std::time_t>(value);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
}

std::string Client::expiryRemaining(const std::string& unixExpiry) {
    if (unixExpiry.empty()) {
        return "unknown";
    }
    long long expiry = 0;
    try {
        expiry = std::stoll(unixExpiry);
    } catch (...) {
        return "unknown";
    }
    const long long now = std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();
    long long diff = expiry - now;
    if (diff <= 0) {
        return "expired";
    }

    const long long days = diff / 86400;
    diff %= 86400;
    const long long hours = diff / 3600;
    diff %= 3600;
    const long long minutes = diff / 60;

    if (days > 0) {
        return std::to_string(days) + "d " + std::to_string(hours) + "h";
    }
    if (hours > 0) {
        return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    }
    return std::to_string(minutes) + "m";
}

}  // namespace keyauth
