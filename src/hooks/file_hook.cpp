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
static std::atomic<bool> g_configured{false};

static std::string g_dataBaseA, g_datBaseA;
static std::wstring g_dataBaseW, g_datBaseW;

static file_hook_game_ready_cb g_ready_cb = nullptr;
static std::atomic<bool> g_ready_fired{false};
static std::atomic<bool> g_ready_pending{false};

using OverrideMap = std::unordered_map<std::string, std::string>;

// TODO: this probably isn't neccesary, might be useful to have it as a Lua CB function?
void file_hook_on_game_ready(file_hook_game_ready_cb callback) {
	g_ready_cb = callback;
}

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
		spdlog::debug("[file_hook] loading screen texture detected, arming game-ready trigger");
		g_ready_pending.store(true, std::memory_order_release);
	}
}

typedef FILE *(__cdecl *fopen_t)(const char *, const char *);
typedef FILE *(__cdecl *wfopen_t)(const wchar_t *, const wchar_t *);
typedef int(__cdecl *wfopen_s_t)(FILE **, const wchar_t *, const wchar_t *);
typedef HANDLE(WINAPI *CreateFileA_t)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE(WINAPI *CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

static fopen_t g_orig_fopen = nullptr;
static wfopen_t g_orig_wfopen = nullptr;
static wfopen_s_t g_orig_wfopen_s = nullptr;
static CreateFileA_t g_orig_CreateFileA = nullptr;
static CreateFileW_t g_orig_CreateFileW = nullptr;

static void *g_pfopen = nullptr;
static void *g_pwfopen = nullptr;
static void *g_pwfopen_s = nullptr;
static void *g_pCreateFileA = nullptr;
static void *g_pCreateFileW = nullptr;

// TODO: abstract these into a util file
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

// trait-like system probably isn't neccesary but I've been spending too much time w/ Rust
template <typename CharT> struct PathUtils;

// narrow & ansi
template <> struct PathUtils<char> {
	static constexpr char sep = '\\', sep2 = '/';
	static const std::string &data_base() { return g_dataBaseA; }
	static const std::string &dat_base() { return g_datBaseA; }

	static std::string to_utf8(const std::string &s) { return s; }
	static std::string from_utf8(const std::string &s) { return s; }

	static char lower(char c) { return static_cast<char>(tolower(static_cast<unsigned char>(c))); } // awful
};

// wide & unicode
template <> struct PathUtils<wchar_t> {
	static constexpr wchar_t sep = L'\\', sep2 = L'/';
	static const std::wstring &data_base() { return g_dataBaseW; }
	static const std::wstring &dat_base() { return g_datBaseW; }

	static std::string to_utf8(const std::wstring &s) { return wide_to_utf8(s.c_str()); }
	static std::wstring from_utf8(const std::string &s) { return utf8_to_wide(s); }

	static wchar_t lower(wchar_t c) { return towlower(c); } // I might have to std-ify this
};

// TODO: fine for now but Windows specific
template <typename CharT> static std::basic_string<CharT> normalize(const std::basic_string<CharT> &path) {
	using T = PathUtils<CharT>;
	std::basic_string<CharT> out = path;
	std::replace(out.begin(), out.end(), T::sep2, T::sep);
	for (auto &c : out) c = T::lower(c);
	return out;
}

template <typename CharT> static std::basic_string<CharT> resolve(const CharT *path) {
	if (!path || path[0] == CharT{}) return {};

	std::error_code ec;
	auto abs = fs::absolute(fs::path(path), ec);

	if constexpr (std::is_same_v<CharT, wchar_t>) {
		return normalize<CharT>(ec ? std::wstring(path) : abs.wstring());
	} else {
		return normalize<CharT>(ec ? std::string(path) : abs.string());
	} // who knew cpp could ignore dead branches
}

template <typename CharT>
static std::string utf8_relative(const std::basic_string<CharT> &norm, const std::basic_string<CharT> &base) {
	if (norm.size() <= base.size()) return {};
	auto rel = norm.substr(base.size());
	std::string utf8 = PathUtils<CharT>::to_utf8(rel);
	std::replace(utf8.begin(), utf8.end(), '\\', '/');
	return utf8;
}

template <typename CharT>
static const std::string *match_override(const std::basic_string<CharT> &abs, const std::basic_string<CharT> &base,
										 const OverrideMap &overrides, bool notify) {
	if (base.empty() || abs.compare(0, base.size(), base) != 0) return nullptr;

	std::string rel = utf8_relative(abs, base);
	if (rel.empty()) return nullptr;
	if (notify) check_loaded(rel);

	auto found = overrides.find(rel);
	return (found != overrides.end()) ? &found->second : nullptr;
}

template <typename CharT> static const std::string *find_override(const std::basic_string<CharT> &abs) {
	using T = PathUtils<CharT>;
	if (!g_loader || !g_configured) return nullptr;

	if (auto *r = match_override(abs, T::data_base(), g_loader->getAllOverrides(), true)) return r;
	if (auto *r = match_override(abs, T::dat_base(), g_loader->getAllDatOverrides(), false)) return r;

	return nullptr;
}

template <typename CharT> static std::optional<std::basic_string<CharT>> attempt_redirect(const CharT *filename) {
	if (!filename || !g_configured) return std::nullopt;

	auto abs = resolve(filename);
	const std::string *replacement = find_override(abs);
	if (!replacement) return std::nullopt;

	spdlog::debug("[file_hook] override: {} -> {}", PathUtils<CharT>::to_utf8(std::basic_string<CharT>(filename)),
				  *replacement);

	return PathUtils<CharT>::from_utf8(*replacement);
}

// actual cool stuff below
static FILE *__cdecl hooked_wfopen(const wchar_t *filename, const wchar_t *mode) {
	if (auto redir = attempt_redirect(filename)) return g_orig_wfopen(redir->c_str(), mode);
	return g_orig_wfopen(filename, mode);
}

static int __cdecl hooked_wfopen_s(FILE **pFile, const wchar_t *filename, const wchar_t *mode) {
	if (auto redir = attempt_redirect(filename)) return g_orig_wfopen_s(pFile, redir->c_str(), mode);
	return g_orig_wfopen_s(pFile, filename, mode);
}

static FILE *__cdecl hooked_fopen(const char *filename, const char *mode) {
	if (auto redir = attempt_redirect(filename)) return g_orig_fopen(redir->c_str(), mode);
	return g_orig_fopen(filename, mode);
}

static HANDLE WINAPI hooked_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
										LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
										DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
	if (auto redir = attempt_redirect(lpFileName))
		return g_orig_CreateFileW(redir->c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes,
								  dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	return g_orig_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
							  dwFlagsAndAttributes, hTemplateFile);
}

