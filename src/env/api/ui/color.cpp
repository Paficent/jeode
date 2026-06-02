#include "widgets.h"

#include "imgui.h"

static int l_color(lua_State *L) {
	ui_require_in_frame(L, "ui.color");
	const char *label = luaL_checkstring(L, 1);

	bool has_state = ui_is_state(L, 2);
	if (!has_state && !lua_isnoneornil(L, 2)) {
		return luaL_error(L, "ui.color: 2nd arg must be a state with {r, g, b, a?}");
	}

	int opts_idx = lua_istable(L, 3) ? 3 : 0;
	bool alpha = ui_opt_bool(L, opts_idx, "alpha", false);
	bool picker = ui_opt_bool(L, opts_idx, "picker", false);

	float col[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	if (has_state) {
		lua_rawgeti(L, 2, 1);
		if (lua_istable(L, -1)) {
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
			col[0] = read("r", 1, 1.0f);
			col[1] = read("g", 2, 1.0f);
			col[2] = read("b", 3, 1.0f);
			col[3] = read("a", 4, 1.0f);
		}
		lua_pop(L, 1);
	}

	bool changed = false;
	if (picker) {
		changed = alpha ? ImGui::ColorPicker4(label, col) : ImGui::ColorPicker3(label, col);
	} else {
		changed = alpha ? ImGui::ColorEdit4(label, col) : ImGui::ColorEdit3(label, col);
	}
	bool hovered = ImGui::IsItemHovered();

	if (has_state && changed) {
		int n = alpha ? 4 : 3;
		lua_createtable(L, n, n);
		for (int i = 0; i < n; i++) {
			lua_pushnumber(L, col[i]);
			lua_rawseti(L, -2, i + 1);
		}
		const char *names[4] = {"r", "g", "b", "a"};
		for (int i = 0; i < n; i++) {
			lua_pushnumber(L, col[i]);
			lua_setfield(L, -2, names[i]);
		}
		ui_state_set_value(L, 2);
	}

	UiEvent events[] = {
		{"changed", changed},
		{"hovered", hovered},
	};
	ui_push_widget(L, events, 2);
	return 1;
}

const LuaApiFunction &ui_color_fn() {
	static const LuaApiFunction fn = {"color", l_color};
	return fn;
}
