#pragma once

struct LuaApiTable;
struct lua_State;

void ui_api_init();
const LuaApiTable &ui_api_table();

int ui_register_frame_callback(lua_State *L);
void ui_draw_frame();
