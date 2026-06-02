#include "widgets.h"

#include "imgui.h"

static int l_tab_bar(lua_State *L) {
	ui_require_in_frame(L, "ui.tabBar");
	const char *id = luaL_checkstring(L, 1);

	int opts_idx, body_idx;
	ui_resolve_opts_body(L, 2, &opts_idx, &body_idx);

	if (body_idx == 0) {
		return luaL_error(L, "ui.tabBar: function body required");
	}

	bool is_open = ImGui::BeginTabBar(id);

	int err = 0;
	if (is_open) {
		err = ui_call_body(L, body_idx);
		ImGui::EndTabBar();
	}

	if (err != 0) return lua_error(L);
	return 0;
}

static int l_tab_item(lua_State *L) {
	ui_require_in_frame(L, "ui.tabItem");
	const char *label = luaL_checkstring(L, 1);

	int opts_idx, body_idx;
	ui_resolve_opts_body(L, 2, &opts_idx, &body_idx);

	if (body_idx == 0) {
		return luaL_error(L, "ui.tabItem: function body required");
	}

	bool is_selected = ImGui::BeginTabItem(label);

	int err = 0;
	if (is_selected) {
		err = ui_call_body(L, body_idx);
		ImGui::EndTabItem();
	}

	if (err != 0) return lua_error(L);
	return 0;
}

const LuaApiFunction &ui_tab_bar_fn() {
	static const LuaApiFunction fn = {"tabBar", l_tab_bar};
	return fn;
}

const LuaApiFunction &ui_tab_item_fn() {
	static const LuaApiFunction fn = {"tabItem", l_tab_item};
	return fn;
}
