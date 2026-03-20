#include "jeode_native.h"

static const struct JeodeNativeAPI *g_api = NULL;

static int JEODE_CALL native_test(void *L) {
	(void)L;
	if (g_api) g_api->log("native-example", "native_test() called from lua");
	return 0;
}

JEODE_EXPORT int JEODE_CALL jeode_native_init(const struct JeodeNativeAPI *api) {
	if (api->api_version != JEODE_NATIVE_API_VERSION) return 1;

	g_api = api;

	g_api->log("native-example", "native mod loaded");
	g_api->log("native-example", g_api->game_version);
	g_api->log("native-example", g_api->is_lua_ready() ? "lua is ready" : "lua not ready yet");

	g_api->register_global("native_test", native_test);
	g_api->queue_lua("_G.native = {['test']=native_test}; native_test=nil", "=native_example_init");
	g_api->queue_lua("native.test()", "=native_example_test");

	return 0;
}

JEODE_EXPORT void JEODE_CALL jeode_native_shutdown(void) {
	if (g_api) g_api->log("native-example", "native mod shutting down");
}
