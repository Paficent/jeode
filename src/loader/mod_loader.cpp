#include "mod_loader.h"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace fs = std::filesystem;

ModLoader::ModLoader(const fs::path &modsDirectory) : modsDirectory(modsDirectory) {}

void ModLoader::loadMods() {
	spdlog::debug("[loader] scanning '{}'", modsDirectory.string());
	if (!fs::exists(modsDirectory)) {
		fs::create_directories(modsDirectory);
		spdlog::info("[loader] created mods directory: {}", modsDirectory.string());
	}

	int dirCount = 0;
	for (auto &entry : fs::directory_iterator(modsDirectory)) {
		if (!entry.is_directory()) continue;

		dirCount++;
		try {
			auto mod = std::make_shared<Mod>(entry.path());
			if (!mod->isEnabled()) {
				spdlog::info("[loader] '{}' is disabled, skipping", mod->getId());
				continue;
			}
			if (modMap.count(mod->getId())) {
				spdlog::warn("[loader] duplicate mod id '{}' at '{}', skipping", mod->getId(), entry.path().string());
				continue;
			}
			modMap[mod->getId()] = mod;
			mods.push_back(mod);
			spdlog::info("[loader] loaded '{}' ({}) [{} overrides, {} dat overrides]", mod->getId(), mod->getName(),
						 mod->getOverrides().size(), mod->getDatOverrides().size());
		} catch (const std::exception &e) {
			spdlog::warn("[loader] failed to load mod at '{}': {}", entry.path().string(), e.what());
		}
	}
	spdlog::info("[loader] scanned {} directories, loaded {} mod(s)", dirCount, mods.size());
}

void ModLoader::resolveDependencies() {
	spdlog::debug("[loader] resolving dependencies for {} mod(s)", mods.size());
	for (auto &mod : mods) {
		mod->calculateDepth(modMap);
	}
}

void ModLoader::sortMods() {
	std::sort(mods.begin(), mods.end(), [](const auto &a, const auto &b) {
		if (a->getDepth() != b->getDepth()) return a->getDepth() < b->getDepth();
		if (a->getLoadPriority() != b->getLoadPriority()) return a->getLoadPriority() > b->getLoadPriority();
		return a->getId() < b->getId();
	});

	spdlog::info("[loader] load order:");
	for (auto &mod : mods) {
		spdlog::info("[loader]   {} (depth={}, priority={})", mod->getId(), mod->getDepth(), mod->getLoadPriority());
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
const OverrideMap &ModLoader::getAllOverrides() const {
	return allOverrides;
}
const OverrideMap &ModLoader::getAllDatOverrides() const {
	return allDatOverrides;
}

void ModLoader::buildGlobalOverrides() {
	allOverrides.clear();
	allDatOverrides.clear();

	for (auto &mod : mods) {
		for (auto &[gamePath, modPath] : mod->getOverrides()) {
			if (allOverrides.count(gamePath)) {
				spdlog::warn("[loader] override conflict for '{}': '{}' takes precedence", gamePath, mod->getId());
			}
			allOverrides[gamePath] = modPath;
		}
		for (auto &[datPath, modPath] : mod->getDatOverrides()) {
			if (allDatOverrides.count(datPath)) {
				spdlog::warn("[loader] dat override conflict for '{}': '{}' takes precedence", datPath, mod->getId());
			}
			allDatOverrides[datPath] = modPath;
		}
	}

	spdlog::debug("[loader] global overrides built: {} asset, {} dat", allOverrides.size(), allDatOverrides.size());
}
