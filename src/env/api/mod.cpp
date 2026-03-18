#include "mod.h"
#include "../../lua/game_lua.h"
#include "../environment.h"

extern "C" {
#include <lua.h>
}

#include <cstring>

static int l_get_root(lua_State *L) {
	const std::string &root = get_environment().mod_root();
	lua_pushstring(L, root.c_str());
	return 1;
}

static int l_get_id(lua_State *L) {
	const std::string &id = get_environment().mod_id();
	lua_pushstring(L, id.c_str());
	return 1;
}

void mod_api_register(lua_State *L) {
	lua_pushcfunction(L, l_get_root);
	lua_setglobal(L, "__mod_getRoot");
	lua_pushcfunction(L, l_get_id);
	lua_setglobal(L, "__mod_getId");
}

// This is a janky solution
static const char API_BUILD_LUA[] = R"LUA(
mod = {
    getModRoot = __mod_getModRoot,
    getModId   = __mod_getModId,
}
__mod_getRoot = nil
__mod_getId   = nil
)LUA";

void mod_api_build_table(lua_State *L) {
	int base = lua_gettop(L);
	int s = game_luaL_loadbuffer(L, API_BUILD_LUA, static_cast<int>(strlen(API_BUILD_LUA)), "=jeode_api_init");
	if (s == 0) game_lua_pcall(L, 0, 0, 0);

	game_lua_settop(L, base);
}
