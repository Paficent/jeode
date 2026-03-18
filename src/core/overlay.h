#pragma once

#include <filesystem>
#include <string>

void overlay_init(const std::filesystem::path &jeodeDir);
void overlay_draw();

void overlay_log(const std::string &line);
void overlay_debug_log(const std::string &line);
