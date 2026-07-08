#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "imgui.h"

#include "config.hpp"
#include "keyauth.hpp"
#include "supabase.hpp"

// Owns all application state and renders the two screens (login + dashboard).
class App {
public:
    explicit App(AppConfig config);
    ~App();

    // Renders the whole UI into a full-viewport window. Call once per frame.
    void render();

    // True once the user requests to quit (window handles the rest).
    bool shouldQuit() const { return quit_; }

private:
    enum class Screen { Login, Dashboard };
    enum class AuthMode { Account, License };

    struct Dashboard {
        std::string username;
        std::string ip;
        std::string hwid;
        std::string plan = "-";
        std::string expiryUnix;
        std::string createdate;
        std::string lastlogin;
        std::string status = "All systems operational";
        bool demo = false;
    };

    void renderLogin(const ImVec2& size);
    void renderDashboard(const ImVec2& size);
    void renderConsole(float width, float height);

    void beginAuth();
    void runAuth(AuthMode mode, std::string field1, std::string field2, std::string code);
    void runLauncher();

    void log(const std::string& line);

    AppConfig config_;
    keyauth::Client keyauth_;
    supabase::Client supabase_;

    Screen screen_ = Screen::Login;
    AuthMode authMode_ = AuthMode::Account;
    bool quit_ = false;

    // Login form buffers.
    char username_[128] = {0};
    char password_[128] = {0};
    char licenseKey_[128] = {0};
    char code_[32] = {0};

    std::atomic<bool> busy_{false};
    std::atomic<bool> authenticated_{false};
    std::atomic<bool> launcherRunning_{false};

    std::mutex mutex_;  // guards errorMessage_, dashboard_, console_
    std::string errorMessage_;
    Dashboard dashboard_;
    std::vector<std::string> console_;
    bool consoleDirty_ = false;
};
