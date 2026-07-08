#pragma once

#include <string>

namespace hwid {

// Returns a stable, per-machine identifier used as the KeyAuth HWID.
// Cross-platform: Windows uses the MachineGuid registry value, Linux uses
// the machine-id, macOS uses the platform UUID. Falls back to a hostname hash.
std::string get();

}  // namespace hwid
