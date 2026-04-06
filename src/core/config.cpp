#include "core/config.h"
#include "core/keybind.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

static const char *CONFIG_FILENAME = "config.json";

static bool read_bool(const json &j, const char *key, bool fallback) {
	return j.contains(key) && j[key].is_boolean() ? j[key].get<bool>() : fallback;
}

static int64_t read_int64(const json &j, const char *key, int64_t fallback) {
	return j.contains(key) && j[key].is_number_integer() ? j[key].get<int64_t>() : fallback;
}

static std::string read_string(const json &j, const char *key, const std::string &fallback) {
	return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : fallback;
}

JeodeConfig config_load(const fs::path &jeodeDir) {
	JeodeConfig cfg;
	fs::path configPath = jeodeDir / CONFIG_FILENAME;

	if (fs::exists(configPath)) {
		try {
			std::ifstream file(configPath);
			json j = json::parse(file);

			cfg.last_update_check = read_int64(j, "last_update_check", cfg.last_update_check);
			cfg.overlays_enabled = read_bool(j, "overlays_enabled", cfg.overlays_enabled);
			cfg.debug = read_bool(j, "debug", cfg.debug);
			cfg.allow_unsafe_functions = read_bool(j, "allow_unsafe_functions", cfg.allow_unsafe_functions);
			cfg.suppress_native_warnings = read_bool(j, "suppress_native_warnings", cfg.suppress_native_warnings);

			std::string key_name = read_string(j, "toggle_key", keybind_vk_to_name(cfg.toggle_key));
			int vk = keybind_name_to_vk(key_name);
			if (vk != 0) cfg.toggle_key = vk;
		} catch (...) {
		}
	}

	config_save(cfg, jeodeDir);
	return cfg;
}

void config_save(const JeodeConfig &cfg, const fs::path &jeodeDir) {
	fs::create_directories(jeodeDir);

	nlohmann::ordered_json j;
	j["last_update_check"] = cfg.last_update_check;
	j["overlays_enabled"] = cfg.overlays_enabled;
	j["debug"] = cfg.debug;
	j["allow_unsafe_functions"] = cfg.allow_unsafe_functions;
	j["suppress_native_warnings"] = cfg.suppress_native_warnings;

	std::string key_name = keybind_vk_to_name(cfg.toggle_key);
	j["toggle_key"] = key_name.empty() ? "F1" : key_name;

	try {
		std::ofstream file(jeodeDir / CONFIG_FILENAME);
		file << j.dump(2) << "\n";
	} catch (...) {
	}
}
