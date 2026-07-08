#pragma once

#include <string>
#include <vector>

#include "config.hpp"

namespace keyauth {

struct Subscription {
    std::string name;     // subscription / plan name
    std::string expiry;   // unix timestamp (seconds) as string
};

struct UserData {
    std::string username;
    std::string ip;
    std::string hwid;
    std::string createdate;   // unix timestamp string
    std::string lastlogin;    // unix timestamp string
    std::vector<Subscription> subscriptions;
};

struct Result {
    bool success = false;
    std::string message;
};

// Minimal, cross-platform KeyAuth 1.3 client.
//
// It speaks the same wire protocol as the official SDK: form-encoded POSTs to
// the 1.3 endpoint, with each JSON response verified against KeyAuth's Ed25519
// public key using the x-signature-ed25519 / x-signature-timestamp headers.
// Unlike the official SDK it never calls exit() -- every failure is returned
// as a Result so the UI can present it.
class Client {
public:
    explicit Client(KeyAuthConfig config);

    // Establishes an application session. Must succeed before login().
    Result init();

    // Authenticates a user with username + password (+ optional 2FA code).
    Result login(const std::string& username, const std::string& password,
                 const std::string& code = "");

    // Authenticates using only a license key.
    Result license(const std::string& key, const std::string& code = "");

    bool initialized() const { return initialized_; }
    const UserData& user() const { return user_; }

    // Human-readable "in 12 days" style string for a unix-timestamp expiry.
    static std::string expiryRemaining(const std::string& unixExpiry);
    // Formats a unix-timestamp string as a local date/time.
    static std::string formatTimestamp(const std::string& unixTimestamp);

private:
    Result request(const std::string& data);

    KeyAuthConfig config_;
    std::string sessionId_;
    bool initialized_ = false;
    UserData user_;
};

}  // namespace keyauth
