#pragma once

#include "../../api.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <string>

bool ui_in_frame();
int ui_require_in_frame(lua_State *L, const char *name);
int ui_next_anon_id();

void ui_state_init_metatable(lua_State *L);
bool ui_is_state(lua_State *L, int idx);
void ui_state_push_value(lua_State *L, int state_idx);
void ui_state_set_value(lua_State *L, int state_idx);

bool ui_opt_bool(lua_State *L, int opts_idx, const char *key, bool def);
double ui_opt_number(lua_State *L, int opts_idx, const char *key, double def);
int ui_opt_integer(lua_State *L, int opts_idx, const char *key, int def);
std::string ui_opt_string(lua_State *L, int opts_idx, const char *key, const char *def);
bool ui_opt_color(lua_State *L, int opts_idx, const char *key, float &r, float &g, float &b, float &a);
int ui_opt_state(lua_State *L, int opts_idx, const char *key);

void ui_resolve_opts_body(lua_State *L, int opts_slot, int *opts_out, int *body_out);

int ui_call_body(lua_State *L, int body_idx);

struct UiEvent {
	const char *name;
	bool value;
};
int ui_push_widget(lua_State *L, const UiEvent *events, int n);

const LuaApiFunction &ui_window_fn();
const LuaApiFunction &ui_child_fn();
const LuaApiFunction &ui_text_fn();
const LuaApiFunction &ui_button_fn();
const LuaApiFunction &ui_checkbox_fn();
const LuaApiFunction &ui_input_fn();
const LuaApiFunction &ui_slider_fn();
const LuaApiFunction &ui_color_fn();
const LuaApiFunction &ui_dropdown_fn();
const LuaApiFunction &ui_tab_bar_fn();
const LuaApiFunction &ui_tab_item_fn();
const LuaApiFunction &ui_tree_fn();
const LuaApiFunction &ui_separator_fn();
const LuaApiFunction &ui_same_line_fn();
const LuaApiFunction &ui_spacing_fn();
const LuaApiFunction &ui_indent_fn();
const LuaApiFunction &ui_tooltip_fn();
const LuaApiFunction &ui_code_editor_fn();
const LuaApiFunction &ui_state_fn();
