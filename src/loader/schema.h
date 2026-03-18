#pragma once

#include "../core/version.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

struct Manifest {
	std::string id = "unknown";
	std::string name = "Unknown";
	std::string author = "Unknown";
	std::string version = "1.0.0";
	std::string game_version = version::game_version();
	bool error_on_game_update = true;
	std::string entry = "init.lua";
	std::string native_entry;
	int load_priority = 0;
	std::vector<std::string> dependencies;

	struct Assets {
		bool auto_override = true;
		std::unordered_map<std::string, std::string> overrides;
		std::unordered_map<std::string, std::string> dat_overrides;
	} assets;

	int schema_version = 1;
};

namespace schema {

Manifest parse(const nlohmann::json &j, const std::string &dirName);
void sanitize(Manifest &manifest);
bool validate(const Manifest &manifest, const std::filesystem::path &modPath);
std::string format(const Manifest &manifest);

} // namespace schema
