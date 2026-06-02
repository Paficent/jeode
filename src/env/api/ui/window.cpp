#include "widgets.h"

#include "imgui.h"

static int l_window(lua_State *L) {
	ui_require_in_frame(L, "ui.window");
	const char *title = luaL_checkstring(L, 1);

	int opts_idx, body_idx;
	ui_resolve_opts_body(L, 2, &opts_idx, &body_idx);

	ImGuiWindowFlags flags = 0;
	if (ui_opt_bool(L, opts_idx, "noTitleBar", false)) flags |= ImGuiWindowFlags_NoTitleBar;
	if (ui_opt_bool(L, opts_idx, "noBackground", false)) flags |= ImGuiWindowFlags_NoBackground;
	if (ui_opt_bool(L, opts_idx, "noCollapse", false)) flags |= ImGuiWindowFlags_NoCollapse;
	if (ui_opt_bool(L, opts_idx, "noMove", false)) flags |= ImGuiWindowFlags_NoMove;
	if (ui_opt_bool(L, opts_idx, "noScrollbar", false)) flags |= ImGuiWindowFlags_NoScrollbar;
	if (ui_opt_bool(L, opts_idx, "noResize", false)) flags |= ImGuiWindowFlags_NoResize;
	bool noClose = ui_opt_bool(L, opts_idx, "noClose", false);

	int state_idx = ui_opt_state(L, opts_idx, "opened");
	bool has_state = state_idx != 0;

	bool state_value = true;
	if (has_state) {
		lua_rawgeti(L, state_idx, 1);
		state_value = lua_toboolean(L, -1) != 0;
		lua_pop(L, 1);
	}

	if (has_state && !state_value) {
		lua_pop(L, 1);
		UiEvent events[] = {
			{"opened", false},
			{"closed", false},
			{"hovered", false},
		};
		ui_push_widget(L, events, 3);
		return 1;
	}

	bool open_value = state_value;
	bool *p_open = (has_state && !noClose) ? &open_value : nullptr;

	bool is_open = ImGui::Begin(title, p_open, flags);

	int err = 0;
	bool hovered = false;
	if (is_open) {
		if (body_idx != 0) err = ui_call_body(L, body_idx);
		if (err == 0) hovered = ImGui::IsWindowHovered();
	}

	ImGui::End();

	bool just_closed = p_open && !open_value;
	if (has_state) {
		if (just_closed) {
			lua_pushboolean(L, 0);
			ui_state_set_value(L, state_idx);
		}
		if (err != 0)
			lua_remove(L, state_idx);
		else
			lua_pop(L, 1);
	}

	if (err != 0) return lua_error(L);

	UiEvent events[] = {
		{"opened", is_open},
		{"closed", just_closed},
		{"hovered", hovered},
	};
	ui_push_widget(L, events, 3);
	return 1;
}

const LuaApiFunction &ui_window_fn() {
	static const LuaApiFunction fn = {"window", l_window};
	return fn;
}
