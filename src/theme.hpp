#pragma once

#include "imgui.h"

namespace theme {

// Dark, rounded "Packet.Online" palette. Tweak these to match a brand theme;
// every widget derives its color from here.
namespace color {
constexpr ImVec4 kBackground = ImVec4(0.055f, 0.063f, 0.086f, 1.00f);  // window fill
constexpr ImVec4 kPanel = ImVec4(0.086f, 0.098f, 0.129f, 1.00f);
constexpr ImVec4 kPanelRaised = ImVec4(0.117f, 0.133f, 0.172f, 1.00f);
constexpr ImVec4 kInputBg = ImVec4(0.075f, 0.086f, 0.114f, 1.00f);
constexpr ImVec4 kAccent = ImVec4(0.486f, 0.396f, 0.988f, 1.00f);      // indigo
constexpr ImVec4 kAccentDim = ImVec4(0.365f, 0.294f, 0.760f, 1.00f);
constexpr ImVec4 kAccentBright = ImVec4(0.631f, 0.557f, 1.000f, 1.00f);
constexpr ImVec4 kText = ImVec4(0.902f, 0.914f, 0.945f, 1.00f);
constexpr ImVec4 kTextDim = ImVec4(0.505f, 0.537f, 0.616f, 1.00f);
constexpr ImVec4 kSuccess = ImVec4(0.298f, 0.808f, 0.529f, 1.00f);
constexpr ImVec4 kError = ImVec4(0.941f, 0.400f, 0.482f, 1.00f);
constexpr ImVec4 kConsoleBg = ImVec4(0.043f, 0.051f, 0.070f, 1.00f);
constexpr ImVec4 kBorder = ImVec4(0.160f, 0.180f, 0.243f, 1.00f);

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
