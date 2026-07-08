#include <cstdio>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include "app.hpp"
#include "config.hpp"
#include "fonts.hpp"
#include "theme.hpp"

namespace ui {
ImFont* fontRegular = nullptr;
ImFont* fontMedium = nullptr;
ImFont* fontLarge = nullptr;
ImFont* fontHuge = nullptr;

bool pushFont(ImFont* font) {
    if (font) {
        ImGui::PushFont(font);
        return true;
    }
    return false;
}
}  // namespace ui

namespace {

const char* firstExistingFont() {
    static const char* candidates[] = {
#if defined(_WIN32)
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
#elif defined(__APPLE__)
        "/System/Library/Fonts/SFNS.ttf",
        "/Library/Fonts/Arial.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
#endif
    };
    for (const char* path : candidates) {
        if (FILE* f = std::fopen(path, "rb")) {
            std::fclose(f);
            return path;
        }
    }
    return nullptr;
}

void loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    const char* fontPath = firstExistingFont();
    if (!fontPath) {
        return;  // fall back to the default bitmap font everywhere
    }
    ui::fontRegular = io.Fonts->AddFontFromFileTTF(fontPath, 16.0f);
    ui::fontMedium = io.Fonts->AddFontFromFileTTF(fontPath, 19.0f);
    ui::fontLarge = io.Fonts->AddFontFromFileTTF(fontPath, 26.0f);
    ui::fontHuge = io.Fonts->AddFontFromFileTTF(fontPath, 40.0f);
}

void glfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

}  // namespace

int main(int argc, char** argv) {
    std::string configPath = "config.json";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            configPath = argv[++i];
        }
    }

    std::string loadError;
    AppConfig config = AppConfig::load(configPath, &loadError);
    if (!loadError.empty()) {
        std::fprintf(stderr, "[config] %s (using defaults / demo mode)\n", loadError.c_str());
    }

    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    const char* glslVersion = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    // Borderless + transparent so we can draw our own rounded chrome and
    // macOS-style traffic lights (the corners show the desktop behind).
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1120, 720, config.title.c_str(), nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // don't write imgui.ini
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    loadFonts();
    theme::apply();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    App app(config);
    app.setWindow(window);

    while (!glfwWindowShouldClose(window) && !app.shouldQuit()) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.render();

        ImGui::Render();
        int fbW = 0;
        int fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        // Transparent clear so the rounded corners reveal the desktop.
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
