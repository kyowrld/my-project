#include "app.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <functional>
#include <string>
#include <thread>

#include <GLFW/glfw3.h>

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

// Small uppercase dim caption used above fields/cards.
void caption(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kTextDim);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

// "13th July, 2126" from a unix timestamp string.
std::string prettyDate(const std::string& unixStr) {
    if (unixStr.empty()) return "-";
    long long t = 0;
    try {
        t = std::stoll(unixStr);
    } catch (...) {
        return "-";
    }
    if (t <= 0) return "-";
    const std::time_t tt = static_cast<std::time_t>(t);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    static const char* kMonths[] = {"January", "February", "March",     "April",
                                    "May",     "June",     "July",      "August",
                                    "September", "October", "November", "December"};
    const int day = tm.tm_mday;
    const char* suffix = "th";
    if (day < 11 || day > 13) {
        switch (day % 10) {
            case 1: suffix = "st"; break;
            case 2: suffix = "nd"; break;
            case 3: suffix = "rd"; break;
            default: break;
        }
    }
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%d%s %s, %d", day, suffix, kMonths[tm.tm_mon],
                  1900 + tm.tm_year);
    return buf;
}

// Deterministic PACKET-xxxxx-xxxxx-xxxxx-xxxxx demo key derived from a seed.
std::string demoLicenseKey(const std::string& seed) {
    static const char* kHex = "0123456789ABCDEF";
    std::uint64_t h = std::hash<std::string>{}(seed.empty() ? "demo" : seed);
    auto block = [&](int n) {
        std::string s;
        for (int i = 0; i < n; ++i) {
            s += kHex[h & 0xF];
            h = h * 6364136223846793005ULL + 1442695040888963407ULL;
        }
        return s;
    };
    return "PACKET-" + block(5) + "-" + block(5) + "-" + block(5) + "-" + block(5);
}

// A circular "refresh" glyph drawn into the given draw list.
void drawRefreshIcon(ImDrawList* dl, ImVec2 center, float radius, ImU32 col) {
    dl->PathClear();
    dl->PathArcTo(center, radius, 0.7f, 6.0f, 20);
    dl->PathStroke(col, 0, 2.0f);
    // Arrowhead at the arc start.
    const ImVec2 tip(center.x + radius, center.y);
    dl->AddTriangleFilled(ImVec2(tip.x - 4, tip.y - 5), ImVec2(tip.x + 4, tip.y - 5),
                          ImVec2(tip.x + 2, tip.y + 3), col);
}

}  // namespace

App::App(AppConfig config)
    : config_(std::move(config)),
      keyauth_(config_.keyauth),
      supabase_(config_.supabase) {
    log(config_.title + " initialized");
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
        dash.plan = "Lifetime plan";
        dash.licenseKey = mode == AuthMode::License ? field1 : demoLicenseKey(dash.username);
        const long long now = std::chrono::duration_cast<std::chrono::seconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
        dash.expiryUnix = std::to_string(now + 100LL * 365 * 86400);
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
            dash.licenseKey = mode == AuthMode::License ? field1 : std::string();
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

void App::handleWindowDrag(const ImVec2& size) {
    // Drag the borderless window by grabbing the empty part of the top bar.
    // Done with a hover test (not an InvisibleButton) so it never steals clicks
    // from the traffic lights, which are submitted first.
    if (!window_ || fullscreen_) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragging_ = false;
        return;
    }
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 mouse = ImGui::GetMousePos();
    const bool overTitle = mouse.y >= wp.y && mouse.y <= wp.y + 46.0f && mouse.x >= wp.x &&
                           mouse.x <= wp.x + size.x - 70.0f;  // leave the lights alone
    if (overTitle && !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        dragging_ = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        dragging_ = false;
    }
    if (dragging_) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        int wx = 0, wy = 0;
        glfwGetWindowPos(window_, &wx, &wy);
        glfwSetWindowPos(window_, wx + static_cast<int>(delta.x),
                         wy + static_cast<int>(delta.y));
    }
}

