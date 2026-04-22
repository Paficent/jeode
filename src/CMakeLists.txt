# Shared Libs
add_library(jeode_common OBJECT
    core/config.cpp
    core/keybind.cpp)
target_include_directories(jeode_common PUBLIC
    ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(jeode_common PUBLIC
    nlohmann_json_hdr spdlog_hdr)
target_compile_options(jeode_common PRIVATE -Wall -Wextra -fno-rtti)


# winhttp.dll
add_library(winhttp_proxy SHARED
    proxy/proxy_main.cpp
    proxy/proxy.cpp
    proxy/updater.cpp
    proxy/stubs_proxy.S
    winhttp.def)

set_target_properties(winhttp_proxy PROPERTIES
    PREFIX ""
    OUTPUT_NAME "winhttp"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_include_directories(jeode_common PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(winhttp_proxy PRIVATE
    jeode_common nlohmann_json_hdr spdlog_hdr
    winhttp wininet bcrypt version)
target_compile_options(winhttp_proxy PRIVATE -Wall -Wextra -fno-rtti)
target_compile_definitions(winhttp_proxy PRIVATE JEODE_VERSION="${JEODE_VERSION}")

# libjeode.dll
add_library(jeode SHARED
    core/main.cpp
    core/log.cpp
    core/overlay.cpp
    env/environment.cpp
    env/sandbox.cpp
    env/api.cpp
    env/api/console.cpp
    env/api/file.cpp
    env/api/mod.cpp
    env/api/jeode.cpp
    env/api/net.cpp
    lua/game_lua.cpp
    lua/thread.cpp
    lua/stubs_lua.S
    hooks/hook_manager.cpp
    hooks/file_hook.cpp
    hooks/scheduler_hook.cpp
    hooks/egl_hook.cpp
    loader/schema.cpp
    loader/mod.cpp
    loader/mod_loader.cpp
    loader/native_mod.cpp
    libjeode.def
)

set_target_properties(jeode PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "jeode"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/jeode")
target_link_libraries(jeode PRIVATE
    jeode_common imgui lua51 minhook
    nlohmann_json_hdr spdlog_hdr
    dwmapi gdi32 imm32 winhttp)
target_compile_options(jeode PRIVATE -Wall -Wextra -fno-rtti)
target_compile_definitions(jeode PRIVATE JEODE_VERSION="${JEODE_VERSION}")
