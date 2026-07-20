cmake_minimum_required(VERSION 3.20)
project(SpexronDPI VERSION 1.0.0 LANGUAGES CXX)

# Modern C++20 standardını zorunlu tutuyoruz
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# MSVC derleyicisinin standart C++ kütüphanelerini bulabilmesi için EXTENSIONS kapatılmamalıdır
# set(CMAKE_CXX_EXTENSIONS OFF) dosyaların çıkış dizinleri
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(FetchContent)

# 1. spdlog (Loglama)
FetchContent_Declare(
  spdlog
  URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.zip
)
FetchContent_MakeAvailable(spdlog)

# 2. nlohmann_json (Aşama 5 için Kural ve Ayar Motoru)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip
)
FetchContent_MakeAvailable(json)

# 3. GLFW (Arayüz Penceresi)
FetchContent_Declare(
  glfw
  URL https://github.com/glfw/glfw/releases/download/3.3.8/glfw-3.3.8.zip
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 4. Dear ImGui (Grafiksel Arayüz Motoru)
FetchContent_Declare(
  imgui
  URL https://github.com/ocornut/imgui/archive/refs/tags/v1.90.8.zip
)
FetchContent_MakeAvailable(imgui)
# FetchContent_Populate kullanıyoruz çünkü ImGui native CMake projesi değil.
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()


# Kaynak dosyalarımız
set(SOURCES
    src/main.cpp
    src/telemetry/Logger.cpp
    src/utils/Checksum.cpp
    src/parsers/NetworkParser.cpp
    src/capture/WinDivertAdapter.cpp
    src/capture/CaptureWorker.cpp
    src/engine/PacketModifier.cpp
    src/engine/PacketProcessor.cpp
    src/engine/HostManager.cpp
    src/parsers/L7Parser.cpp
    src/rules/RuleEngine.cpp
    src/config/ConfigManager.cpp
    src/telemetry/Statistics.cpp
    src/gui/Dashboard.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/windivert
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0601
)

target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    ws2_32 
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    glfw
    opengl32
    wininet
    ${CMAKE_SOURCE_DIR}/third_party/windivert/WinDivert.lib
)

# Siyah CMD ekranının anlık bile olsa açılıp kapanmasını tamamen engellemek için
# Sistemi CONSOLE yerine WINDOWS alt yapısıyla başlatıyoruz.
if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup" "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
endif()
