#include "game_lua.h"
#include "../core/memory.h"

#include <spdlog/spdlog.h>

// stubs_lua.S
extern "C" {
int call_lua1(void *func, void *p1);
int call_lua2(void *func, void *p1, int p2);
int call_lua3(void *func, void *p1, int p2, int p3);
int call_lua4(void *func, void *p1, int p2, int p3, int p4);
}

static uintptr_t g_luaL_loadbuffer = 0;
static uintptr_t g_lua_pcall = 0;
static uintptr_t g_lua_settop = 0;
static uintptr_t g_lua_newthread = 0;
static uintptr_t g_lua_resume = 0;
static uintptr_t g_lua_yield = 0;

struct LuaFunc {
	const char *name;
	const char *pattern; // IDA style // nullptr = skip scan
	uint32_t fallback_rva;
	uintptr_t *storage;
	uint8_t prologue; // first byte
};

// clang-format off
static const LuaFunc FUNC_TABLE[] = {
    { "luaL_loadbuffer",
      "55 8B EC 83 E4 F8 83 EC 34 8B 45 08"
      " 89 44 24 08 8D 44 24 04 89 44 24 2C"
      " 8D 44 24 20 53 56 8B F1",
      0x00169110, &g_luaL_loadbuffer, 0x55 },

    { "lua_pcall",
      "55 8B EC 83 E4 F8 83 EC 08 56 8B F2"
      " 8B 55 0C 57 8B F9 85 D2 74 0A"
      " E8 ? ? ? ? 8B D0 2B 57 20",
      0x00165C30, &g_lua_pcall, 0x55 },

    { "lua_settop",
      "56 8B F2 C1 E6 04 85 D2 78 ? 8B 51 08 57",
      0x001654A0, &g_lua_settop, 0x56 },

    { "lua_newthread",
      "56 57 8B F9 8B 57 10 8B 42 44 3B 42 40 72 05"
      " E8 ? ? ? ?"
      " 6A 78 6A 00 33 D2 8B CF"
      " E8 ? ? ? ?"
      " 8B 57 10 8B F0 83 C4 08 8B 4A 1C 89 0E 8A 4A 14"
      " 89 72 1C 80 E1 03 88 4E 05 8B CE C6 46 04 08"
      " 8B 57 10"
      " E8 ? ? ? ?"
      " 8B D7"
      " E8 ? ? ? ?"
      " 8B 47 48 8B 4F 4C 89 46 48 89 4E 4C"
      " 8B 47 50 89 46 50 8A 47 38 88 46 38"
      " 8B 4F 3C 89 4E 3C 8B 47 44 89 46 44 89 4E 40"
      " 8B 47 08 89 30 C7 40 08 08 00 00 00"
      " 8B C6 83 47 08 10 5F 5E C3",
      0x00166E30, &g_lua_newthread, 0x56 },

    { "lua_resume",
      "55 8B EC 53 56 57 8B FA 8B D9 3B DF 75 04 33 F6 EB 3E"
      " 0F B6 47 06 83 E8 00 74 13 83 E8 01 74 07"
      " BE 03 00 00 00 EB 29"
      " BE 01 00 00 00 EB 22"
      " 8B 47 14 3B 47 28 76 07 BE 02 00 00 00 EB 13"
      " 8B 77 08 2B 77 0C 83 E6 F0 F7 DE 1B F6 83 E6 FE 83 C6 03"
      " 8B 55 08 8B CF"
      " E8 ? ? ? ?"
      " 85 C0 75 0E"
      " 68 ? ? ? ?"
      " 53"
      " E8 ? ? ? ?"
      " 83 C4 08 83 FE 01 74 1D"
      " FF 34 B5 ? ? ? ?"
      " 68 ? ? ? ?"
      " 53"
      " E8 ? ? ? ?"
      " 83 C4 0C 83 C8 FF 5F 5E 5B 5D C3"
      " 8B 75 08 8B D7 56 8B CB"
      " E8 ? ? ? ?"
      " 66 8B 43 34 83 C4 04 8B D6 66 89 47 34 8B CF"
      " E8 ? ? ? ?"
      " 85 C0 74 31 83 F8 01 74 2C"
      " 3B FB 74 20 83 47 08 F0"
      " 8B 53 08 8B 77 08 8D 42 10 89 43 08"
      " 8B 06 8B 4E 04 89 02 89 4A 04 8B 46 08 89 42 08"
      " 5F 5E 83 C8 FF 5B 5D C3"
      " 8B 77 08 8B CB 2B 77 0C C1 FE 04 8D 56 01"
      " E8 ? ? ? ?"
      " 85 C0 75 0E"
      " 68 ? ? ? ?"
      " 53"
      " E8 ? ? ? ?"
      " 83 C4 08 56 8B D3 8B CF"
      " E8 ? ? ? ?"
      " 83 C4 04 8B C6 5F 5E 5B 5D C3",
      0x001F1EE0, &g_lua_resume, 0x55 },

    { "lua_yield",
      "55 8B EC 56 8B 75 08 57 8B 7E 08"
      " 66 8B 46 34 2B 7E 0C 66 3B 46 36 76 0E"
      " 68 ? ? ? ?"
      " 56"
      " E8 ? ? ? ?"
      " 83 C4 08"
      " 8B 46 08 83 E7 F0 2B C7 C6 46 06 01"
      " 89 46 0C 83 C8 FF 5F 5E 5D C3",
      0x001F2380, &g_lua_yield, 0x55 },
};
// clang-format on

