#ifndef JEODE_NATIVE_H
#define JEODE_NATIVE_H

#include <stddef.h>

#define JEODE_NATIVE_API_VERSION 2
#define JEODE_CALL __cdecl
#define JEODE_EXPORT __declspec(dllexport)

typedef void(JEODE_CALL *jeode_log_fn)(const char *tag, const char *message);
typedef void(JEODE_CALL *jeode_queue_lua_fn)(const char *code, const char *chunk_name);
typedef void(JEODE_CALL *jeode_register_global_fn)(const char *name, int(JEODE_CALL *fn)(void *L));
typedef int(JEODE_CALL *jeode_is_lua_ready_fn)(void);

struct JeodeNativeAPI {
	int api_version;
	const char *mod_id;
	const char *mod_path;
	const char *game_version;
	jeode_log_fn log;
	jeode_queue_lua_fn queue_lua;
	jeode_register_global_fn register_global;
	jeode_is_lua_ready_fn is_lua_ready;
};

#endif
