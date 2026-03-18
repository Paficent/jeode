#pragma once

#include <cstdint>
#include <filesystem>

struct JeodeConfig {
	int64_t last_update_check = 0;
	bool overlays_enabled = true;
	bool debug = false;
	bool enable_native_mods = false;
	bool allow_unsafe_functions = false;
	int toggle_key = 0x70; // F1
};

JeodeConfig config_load(const std::filesystem::path &jeodeDir);
void config_save(const JeodeConfig &cfg, const std::filesystem::path &jeodeDir);