static constexpr int FUNC_COUNT = sizeof(FUNC_TABLE) / sizeof(FUNC_TABLE[0]);

bool game_lua_resolve() {
	uintptr_t base = memory::base_address();
	spdlog::debug("[game_lua] exe base: {:#010x}", static_cast<unsigned>(base));

	uintptr_t textStart = 0;
	size_t textSize = 0;
	memory::get_text_section(base, &textStart, &textSize);
	spdlog::debug("[game_lua] .text: {:#010x} size {:#x}", static_cast<unsigned>(textStart),
				  static_cast<unsigned>(textSize));

	bool all_ok = true;

	for (int i = 0; i < FUNC_COUNT; ++i) {
		const LuaFunc &f = FUNC_TABLE[i];
		uintptr_t addr = 0;

		if (f.pattern) addr = memory::pattern_scan(textStart, textSize, f.pattern);

		if (addr) {
			spdlog::info("[game_lua] {} found via pattern at {:#010x}", f.name, static_cast<unsigned>(addr));
		} else {
			addr = base + f.fallback_rva;
			spdlog::warn("[game_lua] {} pattern miss — fallback RVA {:#010x}", f.name, static_cast<unsigned>(addr));
		}

		auto prologue = *reinterpret_cast<const uint8_t *>(addr);
		if (prologue != f.prologue) {
			spdlog::error("[game_lua] {} bad prologue: expected {:#04x}, got {:#04x}", f.name, f.prologue, prologue);
			all_ok = false;
		}

		*f.storage = addr;
	}

	return all_ok;
}

// call_lua1  — __fastcall, 1 param  (ECX)
// call_lua2  — __fastcall, 2 params (ECX, EDX)
// call_lua3  — __fastcall, 3 params (ECX, EDX, stack)
// call_lua4  — __fastcall, 4 params (ECX, EDX, stack, stack)
int game_luaL_loadbuffer(void *L, const char *buf, int sz, const char *name) {
	return call_lua4((void *)g_luaL_loadbuffer, L, (int)(uintptr_t)buf, sz, (int)(uintptr_t)name);
}

int game_lua_pcall(void *L, int nargs, int nresults, int errfunc) {
	return call_lua4((void *)g_lua_pcall, L, nargs, nresults, errfunc);
}

void game_lua_settop(void *L, int index) {
	call_lua2((void *)g_lua_settop, L, index);
}

void *game_lua_newthread(void *L) {
	return (void *)(uintptr_t)call_lua1((void *)g_lua_newthread, L);
}

int game_lua_resume(void *from, void *co, int narg) {
	return call_lua3((void *)g_lua_resume, from, (int)(uintptr_t)co, narg);
}

// TODO: check this on updates
int game_lua_yield(void *L) { // Doesn't need a wrapper?
	typedef int(__attribute__((cdecl)) * yield_fn_t)(void *);
	auto fn = reinterpret_cast<yield_fn_t>(g_lua_yield);
	return fn(L);
}
