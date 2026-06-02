#include "ui.h"
#include "../../hooks/egl_hook.h"
#include "../../lua/game_lua.h"
#include "../../lua/thread.h"
#include "../api.h"
#include "ui/widgets.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include "imgui.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <vector>

struct FrameCallback {
	int id;
	int ref;
};

static std::vector<FrameCallback> s_callbacks;
static std::vector<FrameCallback> s_pending_add;
static std::vector<int> s_pending_remove;
static std::atomic<int> s_next_id{1};
static bool s_iterating = false;

static bool s_in_frame = false;
static int s_anon_id = 0;

static std::vector<LuaApiFunction> s_table_fns;
static LuaApiTable s_table;

bool ui_in_frame() {
	return s_in_frame;
}

int ui_require_in_frame(lua_State *L, const char *name) {
	if (s_in_frame) return 0;
	return luaL_error(L, "%s: must be called from a frame callback", name);
}

int ui_next_anon_id() {
	return ++s_anon_id;
}

void ui_resolve_opts_body(lua_State *L, int opts_slot, int *opts_out, int *body_out) {
	if (lua_isfunction(L, opts_slot)) {
		*opts_out = 0;
		*body_out = opts_slot;
	} else if (lua_istable(L, opts_slot)) {
		*opts_out = opts_slot;
		*body_out = lua_isfunction(L, opts_slot + 1) ? opts_slot + 1 : 0;
	} else {
		*opts_out = 0;
		*body_out = 0;
	}
}

int ui_call_body(lua_State *L, int body_idx) {
	if (body_idx == 0) return 0;
	lua_pushvalue(L, body_idx);
	return game_lua_pcall(L, 0, 0, 0);
}

bool ui_opt_bool(lua_State *L, int opts_idx, const char *key, bool def) {
	if (opts_idx == 0 || !lua_istable(L, opts_idx)) return def;
	lua_getfield(L, opts_idx, key);
	bool out = def;
	if (lua_isboolean(L, -1))
		out = lua_toboolean(L, -1) != 0;
	else if (lua_isnumber(L, -1))
		out = lua_tonumber(L, -1) != 0;
	lua_pop(L, 1);
	return out;
}

double ui_opt_number(lua_State *L, int opts_idx, const char *key, double def) {
	if (opts_idx == 0 || !lua_istable(L, opts_idx)) return def;
	lua_getfield(L, opts_idx, key);
	double out = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : def;
	lua_pop(L, 1);
	return out;
}

int ui_opt_integer(lua_State *L, int opts_idx, const char *key, int def) {
	if (opts_idx == 0 || !lua_istable(L, opts_idx)) return def;
	lua_getfield(L, opts_idx, key);
	int out = lua_isnumber(L, -1) ? static_cast<int>(lua_tointeger(L, -1)) : def;
	lua_pop(L, 1);
	return out;
}

std::string ui_opt_string(lua_State *L, int opts_idx, const char *key, const char *def) {
	if (opts_idx == 0 || !lua_istable(L, opts_idx)) return def ? def : "";
	lua_getfield(L, opts_idx, key);
	std::string out;
	if (lua_isstring(L, -1))
		out = lua_tostring(L, -1);
	else
		out = def ? def : "";
	lua_pop(L, 1);
	return out;
}

bool ui_opt_color(lua_State *L, int opts_idx, const char *key, float &r, float &g, float &b, float &a) {
	if (opts_idx == 0 || !lua_istable(L, opts_idx)) return false;
	lua_getfield(L, opts_idx, key);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return false;
	}
	int color_idx = lua_gettop(L);

	auto read = [&](const char *name, int num_idx, float def) -> float {
		lua_getfield(L, color_idx, name);
		if (lua_isnumber(L, -1)) {
			float v = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
			return v;
		}
		lua_pop(L, 1);
		lua_rawgeti(L, color_idx, num_idx);
		if (lua_isnumber(L, -1)) {
			float v = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
			return v;
		}
		lua_pop(L, 1);
		return def;
	};

	r = read("r", 1, 1.0f);
	g = read("g", 2, 1.0f);
	b = read("b", 3, 1.0f);
	a = read("a", 4, 1.0f);

	lua_pop(L, 1);
	return true;
}

