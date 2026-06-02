#include "widgets.h"

#include "imgui.h"

static int l_tooltip(lua_State *L) {
	ui_require_in_frame(L, "ui.tooltip");
	const char *text = luaL_checkstring(L, 1);

	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", text);
	}
	return 0;
}

const LuaApiFunction &ui_tooltip_fn() {
	static const LuaApiFunction fn = {"tooltip", l_tooltip};
	return fn;
}
