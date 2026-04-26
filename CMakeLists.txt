cmake_minimum_required(VERSION 3.20)
project(jeode LANGUAGES C CXX ASM)

if(NOT DEFINED JEODE_VERSION)
    set(JEODE_VERSION "0.0.0.0") # dev
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Os -ffunction-sections -fdata-sections)
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
add_link_options(-static -static-libgcc -static-libstdc++
                 -Wl,--gc-sections -Wl,--strip-all -Wl,--enable-stdcall-fixup)

include(FetchContent)


# Dependencies:
FetchContent_Declare(minhook
  GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
  GIT_TAG        v1.3.4
  GIT_SHALLOW    TRUE)

FetchContent_Declare(imgui # TODO: patching may not be neccesary if hooked at a different point
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
      GIT_TAG        v1.92.6
      GIT_SHALLOW    TRUE
      PATCH_COMMAND  sh -c "patch -p3 --forward -i '${CMAKE_SOURCE_DIR}/cmake/imgui_angle_loader.patch' || true")

FetchContent_Declare(imgui_coloredit # TODO: this library has so many flaws & I plan to move overlay to a seperate mod
  GIT_REPOSITORY https://github.com/BalazsJako/ImGuiColorTextEdit.git
  GIT_TAG        master
  GIT_SHALLOW    TRUE)

FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip)

FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.17.0
  GIT_SHALLOW    TRUE)

FetchContent_Declare(lua
  URL https://www.lua.org/ftp/lua-5.1.4.tar.gz)

FetchContent_MakeAvailable(
  minhook imgui imgui_coloredit nlohmann_json spdlog lua)


# Lua 5.1.4 - no cmake
file(GLOB LUA_SRCS CONFIGURE_DEPENDS
     ${lua_SOURCE_DIR}/src/*.c)
list(FILTER LUA_SRCS EXCLUDE REGEX "(lua\\.c|luac\\.c)$") # no interpreter/compiler
add_library(lua51 STATIC ${LUA_SRCS})
target_include_directories(lua51 PUBLIC ${lua_SOURCE_DIR}/src)
target_compile_options(lua51 PRIVATE -w)


# ImGui & ColorTextEdit
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_coloredit_SOURCE_DIR}/TextEditor.cpp)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_coloredit_SOURCE_DIR})
target_compile_options(imgui PRIVATE -w -fno-rtti)


# Header-only libs
add_library(nlohmann_json_hdr INTERFACE)
target_include_directories(nlohmann_json_hdr INTERFACE
    ${nlohmann_json_SOURCE_DIR}/include)

add_library(spdlog_hdr INTERFACE)
target_include_directories(spdlog_hdr INTERFACE
    ${spdlog_SOURCE_DIR}/include)

add_subdirectory(src)

include(cmake/package.cmake) # NSIS
