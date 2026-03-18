#pragma once

#include <functional>

extern "C" {
#include "lua.h"
}

void lua_thread_set_state(lua_State *L);
lua_State *lua_thread_get_state();
bool lua_thread_ready();

void lua_thread_queue(std::function<void(lua_State *)> work);
void lua_thread_queue_sync(std::function<void(lua_State *)> work);
void lua_thread_register_global(const char *name, lua_CFunction fn);
