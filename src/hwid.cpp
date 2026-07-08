#include "hwid.hpp"

#include <array>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace hwid {

namespace {

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) {
        s.pop_back();
    }
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
        ++start;
    }
    return s.substr(start);
}

std::string hashFallback() {
    char host[256] = {0};
#if defined(_WIN32)
    DWORD size = sizeof(host);
    GetComputerNameA(host, &size);
#else
    gethostname(host, sizeof(host) - 1);
#endif
    const std::size_t h = std::hash<std::string>{}(std::string(host) + "purple-loader");
    std::ostringstream oss;
    oss << std::hex << h;
    return oss.str();
}

}  // namespace

std::string get() {
#if defined(_WIN32)
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0,
                      KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        char value[256] = {0};
        DWORD size = sizeof(value);
        DWORD type = 0;
        const LONG rc = RegQueryValueExA(hKey, "MachineGuid", nullptr, &type,
                                         reinterpret_cast<LPBYTE>(value), &size);
        RegCloseKey(hKey);
        if (rc == ERROR_SUCCESS && type == REG_SZ) {
            return trim(std::string(value));
        }
    }
    return hashFallback();
#elif defined(__APPLE__)
    std::array<char, 256> buffer{};
    std::string result;
    FILE* pipe = popen(
        "ioreg -rd1 -c IOPlatformExpertDevice | awk -F'\"' '/IOPlatformUUID/{print $4}'",
        "r");
    if (pipe) {
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }
        pclose(pipe);
    }
    result = trim(result);
    return result.empty() ? hashFallback() : result;
#else
    for (const char* path : {"/etc/machine-id", "/var/lib/dbus/machine-id"}) {
        std::ifstream file(path);
        if (file) {
            std::string id;
            std::getline(file, id);
            id = trim(id);
            if (!id.empty()) {
                return id;
            }
        }
    }
    return hashFallback();
#endif
}

}  // namespace hwid
