// Minimal stand-in "launcher" used to test the Run Launcher button.
// It just prints a few lines with small delays so the in-app console shows
// live streaming output. Point config.json -> launcher.command at the built
// binary (test_launcher / test_launcher.exe).
#include <chrono>
#include <cstdio>
#include <thread>

int main() {
    const char* steps[] = {
        "Packet.Online test launcher starting...",
        "Connecting to CDN edge node...",
        "Authenticating session token... ok",
        "Downloading module [1/2] ................ done",
        "Downloading module [2/2] ................ done",
        "Verifying signatures... ok",
        "Injection complete. Enjoy!",
    };
    for (const char* s : steps) {
        std::printf("%s\n", s);
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
    }
    return 0;
}
