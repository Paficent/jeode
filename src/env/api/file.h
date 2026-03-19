#pragma once

struct LuaApiTable;

void file_api_init(const char *gameDir);
const LuaApiTable &file_api_table();

void file_api_set_mod_root(const char *path);
void file_api_clear_mod_root();
