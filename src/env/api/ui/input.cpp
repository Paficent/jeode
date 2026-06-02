#include "widgets.h"

#include "imgui.h"

#include <cstring>

static int l_input(lua_State *L) {
	ui_require_in_frame(L, "ui.input");
	const char *label = luaL_checkstring(L, 1);

	bool has_state = ui_is_state(L, 2);
	if (!has_state && !lua_isnoneornil(L, 2)) {
		return luaL_error(L, "ui.input: 2nd arg must be a state (use ui.state(\"\"))");
	}

	int opts_idx = lua_istable(L, 3) ? 3 : 0;
	bool numeric = ui_opt_bool(L, opts_idx, "numeric", false);
	bool password = ui_opt_bool(L, opts_idx, "password", false);
	bool multiline = ui_opt_bool(L, opts_idx, "multiline", false);
	bool readOnly = ui_opt_bool(L, opts_idx, "readOnly", false);
	std::string hint = ui_opt_string(L, opts_idx, "hint", "");
	float height = static_cast<float>(ui_opt_number(L, opts_idx, "height", 0.0));

	char buf[2048] = {0};
	if (has_state) {
		lua_rawgeti(L, 2, 1);
		const char *v = lua_tostring(L, -1);
		if (v) {
			size_t n = strlen(v);
			if (n >= sizeof(buf)) n = sizeof(buf) - 1;
			memcpy(buf, v, n);
			buf[n] = '\0';
		}
		lua_pop(L, 1);
	}

	ImGuiInputTextFlags flags = 0;
	if (numeric) flags |= ImGuiInputTextFlags_CharsDecimal;
	if (password) flags |= ImGuiInputTextFlags_Password;
	if (readOnly) flags |= ImGuiInputTextFlags_ReadOnly;

	bool changed = false;
	if (multiline) {
		changed = ImGui::InputTextMultiline(label, buf, sizeof(buf), ImVec2(0, height), flags);
	} else if (!hint.empty()) {
		changed = ImGui::InputTextWithHint(label, hint.c_str(), buf, sizeof(buf), flags);
	} else {
		changed = ImGui::InputText(label, buf, sizeof(buf), flags);
	}

	bool hovered = ImGui::IsItemHovered();

	if (has_state && changed) {
		lua_pushstring(L, buf);
		ui_state_set_value(L, 2);
	}

	UiEvent events[] = {
		{"changed", changed},
		{"hovered", hovered},
	};
	ui_push_widget(L, events, 2);
	return 1;
}

const LuaApiFunction &ui_input_fn() {
	static const LuaApiFunction fn = {"input", l_input};
	return fn;
}
