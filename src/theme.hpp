#pragma once

#include "imgui.h"

namespace theme {

// Palette sampled from packet.online: deep purple-black background, vivid
// violet accent (#8858F0), lavender text, neon green status. Every widget
// derives its color from here.
namespace color {
constexpr ImVec4 kBackground = ImVec4(0.043f, 0.031f, 0.078f, 1.00f);  // #0B0814 window fill
constexpr ImVec4 kPanel = ImVec4(0.059f, 0.047f, 0.102f, 1.00f);       // #0F0C1A card
constexpr ImVec4 kPanelRaised = ImVec4(0.090f, 0.082f, 0.133f, 1.00f); // #171522 sub-box
constexpr ImVec4 kInputBg = ImVec4(0.075f, 0.063f, 0.114f, 1.00f);     // #13101D field
constexpr ImVec4 kAccent = ImVec4(0.533f, 0.345f, 0.941f, 1.00f);      // #8858F0 violet
constexpr ImVec4 kAccentDim = ImVec4(0.416f, 0.259f, 0.808f, 1.00f);   // #6A42CE
constexpr ImVec4 kAccentBright = ImVec4(0.627f, 0.502f, 0.973f, 1.00f);// #A080F8
constexpr ImVec4 kText = ImVec4(0.957f, 0.953f, 0.984f, 1.00f);        // #F4F3FB
constexpr ImVec4 kTextDim = ImVec4(0.561f, 0.529f, 0.659f, 1.00f);     // #8F87A8
constexpr ImVec4 kSuccess = ImVec4(0.000f, 0.847f, 0.439f, 1.00f);     // #00D870
constexpr ImVec4 kError = ImVec4(0.941f, 0.400f, 0.482f, 1.00f);
constexpr ImVec4 kConsoleBg = ImVec4(0.020f, 0.012f, 0.043f, 1.00f);   // #05030B
constexpr ImVec4 kBorder = ImVec4(0.141f, 0.094f, 0.267f, 1.00f);      // #241844

// macOS-style window traffic lights.
constexpr ImVec4 kClose = ImVec4(0.996f, 0.373f, 0.341f, 1.00f);       // red
constexpr ImVec4 kMinimize = ImVec4(0.996f, 0.737f, 0.180f, 1.00f);    // orange
constexpr ImVec4 kMaximize = ImVec4(0.157f, 0.784f, 0.251f, 1.00f);    // green
}  // namespace color

// Corner radius of the whole (borderless) window.
constexpr float kWindowRadius = 16.0f;

// Applies the global ImGui style (colors, rounding, spacing).
void apply();

}  // namespace theme
