#include "jeode.h"
#include "../../core/version.h"
#include "../../loader/mod_loader.h"
#include "../api.h"
#include "../environment.h"
#include "file.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <spdlog/spdlog.h>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static const ModLoader *s_loader = nullptr;

static int l_set_context(lua_State *L) {
	const char *id = luaL_checkstring(L, 1);

	if (!s_loader) return luaL_error(L, "jeode.setContext: loader not available");

	auto mod = s_loader->getModById(id);
	if (!mod) {
		lua_pushboolean(L, 0);
		return 1;
	}

	std::string root = "mods/" + mod->getPath().filename().string();

	get_environment().set_mod_context(mod->getId(), root);
	file_api_set_mod_root(root.c_str());

	lua_pushboolean(L, 1);
	return 1;
}

static int l_clear_context(lua_State *L) {
	(void)L;
	get_environment().clear_mod_context();
	file_api_clear_mod_root();
	return 0;
}

static int l_register_global(lua_State *L) {
	const char *name = luaL_checkstring(L, 1);
	luaL_checkany(L, 2);
	lua_pushvalue(L, 2);
	lua_setglobal(L, name);
	return 0;
}

static int l_get_version(lua_State *L) {
	lua_pushstring(L, JEODE_VERSION);
	return 1;
}

static int l_get_mods(lua_State *L) {
	lua_newtable(L);

	if (!s_loader) return 1;

	const auto &mods = s_loader->getAllMods();
	for (size_t i = 0; i < mods.size(); i++) {
		lua_pushstring(L, mods[i]->getId().c_str());
		lua_rawseti(L, -2, static_cast<int>(i + 1));
	}

	return 1;
}

static int l_get_mod_info(lua_State *L) {
	const char *id = luaL_checkstring(L, 1);

	if (!s_loader) return luaL_error(L, "jeode.getModInfo: loader not available");

	auto mod = s_loader->getModById(id);
	if (!mod) {
		lua_pushnil(L);
		return 1;
	}

	const Manifest &m = mod->getManifest();

	lua_newtable(L);

	lua_pushstring(L, m.id.c_str());
	lua_setfield(L, -2, "id");

	lua_pushstring(L, m.name.c_str());
	lua_setfield(L, -2, "name");

	lua_pushstring(L, m.author.c_str());
	lua_setfield(L, -2, "author");

	lua_pushstring(L, m.version.c_str());
	lua_setfield(L, -2, "version");

	lua_pushstring(L, m.game_version.c_str());
	lua_setfield(L, -2, "gameVersion");

	std::string root = "mods/" + mod->getPath().filename().string();
	lua_pushstring(L, root.c_str());
	lua_setfield(L, -2, "root");

	return 1;
}

void jeode_api_init(const ModLoader *loader) {
	s_loader = loader;
	spdlog::debug("[jeode_api] initialized");
}

static const LuaApiFunction JEODE_FUNCTIONS[] = {
	{"setContext", l_set_context}, {"clearContext", l_clear_context}, {"registerGlobal", l_register_global},
	{"getVersion", l_get_version}, {"getMods", l_get_mods},			  {"getModInfo", l_get_mod_info},
};

static const LuaApiTable JEODE_TABLE = {
	"jeode",
	JEODE_FUNCTIONS,
	sizeof(JEODE_FUNCTIONS) / sizeof(JEODE_FUNCTIONS[0]),
};

const LuaApiTable &jeode_api_table() {
	return JEODE_TABLE;
}