void App::renderChrome(const ImVec2& size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();

    // Subtle rounded border around the whole window.
    dl->AddRect(wp, ImVec2(wp.x + size.x, wp.y + size.y), ImGui::GetColorU32(theme::color::kBorder),
                theme::kWindowRadius, 0, 1.5f);

    // Traffic lights, top-right. Order left->right: red, orange, green.
    const float r = 7.0f;
    const float cy = 22.0f;
    const float gap = 22.0f;
    auto light = [&](int indexFromRight, const ImVec4& col, const char* id) -> bool {
        const float cx = size.x - 22.0f - r - indexFromRight * gap;
        ImGui::SetCursorPos(ImVec2(cx - r, cy - r));
        ImGui::InvisibleButton(id, ImVec2(r * 2, r * 2));
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 center(wp.x + cx, wp.y + cy);
        dl->AddCircleFilled(center, hovered ? r + 1.0f : r, ImGui::GetColorU32(col), 24);
        return ImGui::IsItemClicked();
    };
    if (light(0, theme::color::kMaximize, "##max") && window_) {
        if (!fullscreen_) {
            glfwGetWindowPos(window_, &savedX_, &savedY_);
            glfwGetWindowSize(window_, &savedW_, &savedH_);
            GLFWmonitor* mon = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = mon ? glfwGetVideoMode(mon) : nullptr;
            if (mode) {
                glfwSetWindowMonitor(window_, nullptr, 0, 0, mode->width, mode->height, 0);
                fullscreen_ = true;
            }
        } else {
            glfwSetWindowMonitor(window_, nullptr, savedX_, savedY_,
                                 savedW_ ? savedW_ : 1120, savedH_ ? savedH_ : 720, 0);
            fullscreen_ = false;
        }
    }
    if (light(1, theme::color::kMinimize, "##min")) {
        if (window_) glfwIconifyWindow(window_);
    }
    if (light(2, theme::color::kClose, "##close")) {
        quit_ = true;
        if (window_) glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }

    handleWindowDrag(size);
}

