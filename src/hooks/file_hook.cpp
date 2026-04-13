// TODO: There is probably a better way of handling this logic and simplifying this file

#include "file_hook.h"

#include <spdlog/spdlog.h>

#include "MinHook.h"
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static const ModLoader *g_loader = nullptr;
static std::wstring g_dataBaseW;
static std::wstring g_datBaseW;
static std::string g_dataBaseA;
static std::string g_datBaseA;
static std::atomic<bool> g_configured{false};

static file_hook_game_ready_cb g_ready_cb = nullptr;
static std::atomic<bool> g_ready_fired{false};
static std::atomic<bool> g_ready_pending{false};

void file_hook_on_game_ready(file_hook_game_ready_cb callback) {
	g_ready_cb = callback;
}

// TODO: Find a better detection method for earlier loading of mods
static void check_loaded(const std::string &rel) {
	if (g_ready_fired.load(std::memory_order_acquire) || !g_ready_cb) return;
	if (g_ready_pending.load(std::memory_order_acquire)) {
		if (!g_ready_fired.exchange(true, std::memory_order_acq_rel)) {
			spdlog::debug("[file_hook] game ready trigger fired on '{}'", rel);
			g_ready_cb();
		}
		return;
	}
	if (rel.find("gfx/menu/bbb_logo_loading_screen.") != std::string::npos) {
		spdlog::debug("[file_hook] loading screen texture detected");
		g_ready_pending.store(true, std::memory_order_release);
	}
}

