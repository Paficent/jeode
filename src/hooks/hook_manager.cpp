#include "hook_manager.h"
#include "../env/environment.h"
#include "../loader/native_mod.h"
#include "../lua/game_lua.h"
#include "../lua/thread.h"
#include "egl_hook.h"
#include "file_hook.h"
#include "scheduler_hook.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <windows.h>

#include "MinHook.h"

typedef int(__cdecl *luaopen_func_t)(void *L);
static luaopen_func_t g_orig_luaopen_game = nullptr;
static const ModLoader *g_modLoader = nullptr;
static JeodeConfig g_config;
static std::filesystem::path g_gameDir;

static void on_game_ready() {
	spdlog::info("[hooks] game ready, preparing mod environment");
	lua_State *L = lua_thread_get_state();
	if (!L || !g_modLoader) {
		spdlog::warn("[hooks] game ready but lua state or mod loader unavailable (L={}, loader={})", (void *)L,
					 (void *)g_modLoader);
		return;
	}

	const ModLoader *loader = g_modLoader;
	bool nativeEnabled = g_config.enable_native_mods;
	std::filesystem::path gameDir = g_gameDir;
	JeodeConfig config = g_config;

	spdlog::debug("[hooks] queuing native mod load (nativeEnabled={}) and environment init", nativeEnabled);
	scheduler_queue_work([loader, nativeEnabled]() { native_mods_load(loader->getAllMods(), nativeEnabled); });

	scheduler_queue_work([L, loader, gameDir, config]() { get_environment().init(L, loader, gameDir, config); });
}

static int __cdecl hooked_luaopen_game(void *L) {
	spdlog::debug("[hooks] hooked_luaopen_game called, L={}", L);
	int result = g_orig_luaopen_game(L);
	lua_thread_set_state(reinterpret_cast<lua_State *>(L));
	spdlog::debug("[hooks] lua thread state captured");
	return result;
}

bool hooks_init_early() {
	MH_STATUS s = MH_Initialize();
	if (s != MH_OK) {
		spdlog::error("[hooks] MinHook init failed: {}", MH_StatusToString(s));
		return false;
	}
	spdlog::debug("[hooks] MinHook initialized");

	bool file_ok = file_hook_install();
	bool sched_ok = scheduler_hook_install();
	bool egl_ok = egl_hook_install();

	if (!file_ok) spdlog::warn("[hooks] file hook install failed (non-fatal)");
	if (!sched_ok) spdlog::warn("[hooks] scheduler hook install failed (non-fatal)");
	if (!egl_ok) spdlog::warn("[hooks] EGL hook install failed (non-fatal)");

	spdlog::info("[hooks] early hooks installed (file={}, scheduler={}, egl={})", file_ok, sched_ok, egl_ok);
	return true;
}

bool hooks_init(const ModLoader *loader, const std::filesystem::path &dllDir, JeodeConfig *cfg) {
	file_hook_configure(loader, dllDir);
	g_modLoader = loader;
	g_config = *cfg;
	g_gameDir = dllDir;

	file_hook_on_game_ready(on_game_ready);

	HMODULE hExe = GetModuleHandle(nullptr);
	void *luaopen_addr = reinterpret_cast<void *>(GetProcAddress(hExe, "luaopen_game"));

	if (!luaopen_addr) {
		spdlog::error("[hooks] 'luaopen_game' export not found");
		return false;
	}
	spdlog::debug("[hooks] found luaopen_game at {}", luaopen_addr);

	MH_STATUS s = MH_CreateHook(luaopen_addr, reinterpret_cast<void *>(&hooked_luaopen_game),
								reinterpret_cast<void **>(&g_orig_luaopen_game));
	if (s != MH_OK) {
		spdlog::error("[hooks] MH_CreateHook(luaopen_game) failed: {}", MH_StatusToString(s));
		return false;
	}

	s = MH_EnableHook(luaopen_addr);
	if (s != MH_OK) {
		spdlog::error("[hooks] MH_EnableHook(luaopen_game) failed: {}", MH_StatusToString(s));
		return false;
	}
	spdlog::info("[hooks] luaopen_game hook active");

	egl_hook_configure(g_config.overlays_enabled, g_config.toggle_key);
	spdlog::debug("[hooks] EGL configured (overlays={}, toggle=0x{:02X})", g_config.overlays_enabled,
				  g_config.toggle_key);

	spdlog::info("[hooks] hook setup complete");
	return true;
}

void hooks_shutdown() {
	native_mods_unload();
	egl_hook_shutdown();
	file_hook_shutdown();
	scheduler_hook_shutdown();
	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
}
