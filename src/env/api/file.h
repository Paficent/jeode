#pragma once

struct lua_State;

void file_api_init(lua_State *L, const char *gameDir);
void file_api_build_table(lua_State *L);
void file_api_set_mod_root(const char *path);
void file_api_clear_mod_root();
