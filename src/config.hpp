#pragma once

#include <string>

struct KeyAuthConfig {
    std::string name;
    std::string ownerid;
    std::string version = "1.0";
    std::string url = "https://keyauth.win/api/1.3/";

    bool configured() const {
        return !name.empty() && ownerid.size() == 10;
    }
};

struct SupabaseConfig {
    std::string url;
    std::string anonKey;
    std::string loginEventsTable = "login_events";
    std::string statusTable = "app_status";

    bool configured() const {
        return !url.empty() && !anonKey.empty();
    }
};

struct LauncherConfig {
    std::string command;   // shell command / executable to run
    std::string label = "Run Launcher";
};

struct AppConfig {
    std::string title = "Packet.Online";
    std::string subtitle = "secure authentication";
    // When true (or when KeyAuth is not configured) any credentials are accepted
    // and the dashboard is populated with sample data so the UI can be previewed.
    bool demoMode = true;

    KeyAuthConfig keyauth;
    SupabaseConfig supabase;
    LauncherConfig launcher;

    // Loads config from the given JSON file. Missing file / fields fall back to
    // defaults. Returns true if a file was found and parsed.
    static AppConfig load(const std::string& path, std::string* loadError = nullptr);
};
