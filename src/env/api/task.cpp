/*
 * An interpretation and extension of Lune's (Roblox's) task library
 * https://lune-org.github.io/docs/api-reference/task/
 *
 * The scheduler tick runs at far too high of a frequency to dispatch lua
 * callbacks on, hence why there is no `onTick` function.
 *
 * TODO: `task.defer` &  more `task.onX`
 */

#include "task.h"
#include "../../hooks/egl_hook.h"
#include "../../hooks/scheduler_hook.h"
#include "../../lua/game_lua.h"
#include "../../lua/thread.h"
#include "../api.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <utility>
#include <vector>

using clock_type = std::chrono::steady_clock;
using time_point = clock_type::time_point;

struct WaitingCoroutine {
	lua_State *co;
	int ref;
	time_point start;
	time_point deadline;
	bool pending;
	int nargs;
};

struct FrameListener {
	int id;
	lua_State *co;
	int fn_ref;
	int co_ref;
};

static std::vector<WaitingCoroutine> s_waiting;
static std::vector<FrameListener> s_frame_listeners;
static int s_next_frame_id = 1;
static time_point s_last_frame;
static bool s_have_last_frame = false;

static double sub_seconds(time_point a, time_point b) {
	return std::chrono::duration<double>(b - a).count();
}

// task.delay(duration: number, toDelay: function | thread): thread
static int l_task_delay(lua_State *L) {
	double seconds = luaL_checknumber(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	if (seconds < 0.0) seconds = 0.0;

	int nargs = lua_gettop(L) - 2;

	lua_State *co = static_cast<lua_State *>(game_lua_newthread(L));

	lua_pushvalue(L, 2);
	for (int i = 1; i <= nargs; i++) lua_pushvalue(L, 2 + i);
	lua_xmove(L, co, nargs + 1);

	lua_pushvalue(L, -1);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	WaitingCoroutine entry;
	entry.co = co;
	entry.ref = ref;
	entry.start = clock_type::now();
	entry.deadline = entry.start +
					 std::chrono::duration_cast<clock_type::duration>(std::chrono::duration<double>(seconds));
	entry.pending = true;
	entry.nargs = nargs;
	s_waiting.push_back(entry);

	return 1;
}

// task.cancel(thread: thread): nil
static int l_task_cancel(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTHREAD);
	lua_State *target = lua_tothread(L, 1);

	auto wit = std::find_if(s_waiting.begin(), s_waiting.end(),
							[target](const WaitingCoroutine &w) { return w.co == target; });
	if (wit != s_waiting.end()) {
		luaL_unref(L, LUA_REGISTRYINDEX, wit->ref);
		s_waiting.erase(wit);
		return 0;
	}

	// TODO: update this when there are more `onX` functions
	auto fit = std::find_if(s_frame_listeners.begin(), s_frame_listeners.end(),
							[target](const FrameListener &f) { return f.co == target; });
	if (fit != s_frame_listeners.end()) {
		luaL_unref(L, LUA_REGISTRYINDEX, fit->fn_ref);
		luaL_unref(L, LUA_REGISTRYINDEX, fit->co_ref);
		s_frame_listeners.erase(fit);
	}
	return 0;
}
// task.onFrame(listener: function(dt: number)): thread
static int l_task_on_frame(lua_State *L) {
	luaL_checktype(L, 1, LUA_TFUNCTION);

	lua_pushvalue(L, 1);
	int fn_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	lua_State *co = static_cast<lua_State *>(game_lua_newthread(L));

	lua_pushvalue(L, -1);
	int co_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	FrameListener fl;
	fl.id = s_next_frame_id++;
	fl.co = co;
	fl.fn_ref = fn_ref;
	fl.co_ref = co_ref;
	s_frame_listeners.push_back(fl);

	return 1;
}

// task.spawn(toSpawn: function | thread): thread
static int l_task_spawn(lua_State *L) {
	luaL_checktype(L, 1, LUA_TFUNCTION);
	int nargs = lua_gettop(L) - 1;

	lua_State *co = static_cast<lua_State *>(game_lua_newthread(L));

	lua_pushvalue(L, 1);
	for (int i = 1; i <= nargs; i++) lua_pushvalue(L, i + 1);
	lua_xmove(L, co, nargs + 1);

	int r = game_lua_resume(L, co, nargs);
	if (r < 0) {
		const char *err = lua_tostring(co, -1);
		spdlog::error("[task] spawn error: {}", err ? err : "(unknown)");
	}

	return 1;
}

