#include "thread.h"
#include "../hooks/scheduler_hook.h"
#include "game_lua.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <spdlog/spdlog.h>
#include <windows.h>

#include <atomic>

static std::atomic<lua_State *> g_state{nullptr};
static DWORD g_game_thread_id = 0;

void lua_thread_set_state(lua_State *L) {
	g_state.store(L, std::memory_order_release);
	g_game_thread_id = GetCurrentThreadId();
}

lua_State *lua_thread_get_state() {
	return g_state.load(std::memory_order_acquire);
}

bool lua_thread_ready() {
	return g_state.load(std::memory_order_acquire) != nullptr;
}

void lua_thread_queue(std::function<void(lua_State *)> work) {
	scheduler_queue_work([work = std::move(work)]() {
		lua_State *L = g_state.load(std::memory_order_acquire);
		if (L)
			work(L);
		else
			spdlog::error("[lua_thread] work queued but no lua state");
	});
}

void lua_thread_queue_sync(std::function<void(lua_State *)> work) {
	lua_State *L = g_state.load(std::memory_order_acquire);
	if (GetCurrentThreadId() == g_game_thread_id && L) {
		work(L);
		return;
	}

	HANDLE event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
	if (!event) {
		spdlog::error("[lua_thread] CreateEvent failed");
		return;
	}

	lua_thread_queue([work = std::move(work), event](lua_State *L) {
		work(L);
		SetEvent(event);
	});

	WaitForSingleObject(event, INFINITE);
	CloseHandle(event);
}

void lua_thread_register_global(const char *name, lua_CFunction fn) {
	std::string n = name;
	lua_thread_queue([n, fn](lua_State *L) {
		lua_pushcfunction(L, fn);
		lua_setglobal(L, n.c_str());
	});
}
