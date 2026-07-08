cmake_minimum_required(VERSION 3.16)
project(purple_loader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- Dependencies ------------------------------------------------------------
# Works with system packages (apt/brew) and with vcpkg on Windows.
find_package(CURL REQUIRED)
find_package(OpenGL REQUIRED)

# GLFW: prefer the CMake config package (vcpkg / find_package), fall back to
# pkg-config on Linux/macOS.
find_package(glfw3 CONFIG QUIET)
if(NOT glfw3_FOUND)
  find_package(glfw3 QUIET)
endif()
if(NOT glfw3_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GLFW REQUIRED glfw3)
endif()

# libsodium: vcpkg ships "unofficial-sodium", system packages ship pkg-config.
find_package(unofficial-sodium CONFIG QUIET)
if(NOT unofficial-sodium_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SODIUM REQUIRED libsodium)
endif()

# ---- Dear ImGui (vendored) ---------------------------------------------------
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
if(glfw3_FOUND)
  target_link_libraries(imgui PUBLIC glfw)
else()
  target_include_directories(imgui PUBLIC ${GLFW_INCLUDE_DIRS})
  target_link_libraries(imgui PUBLIC ${GLFW_LIBRARIES})
endif()

# ---- Application -------------------------------------------------------------
add_executable(purple_loader
  src/main.cpp
  src/app.cpp
  src/theme.cpp
  src/config.cpp
  src/keyauth.cpp
  src/supabase.cpp
  src/http.cpp
  src/hwid.cpp
)

target_include_directories(purple_loader PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/external
)

target_link_libraries(purple_loader PRIVATE
  imgui
  CURL::libcurl
  OpenGL::GL
)

# libsodium link (config package vs pkg-config).
if(unofficial-sodium_FOUND)
  target_link_libraries(purple_loader PRIVATE unofficial-sodium::sodium)
else()
  target_include_directories(purple_loader PRIVATE ${SODIUM_INCLUDE_DIRS})
  target_link_libraries(purple_loader PRIVATE ${SODIUM_LIBRARIES})
endif()

if(glfw3_FOUND)
  target_link_libraries(purple_loader PRIVATE glfw)
else()
  target_link_libraries(purple_loader PRIVATE ${GLFW_LIBRARIES})
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(purple_loader PRIVATE dl pthread)
endif()

if(WIN32)
  # Registry (HWID) + sockets used transitively by curl.
  target_link_libraries(purple_loader PRIVATE advapi32 ws2_32)
endif()

# Copy the example config next to the binary if the user has no config yet.
add_custom_command(TARGET purple_loader POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/config.example.json
          $<TARGET_FILE_DIR:purple_loader>/config.example.json
)
