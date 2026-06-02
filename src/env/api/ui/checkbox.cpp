#include "widgets.h"

#include "imgui.h"

static int l_checkbox(lua_State *L) {
	ui_require_in_frame(L, "ui.checkbox");
	const char *label = luaL_checkstring(L, 1);

	bool has_state = ui_is_state(L, 2);
	if (!has_state && !lua_isnoneornil(L, 2)) {
		return luaL_error(L, "ui.checkbox: 2nd arg must be a state (use ui.state(false))");
	}

	bool value = false;
	if (has_state) {
		lua_rawgeti(L, 2, 1);
		value = lua_toboolean(L, -1) != 0;
		lua_pop(L, 1);
	}

	bool changed = ImGui::Checkbox(label, &value);
	bool hovered = ImGui::IsItemHovered();

	if (has_state && changed) {
		lua_pushboolean(L, value ? 1 : 0);
		ui_state_set_value(L, 2);
	}

	UiEvent events[] = {
		{"changed", changed},
		{"hovered", hovered},
	};
	ui_push_widget(L, events, 2);
	return 1;
}

const LuaApiFunction &ui_checkbox_fn() {
	static const LuaApiFunction fn = {"checkbox", l_checkbox};
	return fn;
}
