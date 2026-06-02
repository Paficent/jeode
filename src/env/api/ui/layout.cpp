#include "widgets.h"

#include "imgui.h"

static int l_separator(lua_State *L) {
	ui_require_in_frame(L, "ui.separator");
	ImGui::Separator();
	return 0;
}

static int l_same_line(lua_State *L) {
	ui_require_in_frame(L, "ui.sameLine");
	float offset = static_cast<float>(luaL_optnumber(L, 1, 0.0));
	float spacing = static_cast<float>(luaL_optnumber(L, 2, -1.0));
	ImGui::SameLine(offset, spacing);
	return 0;
}

static int l_spacing(lua_State *L) {
	ui_require_in_frame(L, "ui.spacing");
	ImGui::Spacing();
	return 0;
}

static int l_indent(lua_State *L) {
	ui_require_in_frame(L, "ui.indent");

	if (!lua_isfunction(L, 1)) {
		return luaL_error(L, "ui.indent: expected a function body");
	}

	ImGui::Indent();
	int err = ui_call_body(L, 1);
	ImGui::Unindent();

	if (err != 0) return lua_error(L);
	return 0;
}

const LuaApiFunction &ui_separator_fn() {
	static const LuaApiFunction fn = {"separator", l_separator};
	return fn;
}

const LuaApiFunction &ui_same_line_fn() {
	static const LuaApiFunction fn = {"sameLine", l_same_line};
	return fn;
}

const LuaApiFunction &ui_spacing_fn() {
	static const LuaApiFunction fn = {"spacing", l_spacing};
	return fn;
}

const LuaApiFunction &ui_indent_fn() {
	static const LuaApiFunction fn = {"indent", l_indent};
	return fn;
}
