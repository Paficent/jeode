#include "mod.h"
#include "../api.h"
#include "../environment.h"

extern "C" {
#include <lua.h>
}

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

static const LuaApiFunction MOD_FUNCTIONS[] = {
	{"getRoot", l_get_root},
	{"getId", l_get_id},
};

static const LuaApiTable MOD_TABLE = {
	"mod",
	MOD_FUNCTIONS,
	sizeof(MOD_FUNCTIONS) / sizeof(MOD_FUNCTIONS[0]),
};

const LuaApiTable &mod_api_table() {
	return MOD_TABLE;
}
