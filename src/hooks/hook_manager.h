#pragma once

#include "../core/config.h"
#include "../loader/mod_loader.h"
#include <filesystem>

bool hooks_init_early();
bool hooks_init(const ModLoader *loader, const std::filesystem::path &dllDir, JeodeConfig *cfg);
void hooks_shutdown();
