#pragma once

struct LuaApiTable;

void fs_api_init(const char *gameDir);
const LuaApiTable &fs_api_table();
