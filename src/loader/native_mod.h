#pragma once

#include "mod.h"

#include <filesystem>
#include <vector>

#define JEODE_NATIVE_API_VERSION 1
#define JEODE_CALL __cdecl

typedef void(JEODE_CALL *jeode_log_fn)(const char *tag, const char *message);
typedef void(JEODE_CALL *jeode_queue_lua_fn)(const char *code, const char *chunk_name);
typedef void(JEODE_CALL *jeode_register_global_fn)(const char *name, int(JEODE_CALL *fn)(void *L));
typedef int(JEODE_CALL *jeode_is_lua_ready_fn)(void);

struct JeodeNativeAPI {
	int api_version;
	const char *mod_id;
	const char *mod_path;
	jeode_log_fn log;
	jeode_queue_lua_fn queue_lua;
	jeode_register_global_fn register_global;
	jeode_is_lua_ready_fn is_lua_ready;
};

typedef int(JEODE_CALL *jeode_native_init_fn)(const JeodeNativeAPI *api);
typedef void(JEODE_CALL *jeode_native_shutdown_fn)(void);

void native_mods_load(const std::vector<std::shared_ptr<Mod>> &mods, bool enabled);
void native_mods_unload();
