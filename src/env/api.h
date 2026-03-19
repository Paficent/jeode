#pragma once

#include <cstddef>
#include <string>

struct lua_State;

struct LuaApiFunction {
	const char *name;
	int (*func)(lua_State *);
};

struct LuaApiTable {
	const char *name;
	const LuaApiFunction *functions;
	size_t count;
};

void api_register_table(lua_State *L, const LuaApiTable &table);

void api_register_all(lua_State *L, const std::string &gameDir);
