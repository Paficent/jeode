#include "console.h"
#include "../../lua/game_lua.h"
#include "../../lua/thread.h"
#include "../api.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <spdlog/spdlog.h>

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>

#include <windows.h>

static HANDLE s_console_out = INVALID_HANDLE_VALUE;
static HANDLE s_console_in = INVALID_HANDLE_VALUE;

static HANDLE s_writer_thread = nullptr;
static HANDLE s_writer_event = nullptr;
static std::mutex s_writer_mutex;
static std::deque<std::function<void()>> s_writer_queue;
static std::atomic<bool> s_writer_running = false;

static bool ensure_console() {
	if (s_console_out != INVALID_HANDLE_VALUE) return true;

	AllocConsole();

	HWND console_wnd = GetConsoleWindow();
	if (console_wnd) ShowWindow(console_wnd, SW_SHOW);

	s_console_out = GetStdHandle(STD_OUTPUT_HANDLE);
	s_console_in = GetStdHandle(STD_INPUT_HANDLE);

	if (s_console_out == INVALID_HANDLE_VALUE || s_console_in == INVALID_HANDLE_VALUE) return false;

	DWORD mode = 0;
	GetConsoleMode(s_console_in, &mode);
	SetConsoleMode(s_console_in, mode | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);

	return true;
}

static DWORD WINAPI writer_proc(LPVOID) {
	while (s_writer_running) {
		WaitForSingleObject(s_writer_event, INFINITE);

		for (;;) {
			std::function<void()> task;
			{
				std::lock_guard<std::mutex> lock(s_writer_mutex);
				if (s_writer_queue.empty()) break;
				task = std::move(s_writer_queue.front());
				s_writer_queue.pop_front();
			}
			task();
		}
	}
	return 0;
}

static void ensure_writer() {
	if (s_writer_thread) return;
	s_writer_event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
	s_writer_running = true;
	s_writer_thread = CreateThread(nullptr, 0, writer_proc, nullptr, 0, nullptr);
}

static void queue_write(std::function<void()> fn) {
	ensure_writer();
	{
		std::lock_guard<std::mutex> lock(s_writer_mutex);
		s_writer_queue.push_back(std::move(fn));
	}
	SetEvent(s_writer_event);
}

static void force_foreground(HWND hwnd) {
	if (!hwnd) return;
	ShowWindow(hwnd, SW_SHOW);
	DWORD fore_thread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
	DWORD cur_thread = GetCurrentThreadId();
	if (fore_thread != cur_thread) AttachThreadInput(fore_thread, cur_thread, TRUE);
	SetForegroundWindow(hwnd);
	if (fore_thread != cur_thread) AttachThreadInput(fore_thread, cur_thread, FALSE);
}

static int l_console_show(lua_State *) {
	ensure_console();
	queue_write([] {
		HWND hwnd = GetConsoleWindow();
		if (hwnd) {
			ShowWindow(hwnd, SW_SHOW);
			SetForegroundWindow(hwnd);
		}
	});
	return 0;
}

static int l_console_hide(lua_State *) {
	ensure_console();
	queue_write([] {
		HWND hwnd = GetConsoleWindow();
		if (hwnd) ShowWindow(hwnd, SW_HIDE);
	});
	return 0;
}

static int l_console_set_title(lua_State *L) {
	const char *raw = lua_tostring(L, 1);
	if (!raw) {
		lua_pushstring(L, "console.setTitle: string expected");
		return lua_error(L);
	}
	std::string title = raw;
	ensure_console();
	queue_write([title] { SetConsoleTitleA(title.c_str()); });
	return 0;
}

static int l_console_write(lua_State *L) {
	const char *raw = lua_tostring(L, 1);
	if (!raw) {
		lua_pushstring(L, "console.write: string expected");
		return lua_error(L);
	}
	std::string buf = raw;
	ensure_console();
	queue_write([buf] {
		DWORD written = 0;
		WriteConsoleA(s_console_out, buf.c_str(), static_cast<DWORD>(buf.size()), &written, NULL);
	});
	return 0;
}

static int l_console_write_line(lua_State *L) {
	const char *raw = lua_tostring(L, 1);
	if (!raw) {
		lua_pushstring(L, "console.writeLine: string expected");
		return lua_error(L);
	}
	std::string buf = std::string(raw) + "\r\n";
	ensure_console();
	queue_write([buf] {
		DWORD written = 0;
		WriteConsoleA(s_console_out, buf.c_str(), static_cast<DWORD>(buf.size()), &written, NULL);
	});
	return 0;
}

