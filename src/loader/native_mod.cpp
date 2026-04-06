#include "native_mod.h"
#include "../core/version.h"
#include "../lua/game_lua.h"
#include "../lua/thread.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

struct NativeModContext {
	HMODULE handle = nullptr;
	std::string mod_id;
	std::string mod_path;
	std::string game_ver;
	JeodeNativeAPI api{};
	jeode_native_shutdown_fn shutdown_fn = nullptr;
};

static std::vector<std::unique_ptr<NativeModContext>> g_loaded_mods;
static std::mutex g_modules_mutex;

static void JEODE_CALL native_log(const char *tag, const char *message) {
	std::string t = tag ? tag : "native";
	std::string m = message ? message : "";
	spdlog::info("[{}] {}", t, m);
}

// TODO: this isnt sandboxed
static void JEODE_CALL native_queue_lua(const char *code, const char *chunk_name) {
	if (!code) return;
	std::string src = code;
	std::string name = chunk_name ? chunk_name : "=native_queued";

	lua_thread_queue([src, name](lua_State *L) {
		int base = lua_gettop(L);
		int s = game_luaL_loadbuffer(L, src.c_str(), static_cast<int>(src.size()), name.c_str());
		if (s != 0) {
			const char *err = lua_tostring(L, -1);
			spdlog::error("[native] lua load error: {}", err ? err : "(unknown)");
			game_lua_settop(L, base);
			return;
		}
		s = game_lua_pcall(L, 0, 0, 0);
		if (s != 0) {
			const char *err = lua_tostring(L, -1);
			spdlog::error("[native] lua exec error: {}", err ? err : "(unknown)");
			game_lua_settop(L, base);
			return;
		}
		game_lua_settop(L, base);
	});
}

static void JEODE_CALL native_register_global(const char *name, int(JEODE_CALL *fn)(void *L)) {
	if (!name || !fn) return;
	lua_thread_register_global(name, reinterpret_cast<lua_CFunction>(fn));
}

static int JEODE_CALL native_is_lua_ready() {
	return lua_thread_ready() ? 1 : 0;
}

static bool is_safe_native_entry(const std::string &entry) {
	if (entry.empty()) return false;
	if (entry.find("..") != std::string::npos) return false;
	if (entry.find('/') != std::string::npos) return false;
	if (entry.find('\\') != std::string::npos) return false;
	if (entry.find('\0') != std::string::npos) return false;
	return true;
}

static bool path_is_within(const std::filesystem::path &child, const std::filesystem::path &parent) {
	std::string child_str = child.generic_string();
	std::string parent_str = parent.generic_string();
	if (parent_str.empty()) return false;
	if (parent_str.back() != '/') parent_str += '/';
	return child_str.size() > parent_str.size() && child_str.compare(0, parent_str.size(), parent_str) == 0;
}

static bool load_native_mod(const Mod &mod) {
	const Manifest &manifest = mod.getManifest();

	if (!is_safe_native_entry(manifest.native_entry)) {
		spdlog::error("[native] '{}': invalid native_entry '{}'", manifest.id, manifest.native_entry);
		return false;
	}

	std::filesystem::path dllPath = mod.getPath() / manifest.native_entry;

	std::error_code ec;
	auto canonical = std::filesystem::canonical(dllPath, ec);
	if (ec) {
		spdlog::error("[native] '{}': dll not found at '{}'", manifest.id, dllPath.string());
		return false;
	}

	auto modRoot = std::filesystem::canonical(mod.getPath(), ec);
	if (ec || !path_is_within(canonical, modRoot)) {
		spdlog::error("[native] '{}': native_entry escapes mod directory", manifest.id);
		return false;
	}

	HMODULE handle = LoadLibraryW(canonical.wstring().c_str());
	if (!handle) {
		spdlog::error("[native] '{}': LoadLibrary failed (error {})", manifest.id, GetLastError());
		return false;
	}

	auto init_fn = reinterpret_cast<jeode_native_init_fn>(GetProcAddress(handle, "jeode_native_init"));
	if (!init_fn) {
		spdlog::error("[native] '{}': missing 'jeode_native_init' export", manifest.id);
		FreeLibrary(handle);
		return false;
	}

	auto ctx = std::make_unique<NativeModContext>();
	ctx->handle = handle;
	ctx->mod_id = manifest.id;
	ctx->mod_path = mod.getPath().generic_string();
	ctx->game_ver = version::game_version();
	ctx->shutdown_fn = reinterpret_cast<jeode_native_shutdown_fn>(GetProcAddress(handle, "jeode_native_shutdown"));

	ctx->api.api_version = JEODE_NATIVE_API_VERSION;
	ctx->api.mod_id = ctx->mod_id.c_str();
	ctx->api.mod_path = ctx->mod_path.c_str();
	ctx->api.game_version = ctx->game_ver.c_str();
	ctx->api.log = native_log;
	ctx->api.queue_lua = native_queue_lua;
	ctx->api.register_global = native_register_global;
	ctx->api.is_lua_ready = native_is_lua_ready;

	int result = init_fn(&ctx->api);
	if (result != 0) {
		spdlog::error("[native] '{}': init returned error code {}", manifest.id, result);
		FreeLibrary(handle);
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(g_modules_mutex);
		g_loaded_mods.push_back(std::move(ctx));
	}

	spdlog::info("[native] '{}' loaded successfully", manifest.id);
	return true;
}

void native_mods_load(const std::vector<std::shared_ptr<Mod>> &mods, bool suppressWarnings) {
	std::vector<const Mod *> native_mods;
	for (const auto &mod : mods) {
		const Manifest &manifest = mod->getManifest();
		if (manifest.native_entry.empty()) continue;
		std::filesystem::path dllPath = mod->getPath() / manifest.native_entry;
		if (!std::filesystem::exists(dllPath)) continue;
		native_mods.push_back(mod.get());
	}

	if (native_mods.empty()) {
		spdlog::debug("[native] no native mods found");
		return;
	}

	spdlog::info("[native] {} mod(s) with native entries detected", native_mods.size());

	if (!suppressWarnings) {
		int choice = MessageBoxA(nullptr,
								 "One or more mods use a native DLL.\n\n"
								 "Native DLLs run with full system access and can do anything on your computer. "
								 "Only run mods from trusted sources.\n\n"
								 "Do you wish to load these native mods?",
								 "Jeode - Native Mods", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_TOPMOST);
		if (choice != IDYES) {
			spdlog::info("[native] user declined native mod loading");
			return;
		}
	}

	for (const Mod *mod : native_mods) {
		if (!load_native_mod(*mod)) spdlog::error("[native] [" + mod->getManifest().id + "] failed to load");
	}
}

void native_mods_unload() {
	std::lock_guard<std::mutex> lock(g_modules_mutex);
	for (auto &ctx : g_loaded_mods) {
		if (ctx->shutdown_fn) ctx->shutdown_fn();
	}
	g_loaded_mods.clear();
}
