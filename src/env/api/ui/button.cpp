#include "widgets.h"

#include "imgui.h"

static int l_button(lua_State *L) {
	ui_require_in_frame(L, "ui.button");
	const char *label = luaL_checkstring(L, 1);

	bool clicked = ImGui::Button(label);
	bool hovered = ImGui::IsItemHovered();

	UiEvent events[] = {
		{"clicked", clicked},
		{"hovered", hovered},
	};
	ui_push_widget(L, events, 2);
	return 1;
}

const LuaApiFunction &ui_button_fn() {
	static const LuaApiFunction fn = {"button", l_button};
	return fn;
}
