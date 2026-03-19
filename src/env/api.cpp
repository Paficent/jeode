#include "api.h"
#include "./api/console.h"
#include "./api/file.h"
#include "./api/mod.h"

extern "C" {
#include <lua.h>
}

#include <spdlog/spdlog.h>

void api_register_table(lua_State *L, const LuaApiTable &table) {
	if (!table.name || table.name[0] == '\0') {
		for (size_t i = 0; i < table.count; ++i) {
			lua_pushcfunction(L, table.functions[i].func);
			lua_setglobal(L, table.functions[i].name);
		}
		return;
	}

	lua_newtable(L);
	for (size_t i = 0; i < table.count; ++i) {
		lua_pushcfunction(L, table.functions[i].func);
		lua_setfield(L, -2, table.functions[i].name);
	}
	lua_setglobal(L, table.name);
}

void api_register_all(lua_State *L, const std::string &gameDir) {
	file_api_init(gameDir.c_str());

	api_register_table(L, console_api_table());
	api_register_table(L, file_api_table());
	api_register_table(L, mod_api_table());

	spdlog::debug("[api] Lua api registered");
}
