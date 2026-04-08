#pragma once

#include "schema.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Mod {
  public:
	explicit Mod(const std::filesystem::path &modPath);

	const std::string &getId() const;
	const std::string &getName() const;
	bool isEnabled() const;
	int getLoadPriority() const;
	int getDepth() const;
	const Manifest &getManifest() const;
	const std::filesystem::path &getPath() const;
	const std::unordered_map<std::string, std::string> &getOverrides() const;
	const std::unordered_map<std::string, std::string> &getDatOverrides() const;

	void calculateDepth(const std::unordered_map<std::string, std::shared_ptr<Mod>> &modMap);
	void saveManifest();

  private:
	std::filesystem::path path;
	Manifest manifest;
	int depth = 0;
	std::unordered_map<std::string, std::string> resolvedOverrides;
	std::unordered_map<std::string, std::string> resolvedDatOverrides;

	void loadManifest();
	void buildOverrides();
};