int ui_opt_state(lua_State *L, int opts_idx, const char *key) {
	if (opts_idx == 0 || !lua_istable(L, opts_idx)) return 0;
	lua_getfield(L, opts_idx, key);
	if (ui_is_state(L, -1)) return lua_gettop(L);
	lua_pop(L, 1);
	return 0;
}

static int event_returner(lua_State *L) {
	lua_pushvalue(L, lua_upvalueindex(1));
	return 1;
}

int ui_push_widget(lua_State *L, const UiEvent *events, int n) {
	lua_createtable(L, 0, n);
	for (int i = 0; i < n; i++) {
		lua_pushboolean(L, events[i].value ? 1 : 0);
		lua_pushcclosure(L, event_returner, 1);
		lua_setfield(L, -2, events[i].name);
	}
	return 1;
}

int ui_register_frame_callback(lua_State *L) {
	luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_pushvalue(L, 1);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	FrameCallback fc;
	fc.id = s_next_id.fetch_add(1);
	fc.ref = ref;

	if (s_iterating)
		s_pending_add.push_back(fc);
	else
		s_callbacks.push_back(fc);

	lua_pushinteger(L, fc.id);
	return 1;
}

static int l_register(lua_State *L) {
	return ui_register_frame_callback(L);
}

static int l_disconnect(lua_State *L) {
	int id = static_cast<int>(luaL_checkinteger(L, 1));
	if (s_iterating) {
		s_pending_remove.push_back(id);
		return 0;
	}
	auto it = std::find_if(s_callbacks.begin(), s_callbacks.end(),
						   [id](const FrameCallback &fc) { return fc.id == id; });
	if (it != s_callbacks.end()) {
		luaL_unref(L, LUA_REGISTRYINDEX, it->ref);
		s_callbacks.erase(it);
	}
	return 0;
}

void ui_draw_frame() {
	if (!lua_thread_is_current()) {
		static bool warned = false;
		if (!warned) {
			spdlog::warn("[ui] imgui frame is not on the lua thread, ui callbacks disabled");
			warned = true;
		}
		return;
	}

	lua_State *L = lua_thread_get_state();
	if (!L || s_callbacks.empty()) return;

	s_in_frame = true;
	s_iterating = true;

	for (const auto &cb : s_callbacks) {
		int base = lua_gettop(L);
		s_anon_id = 0;

		lua_rawgeti(L, LUA_REGISTRYINDEX, cb.ref);
		if (!lua_isfunction(L, -1)) {
			game_lua_settop(L, base);
			continue;
		}

		int s = game_lua_pcall(L, 0, 0, 0);
		if (s != 0) {
			const char *err = lua_tostring(L, -1);
			spdlog::error("[ui] frame callback error: {}", err ? err : "(unknown)");
		}

		game_lua_settop(L, base);
	}

	s_iterating = false;
	s_in_frame = false;

	if (!s_pending_remove.empty()) {
		for (int id : s_pending_remove) {
			auto it = std::find_if(s_callbacks.begin(), s_callbacks.end(),
								   [id](const FrameCallback &fc) { return fc.id == id; });
			if (it != s_callbacks.end()) {
				luaL_unref(L, LUA_REGISTRYINDEX, it->ref);
				s_callbacks.erase(it);
			}
		}
		s_pending_remove.clear();
	}
	if (!s_pending_add.empty()) {
		s_callbacks.insert(s_callbacks.end(), s_pending_add.begin(), s_pending_add.end());
		s_pending_add.clear();
	}
}

void ui_api_init() {
	s_table_fns = {
		ui_window_fn(),
		ui_child_fn(),
		ui_text_fn(),
		ui_button_fn(),
		ui_checkbox_fn(),
		ui_input_fn(),
		ui_slider_fn(),
		ui_color_fn(),
		ui_dropdown_fn(),
		ui_tab_bar_fn(),
		ui_tab_item_fn(),
		ui_tree_fn(),
		ui_separator_fn(),
		ui_same_line_fn(),
		ui_spacing_fn(),
		ui_indent_fn(),
		ui_tooltip_fn(),
		ui_code_editor_fn(),
		ui_state_fn(),
		{"register", l_register},
		{"disconnect", l_disconnect},
	};
	s_table.name = "ui";
	s_table.functions = s_table_fns.data();
	s_table.count = s_table_fns.size();

	egl_hook_set_imgui_frame(ui_draw_frame);
	spdlog::debug("[ui] imgui frame registered");
}

const LuaApiTable &ui_api_table() {
	return s_table;
}
