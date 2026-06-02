#include "widgets.h"

#include "imgui.h"

#include <string>

static int l_dropdown(lua_State *L) {
	ui_require_in_frame(L, "ui.dropdown");
	const char *label = luaL_checkstring(L, 1);

	bool has_state = ui_is_state(L, 2);
	if (!has_state && !lua_isnoneornil(L, 2)) {
		return luaL_error(L, "ui.dropdown: 2nd arg must be a state (use ui.state(\"\"))");
	}

	if (!lua_istable(L, 3)) {
		return luaL_error(L, "ui.dropdown: 3rd arg must be an options table with 'options'");
	}

	lua_getfield(L, 3, "options");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return luaL_error(L, "ui.dropdown: opts.options must be a table of strings");
	}
	int options_idx = lua_gettop(L);
	int n_options = static_cast<int>(lua_objlen(L, options_idx));

	std::string current;
	if (has_state) {
		lua_rawgeti(L, 2, 1);
		if (lua_isstring(L, -1)) current = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	bool combo_open = ImGui::BeginCombo(label, current.c_str());
	bool hovered = ImGui::IsItemHovered();
	bool changed = false;

	if (combo_open) {
		for (int i = 1; i <= n_options; i++) {
			lua_rawgeti(L, options_idx, i);
			if (lua_isstring(L, -1)) {
				const char *opt = lua_tostring(L, -1);
				bool selected = current == opt;
				if (ImGui::Selectable(opt, selected)) {
					if (has_state) {
						lua_pushstring(L, opt);
						ui_state_set_value(L, 2);
					}
					changed = true;
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			lua_pop(L, 1);
		}
		ImGui::EndCombo();
	}

	lua_pop(L, 1);

	UiEvent events[] = {
		{"changed", changed},
		{"hovered", hovered},
	};
	ui_push_widget(L, events, 2);
	return 1;
}

const LuaApiFunction &ui_dropdown_fn() {
	static const LuaApiFunction fn = {"dropdown", l_dropdown};
	return fn;
}
