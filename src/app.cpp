#include "app.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <thread>

#include "fonts.hpp"
#include "theme.hpp"

#if defined(_WIN32)
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace {

std::string nowStamp() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return buf;
}

// Draws a small uppercase dim label.
void label(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kTextDim);
    ui::pushFont(ui::fontRegular);
    ImGui::TextUnformatted(text);
    if (ui::fontRegular) ImGui::PopFont();
    ImGui::PopStyleColor();
}

}  // namespace

App::App(AppConfig config)
    : config_(std::move(config)),
      keyauth_(config_.keyauth),
      supabase_(config_.supabase) {
    log("purple-loader initialized");
    if (!config_.keyauth.configured()) {
        log("KeyAuth not configured - running in demo mode");
    }
}

App::~App() {
    // Detached worker threads capture `this`; in a real product you would join
    // them. For this single-window app the process exits right after.
}

void App::log(const std::string& line) {
    std::lock_guard<std::mutex> lock(mutex_);
    console_.push_back("[" + nowStamp() + "] " + line);
    if (console_.size() > 500) {
        console_.erase(console_.begin(), console_.begin() + (console_.size() - 500));
    }
    consoleDirty_ = true;
}

void App::beginAuth() {
    if (busy_) {
        return;
    }
    const AuthMode mode = authMode_;
    std::string field1 = mode == AuthMode::Account ? username_ : licenseKey_;
    std::string field2 = mode == AuthMode::Account ? password_ : std::string();
    std::string code = code_;

    if (mode == AuthMode::Account && field1.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        errorMessage_ = "enter a username";
        return;
    }
    if (mode == AuthMode::License && field1.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        errorMessage_ = "enter a license key";
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        errorMessage_.clear();
    }
    busy_ = true;
    std::thread(&App::runAuth, this, mode, field1, field2, code).detach();
}

void App::runAuth(AuthMode mode, std::string field1, std::string field2, std::string code) {
    const bool demo = config_.demoMode || !config_.keyauth.configured();
    log(std::string("authenticating via ") + (mode == AuthMode::Account ? "account" : "license") +
        (demo ? " (demo)" : ""));

    Dashboard dash;
    dash.demo = demo;
    bool ok = false;
    std::string message;

    if (demo) {
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        ok = true;
        message = "demo login";
        dash.username = mode == AuthMode::Account ? field1 : "license-user";
        dash.ip = "127.0.0.1";
        dash.hwid = "DEMO-HWID-0000";
        dash.plan = "Premium (demo)";
        const long long now = std::chrono::duration_cast<std::chrono::seconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
        dash.expiryUnix = std::to_string(now + 30LL * 86400);
        dash.createdate = std::to_string(now - 12LL * 86400);
        dash.lastlogin = std::to_string(now);
    } else {
        const keyauth::Result initResult = keyauth_.init();
        if (!initResult.success) {
            log("init failed: " + initResult.message);
            std::lock_guard<std::mutex> lock(mutex_);
            errorMessage_ = initResult.message;
            busy_ = false;
            return;
        }
        const keyauth::Result auth = mode == AuthMode::Account
                                         ? keyauth_.login(field1, field2, code)
                                         : keyauth_.license(field1, code);
        ok = auth.success;
        message = auth.message;
        if (ok) {
            const auto& u = keyauth_.user();
            dash.username = u.username.empty() ? field1 : u.username;
            dash.ip = u.ip;
            dash.hwid = u.hwid;
            dash.createdate = u.createdate;
            dash.lastlogin = u.lastlogin;
            if (!u.subscriptions.empty()) {
                dash.plan = u.subscriptions.front().name;
                dash.expiryUnix = u.subscriptions.front().expiry;
            }
        }
    }

    if (!ok) {
        log("authentication failed: " + message);
        std::lock_guard<std::mutex> lock(mutex_);
        errorMessage_ = message.empty() ? "authentication failed" : message;
        busy_ = false;
        return;
    }

    // Optional Supabase status + audit logging.
    if (supabase_.configured()) {
        const std::string status = supabase_.fetchStatus();
        if (!status.empty()) {
            dash.status = status;
        }
        std::string err;
        if (!supabase_.logLoginEvent(dash.username, dash.ip, true, &err)) {
            log("supabase log failed: " + err);
        } else {
            log("login event recorded in supabase");
        }
    }

    log("authenticated as " + dash.username);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        dashboard_ = dash;
    }
    authenticated_ = true;
    busy_ = false;
}

