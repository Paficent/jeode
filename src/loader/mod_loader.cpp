#include "mod_loader.h"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace fs = std::filesystem;

ModLoader::ModLoader(const fs::path &modsDirectory) : modsDirectory(modsDirectory) {}

void ModLoader::loadMods() {
	spdlog::debug("[loader] loadMods: scanning '{}'", modsDirectory.string());
	if (!fs::exists(modsDirectory)) {
		fs::create_directories(modsDirectory);
		spdlog::debug("[loader] created mods directory: {}", modsDirectory.string());
	}

	int dirCount = 0;
	for (auto &entry : fs::directory_iterator(modsDirectory)) {
		if (!entry.is_directory()) {
			spdlog::debug("[loader] skipping non-directory: '{}'", entry.path().filename().string());
			continue;
		}

		dirCount++;
		spdlog::debug("[loader] processing directory: '{}'", entry.path().string());
		try {
			spdlog::debug("[loader] constructing Mod object for '{}'...", entry.path().filename().string());
			auto mod = std::make_shared<Mod>(entry.path());
			spdlog::debug("[loader] Mod object created: id='{}', name='{}'", mod->getId(), mod->getName());
			if (modMap.count(mod->getId())) {
				spdlog::debug("[loader] duplicate mod id '{}', skipping '{}'", mod->getId(), entry.path().string());
				continue;
			}
			modMap[mod->getId()] = mod;
			mods.push_back(mod);
			spdlog::debug("[loader] loaded mod: {} ({}) [{} overrides, {} dat overrides]", mod->getId(), mod->getName(),
						  mod->getOverrides().size(), mod->getDatOverrides().size());
		} catch (const std::exception &e) {
			spdlog::debug("[loader] failed to load mod at '{}': {}", entry.path().string(), e.what());
		}
	}
	spdlog::debug("[loader] loadMods: scanned {} directories, loaded {} mods", dirCount, mods.size());
}

void ModLoader::resolveDependencies() {
	spdlog::debug("[loader] resolveDependencies: processing {} mods", mods.size());
	for (auto &mod : mods) {
		spdlog::debug("[loader] calculating depth for '{}'...", mod->getId());
		mod->calculateDepth(modMap);
		spdlog::debug("[loader] '{}' depth={}", mod->getId(), mod->getDepth());
	}
	spdlog::debug("[loader] resolveDependencies done");
}

void ModLoader::sortMods() {
	spdlog::debug("[loader] sortMods: sorting {} mods", mods.size());
	std::sort(mods.begin(), mods.end(), [](const auto &a, const auto &b) {
		if (a->getDepth() != b->getDepth()) return a->getDepth() < b->getDepth();
		if (a->getLoadPriority() != b->getLoadPriority()) return a->getLoadPriority() > b->getLoadPriority();
		return a->getId() < b->getId();
	});

	spdlog::debug("[loader] mod load order:");
	for (auto &mod : mods) {
		spdlog::debug("[loader]   - {} (depth: {}, priority: {})", mod->getId(), mod->getDepth(),
					  mod->getLoadPriority());
	}

	buildGlobalOverrides();
}

std::shared_ptr<Mod> ModLoader::getModById(const std::string &id) const {
	auto it = modMap.find(id);
	return it != modMap.end() ? it->second : nullptr;
}

const std::vector<std::shared_ptr<Mod>> &ModLoader::getAllMods() const {
	return mods;
}
const std::unordered_map<std::string, std::string> &ModLoader::getAllOverrides() const {
	return allOverrides;
}
const std::unordered_map<std::string, std::string> &ModLoader::getAllDatOverrides() const {
	return allDatOverrides;
}

void ModLoader::buildGlobalOverrides() {
	spdlog::debug("[loader] buildGlobalOverrides: building from {} mods", mods.size());
	allOverrides.clear();
	allDatOverrides.clear();

	for (auto &mod : mods) {
		for (auto &[gamePath, modPath] : mod->getOverrides()) {
			if (allOverrides.count(gamePath)) {
				spdlog::debug("[loader] override conflict for '{}': '{}' overrides previous", gamePath, mod->getId());
			}
			allOverrides[gamePath] = modPath;
		}
		for (auto &[datPath, modPath] : mod->getDatOverrides()) {
			if (allDatOverrides.count(datPath)) {
				spdlog::debug("[loader] dat override conflict for '{}': '{}' overrides previous", datPath,
							  mod->getId());
			}
			allDatOverrides[datPath] = modPath;
		}
	}

	spdlog::debug("[loader] {} asset override(s), {} dat override(s)", allOverrides.size(), allDatOverrides.size());
}
