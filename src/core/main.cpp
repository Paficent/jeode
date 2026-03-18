#include "core/config.h"
#include "core/log.h"
#include "core/overlay.h"
#include "hooks/hook_manager.h"
#include "loader/mod_loader.h"
#include "lua/game_lua.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

static std::unique_ptr<ModLoader> g_modLoader;
static JeodeConfig g_config;
static fs::path g_gameDir;

const ModLoader *get_mod_loader() {
	return g_modLoader.get();
}

static LONG WINAPI jeode_crash_handler(EXCEPTION_POINTERS *ep) {
	if (ep && ep->ExceptionRecord) {
		DWORD code = ep->ExceptionRecord->ExceptionCode;
		void *addr = ep->ExceptionRecord->ExceptionAddress;
		spdlog::critical("[CRASH] exception code=0x{:08X} at address={}", code, addr);

		if (ep->ContextRecord) {
			auto *ctx = ep->ContextRecord;
			spdlog::critical("[CRASH] EAX=0x{:08X} EBX=0x{:08X} ECX=0x{:08X} EDX=0x{:08X}", ctx->Eax, ctx->Ebx,
							 ctx->Ecx, ctx->Edx);
			spdlog::critical("[CRASH] ESI=0x{:08X} EDI=0x{:08X} EBP=0x{:08X} ESP=0x{:08X}", ctx->Esi, ctx->Edi,
							 ctx->Ebp, ctx->Esp);
			spdlog::critical("[CRASH] EIP=0x{:08X}", ctx->Eip);
		}

		if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
			ep->ExceptionRecord->NumberParameters >= 2) {
			const char *op = ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "reading" : "writing";
			spdlog::critical("[CRASH] access violation {} address 0x{:08X}", op,
							 (unsigned)ep->ExceptionRecord->ExceptionInformation[1]);
		}

		spdlog::default_logger()->flush();
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI init_thread(LPVOID) {
	log_init(g_config.debug);
	AddVectoredExceptionHandler(1, jeode_crash_handler);
	spdlog::debug("[main] init_thread started (crash handler registered)");
	spdlog::debug("[main] gameDir='{}'", g_gameDir.string());

	spdlog::debug("[main] initializing overlay...");
	overlay_init(g_gameDir / "jeode");
	spdlog::debug("[main] overlay initialized");

	spdlog::debug("[main] installing early hooks...");
	if (!hooks_init_early()) {
		spdlog::error("Early hook setup failed");
		return 1;
	}
	spdlog::debug("[main] early hooks installed");

	spdlog::info("jeode initializing...");

	fs::path modsDir = g_gameDir / "mods";
	spdlog::debug("[main] mods directory: '{}' (exists={})", modsDir.string(), fs::exists(modsDir));

	spdlog::debug("[main] creating ModLoader...");
	g_modLoader = std::make_unique<ModLoader>(modsDir);

	spdlog::debug("[main] calling loadMods...");
	g_modLoader->loadMods();
	spdlog::debug("[main] loadMods complete, {} mod(s) loaded", g_modLoader->getAllMods().size());

	spdlog::debug("[main] resolving dependencies...");
	g_modLoader->resolveDependencies();
	spdlog::debug("[main] dependencies resolved");

	spdlog::debug("[main] sorting mods...");
	g_modLoader->sortMods();
	spdlog::debug("[main] mods sorted, {} total override(s), {} dat override(s)", g_modLoader->getAllOverrides().size(),
				  g_modLoader->getAllDatOverrides().size());

	spdlog::debug("[main] resolving lua functions...");
	if (!game_lua_resolve()) {
		spdlog::error("Failed to resolve lua functions");
		return 1;
	}
	spdlog::debug("[main] lua functions resolved");

	spdlog::debug("[main] calling hooks_init...");
	if (!hooks_init(g_modLoader.get(), g_gameDir, &g_config)) {
		spdlog::error("Hook setup failed");
		return 1;
	}
	spdlog::debug("[main] hooks_init complete, init_thread done");

	return 0;
}

extern "C" __declspec(dllexport) bool jeode_init(const JeodeConfig *cfg, const wchar_t *gameDir) {
	if (!cfg || !gameDir) return false;

	g_config = *cfg;
	g_gameDir = fs::path(gameDir);

	HANDLE hThread = CreateThread(nullptr, 0, init_thread, nullptr, 0, nullptr);
	if (!hThread) return false;

	CloseHandle(hThread);
	return true;
}

extern "C" BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hModule);
	} else if (reason == DLL_PROCESS_DETACH) {
		hooks_shutdown();
		spdlog::debug("libjeode unloading");
		log_shutdown();
	}

	return TRUE;
}
