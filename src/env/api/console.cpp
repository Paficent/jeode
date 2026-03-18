#include "console.h"
#include "../../lua/game_lua.h"
#include "../../lua/thread.h"

extern "C" {
#include <lua.h>
}

#include <spdlog/spdlog.h>

#include <atomic>
#include <cstring>
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

// -----------------------------------------------------------------------
// Lua callbacks (called from game's Lua — safe context)
// -----------------------------------------------------------------------

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
	});
	return 0;
}

// -----------------------------------------------------------------------
// console.read() — coroutine-based async read
//
// Yield: stores current coroutine in __console_read_co global, yields
// Resume: queues Lua snippet via game_luaL_loadbuffer (avoids C lua_resume)
// -----------------------------------------------------------------------

static void flush_writer() {
	HANDLE done = CreateEventA(nullptr, TRUE, FALSE, nullptr);
	if (!done) return;
	queue_write([done] { SetEvent(done); });
	WaitForSingleObject(done, INFINITE);
	CloseHandle(done);
}

static const char CONSOLE_RESUME_LUA[] = "local co = __console_read_co\n"
										 "__console_read_co = nil\n"
										 "local r = __console_read_result\n"
										 "__console_read_result = nil\n"
										 "if co then coroutine.resume(co, r) end\n";

static DWORD WINAPI read_thread_proc(LPVOID) {
	flush_writer();
	force_foreground(GetConsoleWindow());

	char buf[4096];
	DWORD nread = 0;
	if (!ReadConsoleA(s_console_in, buf, sizeof(buf) - 1, &nread, NULL)) {
		lua_thread_queue([](lua_State *L) {
			int base = lua_gettop(L);
			lua_pushnil(L);
			lua_setglobal(L, "__console_read_co");
			game_lua_settop(L, base);
		});
		return 1;
	}

	while (nread > 0 && (buf[nread - 1] == '\n' || buf[nread - 1] == '\r')) --nread;

	std::string result(buf, nread);

	lua_thread_queue([result](lua_State *L) {
		int base = lua_gettop(L);

		// Store result as global for Lua snippet
		lua_pushstring(L, result.c_str());
		lua_setglobal(L, "__console_read_result");

		// Resume coroutine via Lua (avoids C-level lua_resume)
		int s = game_luaL_loadbuffer(L, CONSOLE_RESUME_LUA, static_cast<int>(strlen(CONSOLE_RESUME_LUA)),
									 "=console_resume");
		if (s == 0) game_lua_pcall(L, 0, 0, 0);

		game_lua_settop(L, base);
	});

	return 0;
}

static int l_console_read_line(lua_State *L) {
	ensure_console();

	// Store current coroutine in global (safe: pushthread + setglobal)
	lua_pushthread(L);
	lua_setglobal(L, "__console_read_co");

	HANDLE h = CreateThread(nullptr, 0, read_thread_proc, nullptr, 0, nullptr);
	if (h) {
		CloseHandle(h);
	} else {
		lua_pushnil(L);
		lua_setglobal(L, "__console_read_co");
		lua_pushstring(L, "failed to create read thread");
		return lua_error(L);
	}

	return lua_yield(L, 0);
}

// -----------------------------------------------------------------------
// Registration
// -----------------------------------------------------------------------

void console_api_register(lua_State *L) {
	lua_newtable(L);

	lua_pushcfunction(L, l_console_show);
	lua_setfield(L, -2, "showWindow");

	lua_pushcfunction(L, l_console_hide);
	lua_setfield(L, -2, "hideWindow");

	lua_pushcfunction(L, l_console_set_title);
	lua_setfield(L, -2, "setTitle");

	lua_pushcfunction(L, l_console_write);
	lua_setfield(L, -2, "write");

	lua_pushcfunction(L, l_console_write_line);
	lua_setfield(L, -2, "writeLine");

	lua_pushcfunction(L, l_console_clear);
	lua_setfield(L, -2, "clear");

	lua_pushcfunction(L, l_console_read_line);
	lua_setfield(L, -2, "read");

	lua_setglobal(L, "console");
}
