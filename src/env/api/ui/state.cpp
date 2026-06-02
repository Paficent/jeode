#include "widgets.h"

#include "../../../lua/game_lua.h"

#include <cstring>
#include <spdlog/spdlog.h>

static const char *STATE_MT_KEY = "jeode.ui.state";

static void state_fire_callbacks(lua_State *L, int state_idx) {
	int new_val_idx = lua_gettop(L);
	int abs_state = state_idx > 0 ? state_idx : (new_val_idx + state_idx + 1);

	lua_rawgeti(L, abs_state, 2);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return;
	}
	int cbs_idx = lua_gettop(L);
	int n = static_cast<int>(lua_objlen(L, cbs_idx));
	for (int i = 1; i <= n; i++) {
		lua_rawgeti(L, cbs_idx, i);
		if (lua_isfunction(L, -1)) {
			lua_pushvalue(L, new_val_idx);
			int s = game_lua_pcall(L, 1, 0, 0);
			if (s != 0) {
				const char *err = lua_tostring(L, -1);
				spdlog::error("[ui] state onChange error: {}", err ? err : "(unknown)");
				lua_pop(L, 1);
			}
		} else {
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
}

void ui_state_push_value(lua_State *L, int state_idx) {
	lua_rawgeti(L, state_idx, 1);
}

void ui_state_set_value(lua_State *L, int state_idx) {
	int new_idx = lua_gettop(L);
	int abs_state = state_idx > 0 ? state_idx : (new_idx + state_idx + 1);

	lua_rawgeti(L, abs_state, 1);
	int same = lua_equal(L, new_idx, -1);
	lua_pop(L, 1);

	if (same) {
		lua_pop(L, 1);
		return;
	}

	lua_pushvalue(L, new_idx);
	lua_rawseti(L, abs_state, 1);

	state_fire_callbacks(L, abs_state);
	lua_pop(L, 1);
}

static int state_on_change(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	lua_rawgeti(L, 1, 2);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_rawseti(L, 1, 2);
	}
	int n = static_cast<int>(lua_objlen(L, -1));
	lua_pushvalue(L, 2);
	lua_rawseti(L, -2, n + 1);
	lua_pop(L, 1);
	return 0;
}

static int state_index(lua_State *L) {
	if (lua_isstring(L, 2)) {
		const char *key = lua_tostring(L, 2);
		if (strcmp(key, "value") == 0) {
			lua_rawgeti(L, 1, 1);
			return 1;
		}
		if (strcmp(key, "onChange") == 0) {
			lua_pushcfunction(L, state_on_change);
			return 1;
		}
	}
	lua_pushnil(L);
	return 1;
}

static int state_newindex(lua_State *L) {
	if (lua_isstring(L, 2)) {
		const char *key = lua_tostring(L, 2);
		if (strcmp(key, "value") == 0) {
			lua_pushvalue(L, 3);
			ui_state_set_value(L, 1);
			return 0;
		}
	}
	return 0;
}

static int state_tostring(lua_State *L) {
	lua_rawgeti(L, 1, 1);
	int t = lua_type(L, -1);
	switch (t) {
	case LUA_TNIL:
		lua_pop(L, 1);
		lua_pushliteral(L, "nil");
		return 1;
	case LUA_TBOOLEAN:
		lua_pushstring(L, lua_toboolean(L, -1) ? "true" : "false");
		return 1;
	case LUA_TNUMBER:
	case LUA_TSTRING:
		lua_pushstring(L, lua_tostring(L, -1));
		return 1;
	default: {
		lua_getglobal(L, "tostring");
		if (lua_isfunction(L, -1)) {
			lua_pushvalue(L, -2);
			lua_call(L, 1, 1);
			return 1;
		}
		lua_pop(L, 1);
		lua_pushliteral(L, "");
		return 1;
	}
	}
}

void ui_state_init_metatable(lua_State *L) {
	luaL_getmetatable(L, STATE_MT_KEY);
	if (!lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return;
	}
	lua_pop(L, 1);

	luaL_newmetatable(L, STATE_MT_KEY);

	lua_pushcfunction(L, state_index);
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, state_newindex);
	lua_setfield(L, -2, "__newindex");

	lua_pushcfunction(L, state_tostring);
	lua_setfield(L, -2, "__tostring");

	lua_pushliteral(L, "jeode.ui.state");
	lua_setfield(L, -2, "__metatable");

	lua_pop(L, 1);
}

bool ui_is_state(lua_State *L, int idx) {
	if (!lua_istable(L, idx)) return false;
	if (!lua_getmetatable(L, idx)) return false;
	luaL_getmetatable(L, STATE_MT_KEY);
	bool eq = lua_isnil(L, -1) ? false : (lua_rawequal(L, -1, -2) != 0);
	lua_pop(L, 2);
	return eq;
}

static int l_state(lua_State *L) {
	lua_settop(L, 1);
	if (lua_isnone(L, 1)) lua_pushnil(L);

	ui_state_init_metatable(L);

	lua_newtable(L);

	lua_pushvalue(L, 1);
	lua_rawseti(L, -2, 1);

	lua_newtable(L);
	lua_rawseti(L, -2, 2);

	luaL_getmetatable(L, STATE_MT_KEY);
	lua_setmetatable(L, -2);

	return 1;
}

const LuaApiFunction &ui_state_fn() {
	static const LuaApiFunction fn = {"state", l_state};
	return fn;
}
