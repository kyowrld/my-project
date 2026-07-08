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

struct GLFWwindow;

// Owns all application state and renders the two screens (login + dashboard).
class App {
public:
    explicit App(AppConfig config);
    ~App();

    // The borderless GLFW window we draw custom chrome for (traffic lights,
    // dragging). Must be called before render().
    void setWindow(GLFWwindow* window) { window_ = window; }

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
        std::string licenseKey;
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

    // Resizes the OS window to fit the active screen (compact login box vs.
    // larger dashboard), keeping it centered on its current position.
    void applyWindowForScreen(Screen s);
    // Eases the OS window toward the target size each frame (resize animation).
    void stepWindowResize();

    // Custom window chrome (borderless): rounded background, drag-to-move, and
    // the red/orange/green traffic lights. Returns the content inset region.
    void renderChrome(const ImVec2& size);
    void handleWindowDrag(const ImVec2& size);

    void beginAuth();
    void runAuth(AuthMode mode, std::string field1, std::string field2, std::string code);
    void runLauncher();

    void log(const std::string& line);

    AppConfig config_;
    keyauth::Client keyauth_;
    supabase::Client supabase_;
    GLFWwindow* window_ = nullptr;

    // Window-drag bookkeeping for the borderless title bar.
    bool dragging_ = false;
    double dragCursorX_ = 0.0;
    double dragCursorY_ = 0.0;
    int dragWindowX_ = 0;
    int dragWindowY_ = 0;

    // Fullscreen toggle (green light) restores to this windowed geometry.
    bool fullscreen_ = false;
    int savedX_ = 0, savedY_ = 0, savedW_ = 0, savedH_ = 0;

    // Smooth window-resize animation between screens (login <-> dashboard).
    float winW_ = 440.0f, winH_ = 496.0f;   // current animated size
    float targetW_ = 440.0f, targetH_ = 496.0f;
    float anchorCX_ = 0.0f, anchorCY_ = 0.0f;  // screen-space center to grow from
    bool resizing_ = false;
    // Screen content fade-in (0 -> 1) restarted on each screen change.
    float screenAlpha_ = 0.0f;

    Screen screen_ = Screen::Login;
    Screen appliedScreen_ = Screen::Dashboard;  // != screen_ so size applies frame 1
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
