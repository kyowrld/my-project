#pragma once

#include <string>

#include "config.hpp"

namespace supabase {

// Thin Supabase (PostgREST) client used for two things:
//  * recording login events for auditing, and
//  * fetching a service-status message shown on the dashboard.
// Both are optional and degrade gracefully when Supabase is not configured.
class Client {
public:
    explicit Client(SupabaseConfig config);

    bool configured() const { return config_.configured(); }

    // Inserts a row into the login-events table. Returns false on failure and
    // fills `error`. Never throws.
    bool logLoginEvent(const std::string& username, const std::string& ip,
                       bool authSuccess, std::string* error = nullptr);

    // Fetches the most recent service status message. Returns empty string if
    // unavailable.
    std::string fetchStatus();

private:
    SupabaseConfig config_;
};

}  // namespace supabase