void App::runLauncher() {
    if (launcherRunning_) {
        return;
    }
    const std::string command = config_.launcher.command;
    if (command.empty()) {
        log("no launcher command configured (set launcher.command in config)");
        return;
    }
    launcherRunning_ = true;
    log("launching: " + command);
    std::thread([this, command]() {
        std::array<char, 512> buffer{};
        FILE* pipe = POPEN((command + " 2>&1").c_str(), "r");
        if (!pipe) {
            log("failed to start launcher");
            launcherRunning_ = false;
            return;
        }
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
            std::string line(buffer.data());
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }
            log("  " + line);
        }
        PCLOSE(pipe);
        log("launcher finished");
        launcherRunning_ = false;
    }).detach();
}

void App::render() {
    if (authenticated_.exchange(false)) {
        screen_ = Screen::Dashboard;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::color::kBackground);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoNavFocus;
    ImGui::Begin("##root", nullptr, flags);

    if (screen_ == Screen::Login) {
        renderLogin(viewport->WorkSize);
    } else {
        renderDashboard(viewport->WorkSize);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void App::renderLogin(const ImVec2& size) {
    const float cardW = 400.0f;
    const float cardH = 470.0f;
    ImGui::SetCursorPos(ImVec2((size.x - cardW) * 0.5f, (size.y - cardH) * 0.5f));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::color::kPanel);
    ImGui::BeginChild("##loginCard", ImVec2(cardW, cardH), true);

    // Brand.
    ui::pushFont(ui::fontHuge);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kAccentBright);
    ImGui::TextUnformatted(config_.title.c_str());
    ImGui::PopStyleColor();
    if (ui::fontHuge) ImGui::PopFont();

    ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kTextDim);
    ImGui::TextUnformatted(config_.subtitle.c_str());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 14));

    // Mode toggle.
    const float toggleW = (cardW - 48.0f - 8.0f) * 0.5f;
    auto modeButton = [&](const char* text, AuthMode mode) {
        const bool active = authMode_ == mode;
        ImGui::PushStyleColor(ImGuiCol_Button,
                              active ? theme::color::kAccentDim : theme::color::kPanelRaised);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              active ? theme::color::kAccent : theme::color::kPanelRaised);
        if (ImGui::Button(text, ImVec2(toggleW, 34))) {
            authMode_ = mode;
        }
        ImGui::PopStyleColor(2);
    };
    modeButton("ACCOUNT", AuthMode::Account);
    ImGui::SameLine();
    modeButton("LICENSE KEY", AuthMode::License);

    ImGui::Dummy(ImVec2(0, 6));

    const float fieldW = cardW - 48.0f;
    ImGui::PushItemWidth(fieldW);
    if (authMode_ == AuthMode::Account) {
        label("USERNAME");
        ImGui::InputTextWithHint("##user", "username", username_, sizeof(username_));
        ImGui::Dummy(ImVec2(0, 2));
        label("PASSWORD");
        ImGui::InputTextWithHint("##pass", "password", password_, sizeof(password_),
                                 ImGuiInputTextFlags_Password);
    } else {
        label("LICENSE KEY");
        ImGui::InputTextWithHint("##key", "XXXX-XXXX-XXXX-XXXX", licenseKey_, sizeof(licenseKey_));
    }
    ImGui::Dummy(ImVec2(0, 2));
    label("2FA CODE (OPTIONAL)");
    ImGui::InputTextWithHint("##code", "------", code_, sizeof(code_));
    ImGui::PopItemWidth();

    ImGui::Dummy(ImVec2(0, 6));

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!errorMessage_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kError);
            ImGui::TextWrapped("%s", errorMessage_.c_str());
            ImGui::PopStyleColor();
        }
    }

    ImGui::Dummy(ImVec2(0, 4));

    const bool busy = busy_;
    if (busy) {
        ImGui::BeginDisabled();
    }
    ui::pushFont(ui::fontMedium);
    if (ImGui::Button(busy ? "AUTHENTICATING..." : "LOGIN", ImVec2(fieldW, 44))) {
        beginAuth();
    }
    if (ui::fontMedium) ImGui::PopFont();
    if (busy) {
        ImGui::EndDisabled();
    }

    // Footer.
    const bool demo = config_.demoMode || !config_.keyauth.configured();
    ImGui::SetCursorPosY(cardH - 34.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, demo ? theme::color::kAccent : theme::color::kSuccess);
    ImGui::TextUnformatted(demo ? "DEMO MODE - any credentials accepted"
                                : "secured by KeyAuth + Ed25519");
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void App::renderConsole(float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::color::kConsoleBg);
    ImGui::BeginChild("##console", ImVec2(width, height), true);

    label("CMD");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4));

    ui::pushFont(ui::fontRegular);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& line : console_) {
            ImGui::PushStyleColor(ImGuiCol_Text, line.find("fail") != std::string::npos
                                                     ? theme::color::kError
                                                     : theme::color::kText);
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        }
        if (consoleDirty_) {
            ImGui::SetScrollHereY(1.0f);
            consoleDirty_ = false;
        }
    }
    if (ui::fontRegular) ImGui::PopFont();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void App::renderDashboard(const ImVec2& size) {
    Dashboard dash;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        dash = dashboard_;
    }

    const float pad = 24.0f;

    // Header row.
    ui::pushFont(ui::fontLarge);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kAccentBright);
    ImGui::TextUnformatted(config_.title.c_str());
    ImGui::PopStyleColor();
    if (ui::fontLarge) ImGui::PopFont();

    ImGui::SameLine();
    const float logoutW = 110.0f;
    ImGui::SetCursorPosX(size.x - pad - logoutW);
    ImGui::PushStyleColor(ImGuiCol_Button, theme::color::kPanelRaised);
    if (ImGui::Button("LOGOUT", ImVec2(logoutW, 32))) {
        screen_ = Screen::Login;
        password_[0] = '\0';
        log("logged out");
    }
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kTextDim);
    ImGui::Text("signed in as %s", dash.username.empty() ? "unknown" : dash.username.c_str());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 8));

    const float bodyTop = ImGui::GetCursorPosY();
    const float bodyH = size.y - bodyTop - pad;
    const float leftW = (size.x - pad * 2) * 0.42f;
    const float rightW = (size.x - pad * 2) - leftW - 16.0f;

    // Left column: console + run launcher button.
    ImGui::BeginGroup();
    const float buttonH = 52.0f;
    renderConsole(leftW, bodyH - buttonH - 12.0f);
    ImGui::Dummy(ImVec2(0, 2));
    const bool launching = launcherRunning_;
    if (launching) ImGui::BeginDisabled();
    ui::pushFont(ui::fontMedium);
    ImGui::PushStyleColor(ImGuiCol_Button, theme::color::kAccentDim);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::color::kAccent);
    if (ImGui::Button(launching ? "RUNNING..." : config_.launcher.label.c_str(),
                      ImVec2(leftW, buttonH))) {
        runLauncher();
    }
    ImGui::PopStyleColor(2);
    if (ui::fontMedium) ImGui::PopFont();
    if (launching) ImGui::EndDisabled();
    ImGui::EndGroup();

    ImGui::SameLine(0, 16);

    // Right column: status cards.
    ImGui::BeginGroup();
    auto statCard = [&](const char* labelText, const std::string& value, const ImVec4& valueColor,
                        float w, float h) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::color::kPanelRaised);
        ImGui::BeginChild(labelText, ImVec2(w, h), true);
        label(labelText);
        ImGui::Dummy(ImVec2(0, 6));
        ui::pushFont(ui::fontLarge);
        ImGui::PushStyleColor(ImGuiCol_Text, valueColor);
        ImGui::TextWrapped("%s", value.empty() ? "-" : value.c_str());
        ImGui::PopStyleColor();
        if (ui::fontLarge) ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    };

    const float cardGap = 12.0f;
    const float cardW = (rightW - cardGap) * 0.5f;
    const float cardH = 92.0f;

    statCard("STATUS", dash.status, theme::color::kSuccess, cardW, cardH);
    ImGui::SameLine(0, cardGap);
    statCard("PLAN", dash.plan, theme::color::kAccentBright, cardW, cardH);

    const std::string expiry = keyauth::Client::expiryRemaining(dash.expiryUnix);
    const ImVec4 expiryColor =
        expiry == "expired" ? theme::color::kError : theme::color::kText;
    statCard("TIME BEFORE EXPIRE", expiry, expiryColor, cardW, cardH);
    ImGui::SameLine(0, cardGap);
    statCard("LICENSE EXPIRES", keyauth::Client::formatTimestamp(dash.expiryUnix),
             theme::color::kText, cardW, cardH);

    // Account details card fills remaining height.
    const float detailsH = bodyH - (cardH + cardGap) * 2;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::color::kPanel);
    ImGui::BeginChild("##details", ImVec2(rightW, detailsH > 80 ? detailsH : 80), true);
    label("ACCOUNT DETAILS");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4));

    auto detailRow = [&](const char* k, const std::string& v) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kTextDim);
        ImGui::Text("%-14s", k);
        ImGui::PopStyleColor();
        ImGui::SameLine(150);
        ImGui::TextUnformatted(v.empty() ? "-" : v.c_str());
    };
    detailRow("USERNAME", dash.username);
    detailRow("IP ADDRESS", dash.ip);
    detailRow("HWID", dash.hwid);
    detailRow("CREATED", keyauth::Client::formatTimestamp(dash.createdate));
    detailRow("LAST LOGIN", keyauth::Client::formatTimestamp(dash.lastlogin));
    detailRow("MODE", dash.demo ? "demo" : "keyauth");

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::EndGroup();
}
