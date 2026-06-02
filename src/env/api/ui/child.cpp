#include "widgets.h"

#include "imgui.h"

#include <cstdio>

static int l_child(lua_State *L) {
	ui_require_in_frame(L, "ui.child");
	float width = static_cast<float>(luaL_optnumber(L, 1, 0.0));
	float height = static_cast<float>(luaL_optnumber(L, 2, 0.0));

	int opts_idx, body_idx;
	ui_resolve_opts_body(L, 3, &opts_idx, &body_idx);

	bool border = ui_opt_bool(L, opts_idx, "border", false);

	char id[32];
	std::snprintf(id, sizeof(id), "##child_%d", ui_next_anon_id());

	ImGuiChildFlags cflags = border ? ImGuiChildFlags_Borders : 0;
	bool visible = ImGui::BeginChild(id, ImVec2(width, height), cflags);

	int err = 0;
	bool hovered = false;
	if (visible) {
		if (body_idx != 0) err = ui_call_body(L, body_idx);
		if (err == 0) hovered = ImGui::IsWindowHovered();
	}

	ImGui::EndChild();

	if (err != 0) return lua_error(L);

	UiEvent events[] = {{"hovered", hovered}};
	ui_push_widget(L, events, 1);
	return 1;
}

const LuaApiFunction &ui_child_fn() {
	static const LuaApiFunction fn = {"child", l_child};
	return fn;
}
