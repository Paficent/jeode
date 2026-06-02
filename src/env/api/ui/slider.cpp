#include "widgets.h"

#include "imgui.h"

#include <cmath>
#include <cstdio>
#include <string>

static int l_slider(lua_State *L) {
	ui_require_in_frame(L, "ui.slider");
	const char *label = luaL_checkstring(L, 1);

	bool has_state = ui_is_state(L, 2);
	if (!has_state && !lua_isnoneornil(L, 2)) {
		return luaL_error(L, "ui.slider: 2nd arg must be a state (use ui.state(0))");
	}

	int opts_idx = lua_istable(L, 3) ? 3 : 0;

	double vmin = ui_opt_number(L, opts_idx, "min", 0.0);
	double vmax = ui_opt_number(L, opts_idx, "max", 100.0);
	double step = ui_opt_number(L, opts_idx, "step", 0.0);
	bool step_provided = step > 0.0;

	bool is_float;
	if (step_provided) {
		is_float = step != std::floor(step);
	} else {
		is_float = (vmin != std::floor(vmin)) || (vmax != std::floor(vmax));
	}

	int precision = 0;
	if (is_float) {
		precision = 3;
		if (step_provided) {
			double s = step;
			int p = 0;
			while (p < 6 && (s - std::floor(s)) > 1e-9) {
				s *= 10.0;
				p++;
			}
			if (p > 0) precision = p;
		}
	}

	std::string user_fmt = ui_opt_string(L, opts_idx, "format", "");
	char fmt_buf[16];
	const char *fmt;
	if (!user_fmt.empty()) {
		fmt = user_fmt.c_str();
	} else if (is_float) {
		std::snprintf(fmt_buf, sizeof(fmt_buf), "%%.%df", precision);
		fmt = fmt_buf;
	} else {
		fmt = "%d";
	}

	bool changed = false;
	bool hovered = false;

	if (is_float) {
		float val = static_cast<float>(vmin);
		if (has_state) {
			lua_rawgeti(L, 2, 1);
			if (lua_isnumber(L, -1)) val = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
		}
		changed = ImGui::SliderFloat(label, &val, static_cast<float>(vmin), static_cast<float>(vmax), fmt);
		hovered = ImGui::IsItemHovered();
		if (has_state && changed) {
			lua_pushnumber(L, val);
			ui_state_set_value(L, 2);
		}
	} else {
		int val = static_cast<int>(vmin);
		if (has_state) {
			lua_rawgeti(L, 2, 1);
			if (lua_isnumber(L, -1)) val = static_cast<int>(lua_tointeger(L, -1));
			lua_pop(L, 1);
		}
		changed = ImGui::SliderInt(label, &val, static_cast<int>(vmin), static_cast<int>(vmax), fmt);
		hovered = ImGui::IsItemHovered();
		if (has_state && changed) {
			lua_pushinteger(L, val);
			ui_state_set_value(L, 2);
		}
	}

	UiEvent events[] = {
		{"changed", changed},
		{"hovered", hovered},
	};
	ui_push_widget(L, events, 2);
	return 1;
}

const LuaApiFunction &ui_slider_fn() {
	static const LuaApiFunction fn = {"slider", l_slider};
	return fn;
}
