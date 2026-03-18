#pragma once

#include "../core/config.h"
#include "../loader/mod_loader.h"
#include "sandbox.h"

#include <filesystem>
#include <string>

struct lua_State;

class Environment {
  public:
	void init(lua_State *L, const ModLoader *loader, const std::filesystem::path &gameDir, const JeodeConfig &config);

	void execute(const std::string &code);

	Sandbox &sandbox();

	void set_mod_context(const std::string &id, const std::string &root);
	void clear_mod_context();
	const std::string &mod_id() const;
	const std::string &mod_root() const;

  private:
	void register_apis(lua_State *L, const std::string &gameDirStr);
	void load_mods(lua_State *L, const ModLoader *loader, const std::string &activeVersion);

	Sandbox m_sandbox;
	std::string m_mod_id;
	std::string m_mod_root;
};

Environment &get_environment();
