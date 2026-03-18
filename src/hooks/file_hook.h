#pragma once

#include "../loader/mod_loader.h"
#include <filesystem>

typedef void (*file_hook_game_ready_cb)();

bool file_hook_install();
void file_hook_configure(const ModLoader *loader, const std::filesystem::path &dllDir);
void file_hook_shutdown();
void file_hook_on_game_ready(file_hook_game_ready_cb callback);
