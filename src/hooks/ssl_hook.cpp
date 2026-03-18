#include "ssl_hook.h"

#include <spdlog/spdlog.h>

#include "MinHook.h"
#include <windows.h>

#include <string>

typedef int(__cdecl *SSL_read_t)(void *ssl, void *buf, int num);
typedef int(__cdecl *SSL_write_t)(void *ssl, const void *buf, int num);

static SSL_read_t g_orig_SSL_read = nullptr;
static SSL_write_t g_orig_SSL_write = nullptr;
static void *g_pSSL_read = nullptr;
static void *g_pSSL_write = nullptr;

// TODO: Actually implement this so Native mods can read packets or seperate this into its own mod
// so I don't get sued for "allowing modders to modify encrypted network traffic"
static std::string hex_dump(const void *data, int len) {
	if (len <= 0) return {};
	const auto *bytes = static_cast<const unsigned char *>(data);
	std::string out;
	out.reserve(len * 3);
	for (int i = 0; i < len; ++i) {
		if (i > 0) out += ' ';
		char buf[4];
		snprintf(buf, sizeof(buf), "%02x", bytes[i]);
		out += buf;
	}
	return out;
}

static int __cdecl hooked_SSL_read(void *ssl, void *buf, int num) {
	int ret = g_orig_SSL_read(ssl, buf, num);
	// if (ret > 0)
	// spdlog::debug("[ssl_hook] SSL_read ssl={} len={} data=[{}]", ssl, ret, shex_dump(buf, ret));
	return ret;
}

static int __cdecl hooked_SSL_write(void *ssl, const void *buf, int num) {
	// if (num > 0)
	//   spdlog::debug("[ssl_hook] SSL_write ssl={} len={} data=[{}]", ssl, num, shex_dump(buf,
	//   num));
	return g_orig_SSL_write(ssl, buf, num);
}

static bool install_hook(void *target, void *detour, void **original, const char *name) {
	if (!target) {
		// spdlog::debug("[ssl_hook] {}: not found", name);
		return false;
	}
	MH_STATUS s = MH_CreateHook(target, detour, original);
	if (s != MH_OK) {
		// spdlog::debug("[ssl_hook] MH_CreateHook({}): {}", name, MH_StatusToString(s));
		return false;
	}
	s = MH_EnableHook(target);
	if (s != MH_OK) {
		// spdlog::debug("[ssl_hook] MH_EnableHook({}): {}", name, MH_StatusToString(s));
		return false;
	}
	return true;
}

bool ssl_hook_install() {
	HMODULE hSSL = GetModuleHandleA("libssl-3.dll");
	if (!hSSL) {
		hSSL = LoadLibraryA("libssl-3.dll");
		if (!hSSL) {
			// spdlog::debug("[ssl_hook] libssl-3.dll not found");
			return false;
		}
	}

	g_pSSL_read = reinterpret_cast<void *>(GetProcAddress(hSSL, "SSL_read"));
	g_pSSL_write = reinterpret_cast<void *>(GetProcAddress(hSSL, "SSL_write"));

	// spdlog::debug("[ssl_hook] SSL_read @ {}, SSL_write @ {}", g_pSSL_read, g_pSSL_write);

	bool ok = true;
	if (g_pSSL_read)
		ok &= install_hook(g_pSSL_read, reinterpret_cast<void *>(&hooked_SSL_read),
						   reinterpret_cast<void **>(&g_orig_SSL_read), "SSL_read");
	if (g_pSSL_write)
		ok &= install_hook(g_pSSL_write, reinterpret_cast<void *>(&hooked_SSL_write),
						   reinterpret_cast<void **>(&g_orig_SSL_write), "SSL_write");

	// spdlog::debug("[ssl_hook] hooks installed");
	return ok;
}

void ssl_hook_shutdown() {
	if (g_pSSL_read) MH_DisableHook(g_pSSL_read);
	if (g_pSSL_write) MH_DisableHook(g_pSSL_write);
}