typedef HANDLE(WINAPI *CreateFileA_t)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE(WINAPI *CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

static CreateFileA_t g_orig_CreateFileA = nullptr;
static CreateFileW_t g_orig_CreateFileW = nullptr;
static void *g_pCreateFileA = nullptr;
static void *g_pCreateFileW = nullptr;

static std::wstring normalize_path(const std::wstring &path) {
	std::wstring out = path;
	std::replace(out.begin(), out.end(), L'/', L'\\');
	for (auto &c : out) c = towlower(c);
	return out;
}

static std::string normalize_path_a(const std::string &path) {
	std::string out = path;
	std::replace(out.begin(), out.end(), '/', '\\');
	for (auto &c : out) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
	return out;
}

static std::string wide_to_utf8(const wchar_t *wide) {
	int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
	if (len <= 0) return {};
	std::string out(len - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide, -1, &out[0], len, nullptr, nullptr);
	return out;
}

static std::wstring utf8_to_wide(const std::string &utf8) {
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
	if (len <= 0) return {};
	std::wstring out(len - 1, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &out[0], len);
	return out;
}

static std::string extract_relative_w(const std::wstring &norm, const std::wstring &base) {
	if (norm.size() <= base.size()) return {};
	std::wstring rel = norm.substr(base.size());
	std::string utf8 = wide_to_utf8(rel.c_str());
	std::replace(utf8.begin(), utf8.end(), '\\', '/');
	return utf8;
}

static std::string extract_relative_a(const std::string &norm, const std::string &base) {
	if (norm.size() <= base.size()) return {};
	std::string rel = norm.substr(base.size());
	std::replace(rel.begin(), rel.end(), '\\', '/');
	return rel;
}

static std::wstring resolve_w(const wchar_t *path) {
	if (!path || path[0] == L'\0') return {};
	std::error_code ec;
	auto abs = fs::absolute(fs::path(path), ec);
	if (ec) return normalize_path(path);
	return normalize_path(abs.wstring());
}

static std::string resolve_a(const char *path) {
	if (!path || path[0] == '\0') return {};
	std::error_code ec;
	auto abs = fs::absolute(fs::path(path), ec);
	if (ec) return normalize_path_a(path);
	return normalize_path_a(abs.string());
}

static const std::string *find_override_w(const std::wstring &abs) {
	if (!g_loader || !g_configured) return nullptr;

	if (!g_dataBaseW.empty() && abs.compare(0, g_dataBaseW.size(), g_dataBaseW) == 0) {
		std::string rel = extract_relative_w(abs, g_dataBaseW);
		if (rel.empty()) return nullptr;
		check_loaded(rel);
		auto &m = g_loader->getAllOverrides();
		auto it = m.find(rel);
		return (it != m.end()) ? &it->second : nullptr;
	}

	if (!g_datBaseW.empty() && abs.compare(0, g_datBaseW.size(), g_datBaseW) == 0) {
		std::string rel = extract_relative_w(abs, g_datBaseW);
		if (rel.empty()) return nullptr;
		auto &m = g_loader->getAllDatOverrides();
		auto it = m.find(rel);
		return (it != m.end()) ? &it->second : nullptr;
	}

	return nullptr;
}

static const std::string *find_override_a(const std::string &abs) {
	if (!g_loader || !g_configured) return nullptr;

	if (!g_dataBaseA.empty() && abs.compare(0, g_dataBaseA.size(), g_dataBaseA) == 0) {
		std::string rel = extract_relative_a(abs, g_dataBaseA);
		if (rel.empty()) return nullptr;
		check_loaded(rel);
		auto &m = g_loader->getAllOverrides();
		auto it = m.find(rel);
		return (it != m.end()) ? &it->second : nullptr;
	}

	if (!g_datBaseA.empty() && abs.compare(0, g_datBaseA.size(), g_datBaseA) == 0) {
		std::string rel = extract_relative_a(abs, g_datBaseA);
		if (rel.empty()) return nullptr;
		auto &m = g_loader->getAllDatOverrides();
		auto it = m.find(rel);
		return (it != m.end()) ? &it->second : nullptr;
	}

	return nullptr;
}

static HANDLE WINAPI hooked_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
										LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
										DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
	if (!lpFileName || !g_configured)
		return g_orig_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
								  dwFlagsAndAttributes, hTemplateFile);

	std::wstring abs = resolve_w(lpFileName);
	const std::string *replacement = find_override_w(abs);

	if (!replacement)
		return g_orig_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
								  dwFlagsAndAttributes, hTemplateFile);

	spdlog::debug("[file_hook] override: {} -> {}", wide_to_utf8(lpFileName), *replacement);
	std::wstring widePath = utf8_to_wide(*replacement);
	return g_orig_CreateFileW(widePath.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes,
							  dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

static HANDLE WINAPI hooked_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
										LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
										DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
	if (!lpFileName || !g_configured)
		return g_orig_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
								  dwFlagsAndAttributes, hTemplateFile);

	std::string abs = resolve_a(lpFileName);
	const std::string *replacement = find_override_a(abs);

	if (!replacement)
		return g_orig_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
								  dwFlagsAndAttributes, hTemplateFile);

	spdlog::debug("[file_hook] override: {} -> {}", lpFileName, *replacement);
	return g_orig_CreateFileA(replacement->c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes,
							  dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

static bool install_hook(void *target, void *detour, void **original, const char *name) {
	if (!target) {
		spdlog::warn("[file_hook] {}: symbol not found, skipping", name);
		return false;
	}
	MH_STATUS s = MH_CreateHook(target, detour, original);
	if (s != MH_OK) {
		spdlog::warn("[file_hook] MH_CreateHook({}) failed: {}", name, MH_StatusToString(s));
		return false;
	}
	s = MH_EnableHook(target);
	if (s != MH_OK) {
		spdlog::warn("[file_hook] MH_EnableHook({}) failed: {}", name, MH_StatusToString(s));
		return false;
	}
	return true;
}

bool file_hook_install() {
	HMODULE hKernel = GetModuleHandleA("kernel32.dll");
	if (!hKernel) {
		spdlog::error("[file_hook] kernel32.dll not found");
		return false;
	}

	g_pCreateFileA = reinterpret_cast<void *>(GetProcAddress(hKernel, "CreateFileA"));
	g_pCreateFileW = reinterpret_cast<void *>(GetProcAddress(hKernel, "CreateFileW"));

	spdlog::debug("[file_hook] resolved CreateFileA={}, CreateFileW={}", g_pCreateFileA, g_pCreateFileW);

	bool ok = true;
	if (g_pCreateFileA)
		ok &= install_hook(g_pCreateFileA, reinterpret_cast<void *>(&hooked_CreateFileA),
						   reinterpret_cast<void **>(&g_orig_CreateFileA), "CreateFileA");
	if (g_pCreateFileW)
		ok &= install_hook(g_pCreateFileW, reinterpret_cast<void *>(&hooked_CreateFileW),
						   reinterpret_cast<void **>(&g_orig_CreateFileW), "CreateFileW");

	spdlog::debug("[file_hook] hooks installed (passthrough until configured)");
	return ok;
}

static std::wstring build_dat_base_path() {
	wchar_t buf[MAX_PATH];
	DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
	if (len == 0 || len >= MAX_PATH) return {};

	std::wstring base(buf);
	size_t sep = base.find_last_of(L'\\');
	if (sep == std::wstring::npos) return {};

	base = base.substr(0, sep) + L"\\LocalLow\\Big Blue Bubble Inc\\My Singing Monsters\\1\\";
	return base;
}

void file_hook_configure(const ModLoader *loader, const fs::path &dllDir) {
	g_loader = loader;

	std::wstring dataDir = (dllDir / "data").wstring() + L"\\";
	g_dataBaseW = normalize_path(dataDir);
	g_dataBaseA = wide_to_utf8(g_dataBaseW.c_str());

	std::wstring datDir = build_dat_base_path();
	if (datDir.empty()) spdlog::warn("[file_hook] could not resolve LocalLow save data path");
	g_datBaseW = normalize_path(datDir);
	g_datBaseA = wide_to_utf8(g_datBaseW.c_str());

	spdlog::debug("[file_hook] data base: '{}'", g_dataBaseA);
	if (!g_datBaseA.empty()) spdlog::debug("[file_hook] dat base: '{}'", g_datBaseA);

	g_configured = true;
	spdlog::info("[file_hook] active with {} asset + {} dat override(s)", loader->getAllOverrides().size(),
				 loader->getAllDatOverrides().size());
}

void file_hook_shutdown() {
	g_configured = false;
	if (g_pCreateFileA) MH_DisableHook(g_pCreateFileA);
	if (g_pCreateFileW) MH_DisableHook(g_pCreateFileW);
	g_loader = nullptr;
}
