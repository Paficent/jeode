#include "core/config.h"
#include "core/version.h"
#include "proxy/proxy.h"
#include "proxy/updater.h"

#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

typedef bool (*jeode_init_fn)(const JeodeConfig *cfg, const wchar_t *gameDir);

static fs::path get_dll_directory() {
	char buf[MAX_PATH];
	HMODULE hSelf = nullptr;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					   reinterpret_cast<LPCSTR>(&get_dll_directory), &hSelf);
	GetModuleFileNameA(hSelf, buf, MAX_PATH);
	return fs::path(buf).parent_path();
}

static void launch_libjeode(const JeodeConfig &cfg, const fs::path &gameDir) {
	fs::path dllPath = gameDir / "jeode" / "libjeode.dll";
	HMODULE hLib = LoadLibraryW(dllPath.wstring().c_str());
	if (!hLib) return;

	auto init = reinterpret_cast<jeode_init_fn>(reinterpret_cast<void *>(GetProcAddress(hLib, "jeode_init")));
	if (init) {
		std::wstring dirW = gameDir.wstring();
		init(&cfg, dirW.c_str());
	}
}

extern "C" BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hModule);

		if (!proxy_init_winhttp()) return FALSE;

		fs::path gameDir = get_dll_directory();
		fs::path jeodeDir = gameDir / "jeode";
		JeodeConfig cfg = config_load(jeodeDir);

		if (std::string(JEODE_VERSION) != "dev") updater_run(cfg, gameDir);

		launch_libjeode(cfg, gameDir);
	}

	return TRUE;
}
