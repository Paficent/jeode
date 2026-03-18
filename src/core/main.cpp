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
		spdlog::error("[exception] code=0x{:08X} at address={}", code, addr);

		if (ep->ContextRecord) {
			auto *ctx = ep->ContextRecord;
			spdlog::debug("[exception] EAX=0x{:08X} EBX=0x{:08X} ECX=0x{:08X} EDX=0x{:08X}", ctx->Eax, ctx->Ebx,
						  ctx->Ecx, ctx->Edx);
			spdlog::debug("[exception] ESI=0x{:08X} EDI=0x{:08X} EBP=0x{:08X} ESP=0x{:08X}", ctx->Esi, ctx->Edi,
						  ctx->Ebp, ctx->Esp);
			spdlog::debug("[exception] EIP=0x{:08X}", ctx->Eip);
		}

		if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
			ep->ExceptionRecord->NumberParameters >= 2) {
			const char *op = ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "reading" : "writing";
			spdlog::error("[exception] access violation {} address 0x{:08X}", op,
						  (unsigned)ep->ExceptionRecord->ExceptionInformation[1]);
		}

		spdlog::default_logger()->flush();
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI init_thread(LPVOID) {
	log_init(g_config.debug);
	AddVectoredExceptionHandler(1, jeode_crash_handler);
	spdlog::info("[main] jeode initializing (gameDir='{}')", g_gameDir.string());

	overlay_init(g_gameDir / "jeode");

	if (!hooks_init_early()) {
		spdlog::error("[main] early hook setup failed, aborting");
		return 1;
	}

	fs::path modsDir = g_gameDir / "mods";
	spdlog::debug("[main] mods directory: '{}' (exists={})", modsDir.string(), fs::exists(modsDir));

	g_modLoader = std::make_unique<ModLoader>(modsDir);
	g_modLoader->loadMods();
	g_modLoader->resolveDependencies();
	g_modLoader->sortMods();

	spdlog::info("[main] {} mod(s) loaded, {} asset override(s), {} dat override(s)", g_modLoader->getAllMods().size(),
				 g_modLoader->getAllOverrides().size(), g_modLoader->getAllDatOverrides().size());

	if (!game_lua_resolve()) {
		spdlog::error("[main] failed to resolve lua functions, aborting");
		return 1;
	}

	if (!hooks_init(g_modLoader.get(), g_gameDir, &g_config)) {
		spdlog::error("[main] hook setup failed, aborting");
		return 1;
	}

	spdlog::info("[main] initialization complete");
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
		spdlog::info("[main] libjeode unloading");
		log_shutdown();
	}

	return TRUE;
}
