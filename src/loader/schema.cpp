#include "schema.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string sanitize_id(const std::string &raw) {
	std::string out;
	out.reserve(raw.size());
	for (char c : raw) {
		char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		if ((lc >= 'a' && lc <= 'z') || (lc >= '0' && lc <= '9') || lc == '_' || lc == '-') out += lc;
	}
	return out.empty() ? "unknown" : out;
}

namespace schema {

Manifest parse(const json &j, const std::string &dirName) {
	Manifest m;

	m.id = j.contains("id") && j["id"].is_string() ? j["id"].get<std::string>() : dirName;
	m.name = j.contains("name") && j["name"].is_string() ? j["name"].get<std::string>() : "Unknown";
	m.author = j.contains("author") && j["author"].is_string() ? j["author"].get<std::string>() : "Unknown";
	m.version = j.contains("version") && j["version"].is_string() ? j["version"].get<std::string>() : "1.0.0";
	m.game_version = j.contains("game_version") && j["game_version"].is_string() ? j["game_version"].get<std::string>()
																				 : version::game_version();
	m.entry = j.contains("entry") && j["entry"].is_string() ? j["entry"].get<std::string>() : "init.lua";
	m.native_entry = j.contains("native_entry") && j["native_entry"].is_string() ? j["native_entry"].get<std::string>()
																				 : "";
	m.error_on_game_update = j.contains("error_on_game_update") && j["error_on_game_update"].is_boolean()
								 ? j["error_on_game_update"].get<bool>()
								 : true;
	m.load_priority = j.contains("load_priority") && j["load_priority"].is_number_integer()
						  ? j["load_priority"].get<int>()
						  : 0;
	m.schema_version = j.contains("schema_version") && j["schema_version"].is_number_integer()
						   ? j["schema_version"].get<int>()
						   : 1;

	if (j.contains("dependencies") && j["dependencies"].is_array()) {
		for (auto &dep : j["dependencies"])
			m.dependencies.push_back(dep.is_string() ? dep.get<std::string>() : dep.dump());
	}

	if (j.contains("assets") && j["assets"].is_object()) {
		auto &a = j["assets"];
		m.assets.auto_override = a.contains("auto_override") && a["auto_override"].is_boolean()
									 ? a["auto_override"].get<bool>()
									 : true;
		if (a.contains("overrides") && a["overrides"].is_object()) {
			for (auto &[k, v] : a["overrides"].items())
				m.assets.overrides[k] = v.is_string() ? v.get<std::string>() : v.dump();
		}
		if (a.contains("dat_overrides") && a["dat_overrides"].is_object()) {
			for (auto &[k, v] : a["dat_overrides"].items())
				m.assets.dat_overrides[k] = v.is_string() ? v.get<std::string>() : v.dump();
		}
	}

	return m;
}

void sanitize(Manifest &m) {
	m.id = sanitize_id(m.id);
	if (m.name.empty()) m.name = "Unknown";
	if (m.author.empty()) m.author = "Unknown";
	if (m.version.empty()) m.version = "1.0.0";
	if (m.game_version.empty()) m.game_version = version::game_version();
	if (m.entry.empty()) m.entry = "init.lua";
	if (!m.native_entry.empty()) {
		if (m.native_entry.find("..") != std::string::npos || m.native_entry.find('/') != std::string::npos ||
			m.native_entry.find('\\') != std::string::npos) {
			spdlog::warn("[schema] mod '{}': native_entry '{}' contains path traversal, rejected", m.id,
						 m.native_entry);
			m.native_entry.clear();
		}
	}
}

bool validate(const Manifest &m, const fs::path &modPath) {
	if (m.id.empty() || m.id == "unknown") {
		spdlog::warn("[schema] mod at '{}' has invalid or empty id", modPath.string());
		return false;
	}

	fs::path entryPath = modPath / m.entry;
	if (!fs::exists(entryPath)) spdlog::warn("[schema] mod '{}': entry file '{}' not found", m.id, entryPath.string());

	if (!m.native_entry.empty()) {
		fs::path nativePath = modPath / m.native_entry;
		if (!fs::exists(nativePath))
			spdlog::warn("[schema] mod '{}': native_entry '{}' not found", m.id, nativePath.string());
	}

	for (auto &[gamePath, modFile] : m.assets.overrides) {
		fs::path src = modPath / modFile;
		if (!fs::exists(src)) {
			spdlog::warn("[schema] mod '{}': override source '{}' does not exist", m.id, src.string());
			return false;
		}
	}

	for (auto &[datPath, modFile] : m.assets.dat_overrides) {
		fs::path src = modPath / modFile;
		if (!fs::exists(src)) {
			spdlog::warn("[schema] mod '{}': dat override source '{}' does not exist", m.id, src.string());
			return false;
		}
	}

	return true;
}

std::string format(const Manifest &m) {
	nlohmann::ordered_json deps = nlohmann::ordered_json::array();
	for (auto &d : m.dependencies) deps.push_back(d);

	nlohmann::ordered_json overrides = nlohmann::ordered_json::object();
	for (auto &[k, v] : m.assets.overrides) overrides[k] = v;

	nlohmann::ordered_json dat_overrides = nlohmann::ordered_json::object();
	for (auto &[k, v] : m.assets.dat_overrides) dat_overrides[k] = v;

	nlohmann::ordered_json j;
	j["id"] = m.id;
	j["name"] = m.name;
	j["author"] = m.author;
	j["version"] = m.version;
	j["game_version"] = m.game_version;
	j["error_on_game_update"] = m.error_on_game_update;
	j["entry"] = m.entry;
	if (!m.native_entry.empty()) j["native_entry"] = m.native_entry;
	j["load_priority"] = m.load_priority;
	j["dependencies"] = deps;

	nlohmann::ordered_json assets;
	assets["auto_override"] = m.assets.auto_override;
	assets["overrides"] = overrides;
	assets["dat_overrides"] = dat_overrides;
	j["assets"] = assets;

	j["schema_version"] = m.schema_version;

	return j.dump(2) + "\n";
}

} // namespace schema
