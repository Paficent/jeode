#pragma once

#include "core/config.h"

#include <filesystem>

void updater_run(JeodeConfig &cfg, const std::filesystem::path &gameDir);