static int l_console_clear(lua_State *) {
	ensure_console();
	queue_write([] {
		CONSOLE_SCREEN_BUFFER_INFO info;
		if (!GetConsoleScreenBufferInfo(s_console_out, &info)) return;
		DWORD cells = info.dwSize.X * info.dwSize.Y;
		COORD origin = {0, 0};
		DWORD written = 0;
		FillConsoleOutputCharacterA(s_console_out, ' ', cells, origin, &written);
		FillConsoleOutputAttribute(s_console_out, info.wAttributes, cells, origin, &written);
		SetConsoleCursorPosition(s_console_out, origin);
		SMALL_RECT viewport = {0, 0, static_cast<SHORT>(info.dwSize.X - 1),
							   static_cast<SHORT>(info.srWindow.Bottom - info.srWindow.Top)};
		SetConsoleWindowInfo(s_console_out, TRUE, &viewport);
	});
	return 0;
}

static void flush_writer() {
	HANDLE done = CreateEventA(nullptr, TRUE, FALSE, nullptr);
	if (!done) return;
	queue_write([done] { SetEvent(done); });
	WaitForSingleObject(done, INFINITE);
	CloseHandle(done);
}

static lua_State *s_read_coroutine = nullptr;
static int s_read_coroutine_ref = LUA_NOREF;

static DWORD WINAPI read_thread_proc(LPVOID) {
	flush_writer();
	force_foreground(GetConsoleWindow());

	char buf[4096];
	DWORD nread = 0;
	if (!ReadConsoleA(s_console_in, buf, sizeof(buf) - 1, &nread, NULL)) {
		lua_thread_queue([](lua_State *L) {
			if (s_read_coroutine_ref != LUA_NOREF) {
				luaL_unref(L, LUA_REGISTRYINDEX, s_read_coroutine_ref);
				s_read_coroutine_ref = LUA_NOREF;
			}
			s_read_coroutine = nullptr;
		});
		return 1;
	}

	while (nread > 0 && (buf[nread - 1] == '\n' || buf[nread - 1] == '\r')) --nread;

	std::string result(buf, nread);

	lua_thread_queue([result](lua_State *L) {
		int base = lua_gettop(L);
		lua_State *co = s_read_coroutine;
		int ref = s_read_coroutine_ref;
		s_read_coroutine = nullptr;
		s_read_coroutine_ref = LUA_NOREF;

		if (!co) {
			spdlog::error("[console] read completed but no coroutine to resume");
			return;
		}

		lua_pushstring(L, result.c_str());
		int r = game_lua_resume(L, co, 1);
		if (r < 0) {
			const char *err = lua_tostring(L, -1);
			spdlog::error("[console] coroutine resume failed: {}", err ? err : "(unknown)");
		}

		if (ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, ref);
		game_lua_settop(L, base);
	});

	return 0;
}

static int l_console_read_line(lua_State *L) {
	ensure_console();

	s_read_coroutine = L;
	lua_pushthread(L);
	s_read_coroutine_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	HANDLE h = CreateThread(nullptr, 0, read_thread_proc, nullptr, 0, nullptr);
	if (h) {
		CloseHandle(h);
	} else {
		luaL_unref(L, LUA_REGISTRYINDEX, s_read_coroutine_ref);
		s_read_coroutine_ref = LUA_NOREF;
		s_read_coroutine = nullptr;
		lua_pushstring(L, "failed to create read thread");
		return lua_error(L);
	}

	return game_lua_yield(L);
}

static const LuaApiFunction CONSOLE_FUNCTIONS[] = {
	{"show", l_console_show},
	{"hide", l_console_hide},
	{"setTitle", l_console_set_title},
	{"write", l_console_write},
	{"writeLine", l_console_write_line},
	{"clear", l_console_clear},
	{"read", l_console_read_line},
};

static const LuaApiTable CONSOLE_TABLE = {
	"console",
	CONSOLE_FUNCTIONS,
	sizeof(CONSOLE_FUNCTIONS) / sizeof(CONSOLE_FUNCTIONS[0]),
};

const LuaApiTable &console_api_table() {
	return CONSOLE_TABLE;
}
