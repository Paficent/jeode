#pragma once

#include "mod.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using OverrideMap = std::unordered_map<std::string, std::string>;

class ModLoader {
  public:
	explicit ModLoader(const std::filesystem::path &modsDirectory);

	void loadMods();
	void resolveDependencies();
	void sortMods();

	std::shared_ptr<Mod> getModById(const std::string &id) const;
	const std::vector<std::shared_ptr<Mod>> &getAllMods() const;
	const OverrideMap &getAllOverrides() const;
	const OverrideMap &getAllDatOverrides() const;

  private:
	std::filesystem::path modsDirectory;
	std::vector<std::shared_ptr<Mod>> mods;
	std::unordered_map<std::string, std::shared_ptr<Mod>> modMap;
	OverrideMap allOverrides;
	OverrideMap allDatOverrides;

	void buildGlobalOverrides();
};
