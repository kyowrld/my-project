#pragma once

#include "imgui.h"

namespace theme {

// Purple / black minimalist palette.
namespace color {
constexpr ImVec4 kBackground = ImVec4(0.043f, 0.039f, 0.063f, 1.00f);  // near-black
constexpr ImVec4 kPanel = ImVec4(0.078f, 0.071f, 0.110f, 1.00f);
constexpr ImVec4 kPanelRaised = ImVec4(0.106f, 0.094f, 0.149f, 1.00f);
constexpr ImVec4 kAccent = ImVec4(0.545f, 0.361f, 0.965f, 1.00f);      // purple
constexpr ImVec4 kAccentDim = ImVec4(0.408f, 0.267f, 0.725f, 1.00f);
constexpr ImVec4 kAccentBright = ImVec4(0.655f, 0.494f, 1.000f, 1.00f);
constexpr ImVec4 kText = ImVec4(0.902f, 0.898f, 0.937f, 1.00f);
constexpr ImVec4 kTextDim = ImVec4(0.549f, 0.541f, 0.616f, 1.00f);
constexpr ImVec4 kSuccess = ImVec4(0.400f, 0.851f, 0.549f, 1.00f);
constexpr ImVec4 kError = ImVec4(0.941f, 0.400f, 0.482f, 1.00f);
constexpr ImVec4 kConsoleBg = ImVec4(0.031f, 0.027f, 0.047f, 1.00f);
}  // namespace color

// Applies the global ImGui style (colors, rounding, spacing).
void apply();

}  // namespace theme
