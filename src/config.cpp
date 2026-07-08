#include "config.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace {

std::string getString(const nlohmann::json& obj, const char* key, const std::string& fallback) {
    if (obj.contains(key) && obj[key].is_string()) {
        return obj[key].get<std::string>();
    }
    return fallback;
}

}  // namespace

AppConfig AppConfig::load(const std::string& path, std::string* loadError) {
    AppConfig config;

    std::ifstream file(path);
    if (!file) {
        if (loadError) {
            *loadError = "config file not found: " + path;
        }
        return config;
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const std::exception& e) {
        if (loadError) {
            *loadError = std::string("failed to parse config: ") + e.what();
        }
        return config;
    }

    if (json.contains("app") && json["app"].is_object()) {
        const auto& app = json["app"];
        config.title = getString(app, "title", config.title);
        config.subtitle = getString(app, "subtitle", config.subtitle);
        if (app.contains("demo_mode") && app["demo_mode"].is_boolean()) {
            config.demoMode = app["demo_mode"].get<bool>();
        }
    }

    if (json.contains("keyauth") && json["keyauth"].is_object()) {
        const auto& ka = json["keyauth"];
        config.keyauth.name = getString(ka, "name", config.keyauth.name);
        config.keyauth.ownerid = getString(ka, "ownerid", config.keyauth.ownerid);
        config.keyauth.version = getString(ka, "version", config.keyauth.version);
        config.keyauth.url = getString(ka, "url", config.keyauth.url);
    }

    if (json.contains("supabase") && json["supabase"].is_object()) {
        const auto& sb = json["supabase"];
        config.supabase.url = getString(sb, "url", config.supabase.url);
        config.supabase.anonKey = getString(sb, "anon_key", config.supabase.anonKey);
        config.supabase.loginEventsTable =
            getString(sb, "login_events_table", config.supabase.loginEventsTable);
        config.supabase.statusTable = getString(sb, "status_table", config.supabase.statusTable);
    }

    if (json.contains("launcher") && json["launcher"].is_object()) {
        const auto& l = json["launcher"];
        config.launcher.command = getString(l, "command", config.launcher.command);
        config.launcher.label = getString(l, "label", config.launcher.label);
    }

    return config;
}
