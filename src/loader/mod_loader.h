#pragma once

#include "mod.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ModLoader {
  public:
	explicit ModLoader(const std::filesystem::path &modsDirectory);

	void loadMods();
	void resolveDependencies();
	void sortMods();

	std::shared_ptr<Mod> getModById(const std::string &id) const;
	const std::vector<std::shared_ptr<Mod>> &getAllMods() const;
	const std::unordered_map<std::string, std::string> &getAllOverrides() const;
	const std::unordered_map<std::string, std::string> &getAllDatOverrides() const;

  private:
	std::filesystem::path modsDirectory;
	std::vector<std::shared_ptr<Mod>> mods;
	std::unordered_map<std::string, std::shared_ptr<Mod>> modMap;
	std::unordered_map<std::string, std::string> allOverrides;
	std::unordered_map<std::string, std::string> allDatOverrides;

	void buildGlobalOverrides();
};