// task.wait(duration: number): number
static int l_task_wait(lua_State *L) { // Waits for atleast (!) the amount of time, depends on OS
	double seconds = 0.0;
	if (lua_isnumber(L, 1)) seconds = lua_tonumber(L, 1);
	if (seconds < 0.0) seconds = 0.0;

	if (lua_pushthread(L)) {
		lua_pop(L, 1);
		return luaL_error(L, "task.wait: cannot wait from the main thread");
	}
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	WaitingCoroutine entry;
	entry.co = L;
	entry.ref = ref;
	entry.start = clock_type::now();
	entry.deadline = entry.start +
					 std::chrono::duration_cast<clock_type::duration>(std::chrono::duration<double>(seconds));
	entry.pending = false;
	entry.nargs = 0;
	s_waiting.push_back(entry);

	return game_lua_yield(L);
}

static void do_frame_listeners(lua_State *main, double dt) {
	std::vector<FrameListener> snapshot = s_frame_listeners;

	for (const auto &fl : snapshot) {
		bool still_registered = std::any_of(s_frame_listeners.begin(), s_frame_listeners.end(),
											[&fl](const FrameListener &x) { return x.id == fl.id; });
		if (!still_registered) continue;

		int base = lua_gettop(main);
		lua_rawgeti(main, LUA_REGISTRYINDEX, fl.fn_ref);
		if (!lua_isfunction(main, -1)) {
			game_lua_settop(main, base);
			continue;
		}
		lua_pushnumber(main, dt);
		int s = game_lua_pcall(main, 1, 0, 0);
		if (s != 0) {
			const char *err = lua_tostring(main, -1);
			spdlog::error("[task] onFrame error: {}", err ? err : "(unknown)");
			game_lua_settop(main, base);
		}
	}
}

static void task_tick() {
	lua_State *main = lua_thread_get_state();
	if (!main) return;

	time_point now = clock_type::now();
	std::vector<WaitingCoroutine> ready;

	// resumes all waiters that are ready
	auto w = s_waiting.begin();
	while (w != s_waiting.end()) {
		if (w->deadline <= now) {
			ready.push_back(std::move(*w));
			w = s_waiting.erase(w);
		} else {
			++w;
		}
	}
	for (auto &w : ready) {
		int r;
		if (w.pending) {
			r = game_lua_resume(main, w.co, w.nargs);
		} else {
			double elapsed = sub_seconds(w.start, now);
			lua_pushnumber(w.co, elapsed);
			r = game_lua_resume(main, w.co, 1);
		}
		if (r < 0) {
			const char *err = lua_tostring(w.co, -1);
			spdlog::error("[task] {} resume error: {}", w.pending ? "delay" : "wait", err ? err : "(unknown)");
		}
		luaL_unref(main, LUA_REGISTRYINDEX, w.ref);
	}
}

static void task_frame() {
	time_point now = clock_type::now();
	double dt = s_have_last_frame ? sub_seconds(s_last_frame, now) : 0.0;
	s_last_frame = now;
	s_have_last_frame = true;

	lua_thread_queue([dt](lua_State *L) { do_frame_listeners(L, dt); });
}

void task_api_init() {
	scheduler_register_tick(task_tick);
	spdlog::debug("[task] tick registered");

	egl_hook_register_frame(task_frame);
	spdlog::debug("[task] frame registered");
}

static const LuaApiFunction TASK_FUNCTIONS[] = {
	{"wait", l_task_wait},	   {"spawn", l_task_spawn},		 {"delay", l_task_delay},
	{"cancel", l_task_cancel}, {"onFrame", l_task_on_frame},
};

static const LuaApiTable TASK_TABLE = {
	"task",
	TASK_FUNCTIONS,
	sizeof(TASK_FUNCTIONS) / sizeof(TASK_FUNCTIONS[0]),
};

const LuaApiTable &task_api_table() {
	return TASK_TABLE;
}