static HANDLE WINAPI hooked_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
										LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
										DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
	if (auto redir = attempt_redirect(lpFileName))
		return g_orig_CreateFileA(redir->c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes,
								  dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	return g_orig_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
							  dwFlagsAndAttributes, hTemplateFile);
}

static void *resolve_from(const char *mod, const char *func) {
	HMODULE h = GetModuleHandleA(mod);
	return h ? reinterpret_cast<void *>(GetProcAddress(h, func)) : nullptr;
}

static void *resolve_crt(const char *func) {
	void *addr = resolve_from("ucrtbase.dll", func);
	if (!addr) addr = resolve_from("api-ms-win-crt-stdio-l1-1-0.dll", func);
	if (!addr) addr = resolve_from("msvcrt.dll", func);
	return addr;
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
	g_pfopen = resolve_crt("fopen");
	g_pwfopen = resolve_crt("_wfopen");
	g_pwfopen_s = resolve_crt("_wfopen_s");
	g_pCreateFileA = resolve_from("kernel32.dll", "CreateFileA");
	g_pCreateFileW = resolve_from("kernel32.dll", "CreateFileW");

	spdlog::debug("[file_hook] resolved fopen={}, _wfopen={}, _wfopen_s={}", g_pfopen, g_pwfopen, g_pwfopen_s);
	spdlog::debug("[file_hook] resolved CreateFileA={}, CreateFileW={}", g_pCreateFileA, g_pCreateFileW);

	bool ok = true;
	if (g_pwfopen)
		ok &= install_hook(g_pwfopen, reinterpret_cast<void *>(&hooked_wfopen),
						   reinterpret_cast<void **>(&g_orig_wfopen), "_wfopen");
	if (g_pwfopen_s)
		ok &= install_hook(g_pwfopen_s, reinterpret_cast<void *>(&hooked_wfopen_s),
						   reinterpret_cast<void **>(&g_orig_wfopen_s), "_wfopen_s");
	if (g_pfopen)
		ok &= install_hook(g_pfopen, reinterpret_cast<void *>(&hooked_fopen), reinterpret_cast<void **>(&g_orig_fopen),
						   "fopen");
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
	g_dataBaseW = normalize(dataDir);
	g_dataBaseA = wide_to_utf8(g_dataBaseW.c_str());

	std::wstring datDir = build_dat_base_path();
	if (datDir.empty()) spdlog::warn("[file_hook] could not resolve LocalLow save data path");
	g_datBaseW = normalize(datDir);
	g_datBaseA = wide_to_utf8(g_datBaseW.c_str());

	spdlog::debug("[file_hook] data base: '{}'", g_dataBaseA);
	if (!g_datBaseA.empty()) spdlog::debug("[file_hook] dat base: '{}'", g_datBaseA);

	g_configured = true;
	spdlog::info("[file_hook] active with {} asset + {} dat override(s)", loader->getAllOverrides().size(),
				 loader->getAllDatOverrides().size());
}

void file_hook_shutdown() {
	g_configured = false;
	if (g_pwfopen) MH_DisableHook(g_pwfopen);
	if (g_pwfopen_s) MH_DisableHook(g_pwfopen_s);
	if (g_pfopen) MH_DisableHook(g_pfopen);
	if (g_pCreateFileA) MH_DisableHook(g_pCreateFileA);
	if (g_pCreateFileW) MH_DisableHook(g_pCreateFileW);
	g_loader = nullptr;
}
