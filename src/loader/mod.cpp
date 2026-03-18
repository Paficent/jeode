#include "mod.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string lowercase(const std::string &s) {
	std::string out = s;
	std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
	return out;
}

Mod::Mod(const fs::path &modPath) : path(modPath) {
	spdlog::debug("[mod] constructing Mod for '{}'", modPath.string());
	spdlog::debug("[mod] '{}': loading manifest...", modPath.filename().string());
	loadManifest();
	spdlog::debug("[mod] '{}': manifest loaded (id='{}'), building overrides...", modPath.filename().string(),
				  manifest.id);
	buildOverrides();
	spdlog::debug("[mod] '{}': construction complete", manifest.id);
}

const std::string &Mod::getId() const {
	return manifest.id;
}
const std::string &Mod::getName() const {
	return manifest.name;
}
int Mod::getLoadPriority() const {
	return manifest.load_priority;
}
int Mod::getDepth() const {
	return depth;
}
const Manifest &Mod::getManifest() const {
	return manifest;
}
const fs::path &Mod::getPath() const {
	return path;
}

const std::unordered_map<std::string, std::string> &Mod::getOverrides() const {
	return resolvedOverrides;
}

const std::unordered_map<std::string, std::string> &Mod::getDatOverrides() const {
	return resolvedDatOverrides;
}

void Mod::loadManifest() {
	std::string dirName = path.filename().string();
	fs::path manifestPath = path / "manifest.json";
	bool rewrite = false;

	spdlog::debug("[mod] '{}': manifest path='{}'", dirName, manifestPath.string());
	spdlog::debug("[mod] '{}': manifest exists={}", dirName, fs::exists(manifestPath));

	Manifest defaults;
	defaults.id = dirName;

	if (fs::exists(manifestPath)) {
		try {
			std::ifstream file(manifestPath);
			json j = json::parse(file);

			json defaultJson;
			defaultJson["id"] = defaults.id;
			defaultJson["name"] = defaults.name;
			defaultJson["author"] = defaults.author;
			defaultJson["version"] = defaults.version;
			defaultJson["game_version"] = defaults.game_version;
			defaultJson["error_on_game_update"] = defaults.error_on_game_update;
			defaultJson["entry"] = defaults.entry;
			defaultJson["load_priority"] = defaults.load_priority;
			defaultJson["dependencies"] = json::array();
			defaultJson["assets"]["auto_override"] = defaults.assets.auto_override;
			defaultJson["assets"]["overrides"] = json::object();
			defaultJson["assets"]["dat_overrides"] = json::object();
			defaultJson["schema_version"] = defaults.schema_version;

			for (auto &[key, value] : defaultJson.items()) {
				if (!j.contains(key)) {
					j[key] = value;
					rewrite = true;
				}
			}

			manifest = schema::parse(j, dirName);
		} catch (const json::exception &e) {
			spdlog::debug("[mod] failed to parse manifest.json for '{}': {}", dirName, e.what());
			manifest = defaults;
			rewrite = true;
		}
	} else {
		manifest = defaults;
		rewrite = true;
	}

	spdlog::debug("[mod] '{}': sanitizing manifest...", dirName);
	schema::sanitize(manifest);
	spdlog::debug("[mod] '{}': sanitized (id='{}', entry='{}', auto_override={})", dirName, manifest.id, manifest.entry,
				  manifest.assets.auto_override);
	spdlog::debug("[mod] '{}': validating manifest...", dirName);
	schema::validate(manifest, path);

	if (rewrite) {
		spdlog::debug("[mod] '{}': rewriting manifest.json...", dirName);
		saveManifest();
		spdlog::debug("[mod] '{}': manifest rewritten", dirName);
	}
}

void Mod::saveManifest() {
	fs::path manifestPath = path / "manifest.json";
	try {
		std::ofstream file(manifestPath);
		file << schema::format(manifest);
	} catch (const std::exception &e) {
		spdlog::debug("[mod] failed to write manifest.json for '{}': {}", manifest.id, e.what());
	}
}

