#include "theme.hpp"

namespace theme {

void apply() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 10.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;

    style.WindowPadding = ImVec2(24, 24);
    style.FramePadding = ImVec2(14, 11);
    style.ItemSpacing = ImVec2(12, 12);
    style.ItemInnerSpacing = ImVec2(8, 8);
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 12.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = color::kBackground;
    colors[ImGuiCol_ChildBg] = color::kPanel;
    colors[ImGuiCol_PopupBg] = color::kPanelRaised;
    colors[ImGuiCol_Text] = color::kText;
    colors[ImGuiCol_TextDisabled] = color::kTextDim;
    colors[ImGuiCol_Border] = ImVec4(0.18f, 0.16f, 0.26f, 1.00f);

    colors[ImGuiCol_FrameBg] = color::kPanelRaised;
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.145f, 0.129f, 0.208f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.176f, 0.153f, 0.259f, 1.00f);

    colors[ImGuiCol_Button] = color::kAccentDim;
    colors[ImGuiCol_ButtonHovered] = color::kAccent;
    colors[ImGuiCol_ButtonActive] = color::kAccentBright;

    colors[ImGuiCol_CheckMark] = color::kAccentBright;
    colors[ImGuiCol_SliderGrab] = color::kAccent;
    colors[ImGuiCol_SliderGrabActive] = color::kAccentBright;

    colors[ImGuiCol_Header] = color::kAccentDim;
    colors[ImGuiCol_HeaderHovered] = color::kAccent;
    colors[ImGuiCol_HeaderActive] = color::kAccentBright;

    colors[ImGuiCol_Separator] = ImVec4(0.18f, 0.16f, 0.26f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = color::kAccent;
    colors[ImGuiCol_SeparatorActive] = color::kAccentBright;

    colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrab] = color::kAccentDim;
    colors[ImGuiCol_ScrollbarGrabHovered] = color::kAccent;
    colors[ImGuiCol_ScrollbarGrabActive] = color::kAccentBright;

    colors[ImGuiCol_TitleBg] = color::kBackground;
    colors[ImGuiCol_TitleBgActive] = color::kBackground;
    colors[ImGuiCol_TitleBgCollapsed] = color::kBackground;
}

}  // namespace theme