void App::render() {
    if (authenticated_.exchange(false)) {
        screen_ = Screen::Dashboard;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::color::kBackground);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, theme::kWindowRadius);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar;
    ImGui::Begin("##root", nullptr, flags);
    ImGui::PopStyleVar(2);  // window-level vars only needed at Begin()

    const ImVec2 size = viewport->Size;
    renderChrome(size);

    if (screen_ == Screen::Login) {
        renderLogin(size);
    } else {
        renderDashboard(size);
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

void App::renderLogin(const ImVec2& size) {
    const bool license = authMode_ == AuthMode::License;
    const float cardW = 380.0f;
    const float cardH = 430.0f;
    ImGui::SetCursorPos(ImVec2((size.x - cardW) * 0.5f, (size.y - cardH) * 0.5f));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::color::kPanel);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28, 26));
    ImGui::BeginChild("##loginCard", ImVec2(cardW, cardH), true);

    ui::pushFont(ui::fontHuge);
    ImGui::TextUnformatted("login");
    if (ui::fontHuge) ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 10));

    const float fieldW = cardW - 56.0f;
    ImGui::PushItemWidth(fieldW);
    if (!license) {
        caption("Username:");
        ImGui::InputTextWithHint("##user", "username", username_, sizeof(username_));
        ImGui::Dummy(ImVec2(0, 4));
        caption("Password:");
        ImGui::InputTextWithHint("##pass", "password", password_, sizeof(password_),
                                 ImGuiInputTextFlags_Password);
    } else {
        caption("License key:");
        ImGui::InputTextWithHint("##key", "PACKET-XXXXX-XXXXX-XXXXX-XXXXX", licenseKey_,
                                 sizeof(licenseKey_));
    }
    ImGui::PopItemWidth();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!errorMessage_.empty()) {
            ImGui::Dummy(ImVec2(0, 2));
            ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kError);
            ImGui::TextWrapped("%s", errorMessage_.c_str());
            ImGui::PopStyleColor();
        }
    }

    ImGui::Dummy(ImVec2(0, 10));

    const bool busy = busy_;
    if (busy) ImGui::BeginDisabled();
    ui::pushFont(ui::fontMedium);
    if (ImGui::Button(busy ? "signing in..." : "sign in", ImVec2(fieldW, 46))) {
        beginAuth();
    }
    if (ui::fontMedium) ImGui::PopFont();
    if (busy) ImGui::EndDisabled();

    ImGui::Dummy(ImVec2(0, 6));

    // Secondary auth-mode toggle.
    ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kAccentBright);
    const char* toggleText = license ? "use account instead" : "use a license key";
    const float tw = ImGui::CalcTextSize(toggleText).x;
    ImGui::SetCursorPosX((cardW - tw) * 0.5f);
    if (ImGui::InvisibleButton("##modetoggle", ImVec2(tw, ImGui::GetTextLineHeight()))) {
        authMode_ = license ? AuthMode::Account : AuthMode::License;
    }
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 mn = ImGui::GetItemRectMin();
        dl->AddText(mn, ImGui::GetColorU32(theme::color::kAccentBright), toggleText);
    }
    ImGui::PopStyleColor();

    // Footer brand pinned to the bottom of the card.
    const char* brand = config_.title.c_str();
    const float bw = ImGui::CalcTextSize(brand).x;
    ImGui::SetCursorPos(ImVec2((cardW - bw) * 0.5f, cardH - 40.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kAccentBright);
    ImGui::TextUnformatted(brand);
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void App::renderConsole(float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::color::kConsoleBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::BeginChild("##console", ImVec2(width, height), true);

    ui::pushFont(ui::fontRegular);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (console_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kTextDim);
            ImGui::TextUnformatted("console output will appear here");
            ImGui::PopStyleColor();
        }
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
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void App::renderDashboard(const ImVec2& size) {
    Dashboard dash;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        dash = dashboard_;
    }

    const float pad = 28.0f;

    // Brand, top-left (traffic lights are drawn top-right by renderChrome).
    ImGui::SetCursorPos(ImVec2(pad, 14.0f));
    ui::pushFont(ui::fontMedium);
    ImGui::TextUnformatted(config_.title.c_str());
    if (ui::fontMedium) ImGui::PopFont();

    const float bodyTop = 64.0f;
    const float bodyH = size.y - bodyTop - pad;
    const float leftW = (size.x - pad * 2) * 0.40f;
    const float rightW = (size.x - pad * 2) - leftW - 24.0f;

    // ---- Left column: welcome + license info -------------------------------
    ImGui::SetCursorPos(ImVec2(pad, bodyTop));
    ImGui::BeginGroup();

    caption("Welcome back,");
    ui::pushFont(ui::fontLarge);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kAccentBright);
    ImGui::Text("\"%s\"", dash.username.empty() ? "User" : dash.username.c_str());
    ImGui::PopStyleColor();
    if (ui::fontLarge) ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kTextDim);
    ImGui::TextUnformatted("INFO");
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Separator, theme::color::kBorder);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 6));

    // A boxed value field (read-only look).
    auto infoBox = [&](const char* label, const std::string& value, const ImVec4& valueColor,
                       bool mono) {
        caption(label);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::color::kInputBg);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
        ImGui::BeginChild(label, ImVec2(leftW, 42), true);
        if (mono) ui::pushFont(ui::fontRegular);
        ImGui::PushStyleColor(ImGuiCol_Text, valueColor);
        ImGui::TextUnformatted(value.empty() ? "-" : value.c_str());
        ImGui::PopStyleColor();
        if (mono && ui::fontRegular) ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 8));
    };

    infoBox("License key", dash.licenseKey, theme::color::kText, true);
    infoBox("License status", dash.plan, theme::color::kSuccess, false);
    infoBox("Expires", prettyDate(dash.expiryUnix), theme::color::kText, false);

    // Status + logout pinned lower in the column.
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kTextDim);
    ImGui::TextUnformatted("Status:");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kSuccess);
    ImGui::TextUnformatted(dash.status.empty() ? "-" : dash.status.c_str());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 4));
    ImGui::PushStyleColor(ImGuiCol_Button, theme::color::kPanelRaised);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::color::kBorder);
    if (ImGui::Button("log out", ImVec2(120, 34))) {
        screen_ = Screen::Login;
        password_[0] = '\0';
        log("logged out");
    }
    ImGui::PopStyleColor(2);
    ImGui::EndGroup();

    // ---- Right column: console panel + run launcher ------------------------
    ImGui::SetCursorPos(ImVec2(pad + leftW + 24.0f, bodyTop));
    ImGui::BeginGroup();

    const float buttonH = 50.0f;
    const float refreshH = 34.0f;
    renderConsole(rightW, bodyH - buttonH - refreshH - 16.0f);

    // Refresh icon row (clears the console).
    ImGui::Dummy(ImVec2(0, 4));
    {
        const float iconBox = refreshH;
        ImGui::SetCursorPosX(pad + leftW + 24.0f + rightW - iconBox);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##refresh", ImVec2(iconBox, iconBox));
        const bool hov = ImGui::IsItemHovered();
        drawRefreshIcon(ImGui::GetWindowDrawList(),
                        ImVec2(p.x + iconBox * 0.5f, p.y + iconBox * 0.5f), 9.0f,
                        ImGui::GetColorU32(hov ? theme::color::kAccentBright
                                               : theme::color::kTextDim));
        if (ImGui::IsItemClicked()) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                console_.clear();
            }
            log("console cleared");
        }
    }

    ImGui::Dummy(ImVec2(0, 4));
    const bool launching = launcherRunning_;
    if (launching) ImGui::BeginDisabled();
    ui::pushFont(ui::fontMedium);
    ImGui::PushStyleColor(ImGuiCol_Button, theme::color::kAccentDim);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::color::kAccent);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    if (ImGui::Button(launching ? "Running..." : config_.launcher.label.c_str(),
                      ImVec2(rightW, buttonH))) {
        runLauncher();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    if (ui::fontMedium) ImGui::PopFont();
    if (launching) ImGui::EndDisabled();

    ImGui::EndGroup();
}
