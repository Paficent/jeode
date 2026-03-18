#pragma once

#include <cstdint>

bool game_lua_resolve();

extern "C" {
int game_luaL_loadbuffer(void *L, const char *buf, int sz, const char *name);
int game_lua_pcall(void *L, int nargs, int nresults, int errfunc);
void game_lua_settop(void *L, int index);

void *game_lua_newthread(void *L);
int game_lua_resume(void *from, void *co, int narg);
int game_lua_yield(void *L);
}