void Mod::buildOverrides() {
	resolvedOverrides.clear();
	resolvedDatOverrides.clear();

	spdlog::debug("[mod] '{}': buildOverrides auto_override={}", manifest.id, manifest.assets.auto_override);

	if (manifest.assets.auto_override) {
		fs::path dataDir = path / "data";
		spdlog::debug("[mod] '{}': checking data dir '{}'", manifest.id, dataDir.string());
		if (fs::exists(dataDir) && fs::is_directory(dataDir)) {
			int count = 0;
			for (auto &entry : fs::recursive_directory_iterator(dataDir)) {
				if (!entry.is_regular_file()) continue;
				fs::path rel = fs::relative(entry.path(), dataDir);
				resolvedOverrides[lowercase(rel.generic_string())] = entry.path().generic_string();
				count++;
			}
			spdlog::debug("[mod] '{}': found {} data file(s) for auto_override", manifest.id, count);
		} else {
			spdlog::debug("[mod] '{}': data dir does not exist", manifest.id);
		}

		fs::path datDir = path / "dat";
		spdlog::debug("[mod] '{}': checking dat dir '{}'", manifest.id, datDir.string());
		if (fs::exists(datDir) && fs::is_directory(datDir)) {
			int count = 0;
			for (auto &entry : fs::recursive_directory_iterator(datDir)) {
				if (!entry.is_regular_file()) continue;
				fs::path rel = fs::relative(entry.path(), datDir);
				resolvedDatOverrides[lowercase(rel.generic_string())] = entry.path().generic_string();
				count++;
			}
			spdlog::debug("[mod] '{}': found {} dat file(s) for auto_override", manifest.id, count);
		} else {
			spdlog::debug("[mod] '{}': dat dir does not exist", manifest.id);
		}
	}

	spdlog::debug("[mod] '{}': processing {} manual overrides...", manifest.id, manifest.assets.overrides.size());
	for (auto &[gamePath, modFile] : manifest.assets.overrides) {
		fs::path src = path / modFile;
		if (fs::exists(src)) {
			resolvedOverrides[lowercase(gamePath)] = src.generic_string();
		} else {
			spdlog::debug("[mod] '{}': manual override source '{}' not found", manifest.id, src.string());
		}
	}

	spdlog::debug("[mod] '{}': processing {} manual dat overrides...", manifest.id,
				  manifest.assets.dat_overrides.size());
	for (auto &[datPath, modFile] : manifest.assets.dat_overrides) {
		fs::path src = path / modFile;
		if (fs::exists(src)) {
			resolvedDatOverrides[lowercase(datPath)] = src.generic_string();
		} else {
			spdlog::debug("[mod] '{}': manual dat override source '{}' not found", manifest.id, src.string());
		}
	}

	size_t total = resolvedOverrides.size() + resolvedDatOverrides.size();
	spdlog::debug("[mod] '{}': buildOverrides complete ({} asset, {} dat)", manifest.id, resolvedOverrides.size(),
				  resolvedDatOverrides.size());
}

void Mod::calculateDepth(const std::unordered_map<std::string, std::shared_ptr<Mod>> &modMap) {
	std::set<std::string> visiting;
	std::unordered_map<std::string, int> cache;

	struct DepthCalc {
		const std::unordered_map<std::string, std::shared_ptr<Mod>> &modMap;
		std::set<std::string> &visiting;
		std::unordered_map<std::string, int> &cache;

		int operator()(const std::string &modId) {
			if (cache.count(modId)) return cache[modId];
			if (modMap.find(modId) == modMap.end()) return 0;
			if (visiting.count(modId)) return 0;

			visiting.insert(modId);
			int d = 0;
			for (auto &dep : modMap.at(modId)->getManifest().dependencies) {
				int sub = (*this)(dep);
				if (sub + 1 > d) d = sub + 1;
			}
			visiting.erase(modId);
			cache[modId] = d;
			return d;
		}
	};

	DepthCalc calc{modMap, visiting, cache};
	depth = calc(manifest.id);
}
