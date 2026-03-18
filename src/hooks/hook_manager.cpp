#include "hook_manager.h"
#include "../env/environment.h"
#include "../loader/native_mod.h"
#include "../lua/game_lua.h"
#include "../lua/thread.h"
#include "egl_hook.h"
#include "file_hook.h"
#include "scheduler_hook.h"
#include "ssl_hook.h"

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
	spdlog::debug("[hooks] on_game_ready fired");
	lua_State *L = lua_thread_get_state();
	if (!L || !g_modLoader) {
		spdlog::debug("[hooks] game ready but lua state ({}) or mod loader ({}) unavailable", (void *)L,
					  (void *)g_modLoader);
		return;
	}

	spdlog::debug("[hooks] L={}, g_modLoader={}", (void *)L, (void *)g_modLoader);

	const ModLoader *loader = g_modLoader;
	bool nativeEnabled = g_config.enable_native_mods;
	std::filesystem::path gameDir = g_gameDir;
	JeodeConfig config = g_config;

	spdlog::debug("[hooks] queuing native_mods_load (nativeEnabled={})...", nativeEnabled);
	scheduler_queue_work([loader, nativeEnabled]() {
		spdlog::debug("[hooks] native_mods_load work item executing...");
		native_mods_load(loader->getAllMods(), nativeEnabled);
		spdlog::debug("[hooks] native_mods_load work item done");
	});

	spdlog::debug("[hooks] queuing environment init...");
	scheduler_queue_work([L, loader, gameDir, config]() {
		spdlog::debug("[hooks] environment init work item executing, L={}...", (void *)L);
		get_environment().init(L, loader, gameDir, config);
		spdlog::debug("[hooks] environment init work item done");
	});

	spdlog::debug("[hooks] on_game_ready: work items queued");
}

static int __cdecl hooked_luaopen_game(void *L) {
	spdlog::debug("[hooks] hooked_luaopen_game called, L={}", L);
	int result = g_orig_luaopen_game(L);
	spdlog::debug("[hooks] original luaopen_game returned {}", result);
	lua_thread_set_state(reinterpret_cast<lua_State *>(L));
	spdlog::debug("[hooks] lua thread state set");
	return result;
}

bool hooks_init_early() {
	spdlog::debug("[hooks] hooks_init_early: initializing MinHook...");
	MH_STATUS s = MH_Initialize();
	if (s != MH_OK) {
		spdlog::error("MinHook init failed: {}", MH_StatusToString(s));
		return false;
	}
	spdlog::debug("[hooks] MinHook initialized");

	spdlog::debug("[hooks] installing file hook...");
	if (!file_hook_install())
		spdlog::debug("File hook install failed (non-fatal)");
	else
		spdlog::debug("[hooks] file hook installed");

	spdlog::debug("[hooks] installing SSL hook...");
	if (!ssl_hook_install())
		spdlog::debug("SSL hook install failed (non-fatal)");
	else
		spdlog::debug("[hooks] SSL hook installed");

	spdlog::debug("[hooks] installing scheduler hook...");
	if (!scheduler_hook_install())
		spdlog::debug("Scheduler hook install failed (non-fatal)");
	else
		spdlog::debug("[hooks] scheduler hook installed");

	spdlog::debug("[hooks] installing EGL hook...");
	if (!egl_hook_install())
		spdlog::debug("EGL hook install failed (non-fatal)");
	else
		spdlog::debug("[hooks] EGL hook installed");

	spdlog::debug("[hooks] hooks_init_early done");
	return true;
}

bool hooks_init(const ModLoader *loader, const std::filesystem::path &dllDir, JeodeConfig *cfg) {
	spdlog::debug("[hooks] hooks_init: configuring file hook...");
	file_hook_configure(loader, dllDir);
	g_modLoader = loader;
	g_config = *cfg;
	g_gameDir = dllDir;

	spdlog::debug("[hooks] hooks_init: setting on_game_ready callback...");
	file_hook_on_game_ready(on_game_ready);

	spdlog::debug("[hooks] hooks_init: looking for luaopen_game...");
	HMODULE hExe = GetModuleHandle(nullptr);
	void *luaopen_addr = reinterpret_cast<void *>(GetProcAddress(hExe, "luaopen_game"));

	if (!luaopen_addr) {
		spdlog::error("'luaopen_game' export not found");
		return false;
	}
	spdlog::debug("Found luaopen_game at {}", luaopen_addr);

	spdlog::debug("[hooks] creating luaopen_game hook...");
	MH_STATUS s = MH_CreateHook(luaopen_addr, reinterpret_cast<void *>(&hooked_luaopen_game),
								reinterpret_cast<void **>(&g_orig_luaopen_game));
	if (s != MH_OK) {
		spdlog::error("MH_CreateHook(luaopen_game) failed: {}", MH_StatusToString(s));
		return false;
	}

	spdlog::debug("[hooks] enabling luaopen_game hook...");
	s = MH_EnableHook(luaopen_addr);
	if (s != MH_OK) {
		spdlog::error("MH_EnableHook(luaopen_game) failed: {}", MH_StatusToString(s));
		return false;
	}
	spdlog::debug("[hooks] luaopen_game hook active");

	spdlog::debug("[hooks] configuring EGL hook (overlays={}, key=0x{:02X})...", g_config.overlays_enabled,
				  g_config.toggle_key);
	egl_hook_configure(g_config.overlays_enabled, g_config.toggle_key);

	spdlog::debug("[hooks] hooks_init complete");
	return true;
}

void hooks_shutdown() {
	native_mods_unload();
	egl_hook_shutdown();
	file_hook_shutdown();
	ssl_hook_shutdown();
	scheduler_hook_shutdown();
	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
}
