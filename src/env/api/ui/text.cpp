#include "widgets.h"

#include "imgui.h"

static int l_text(lua_State *L) {
	ui_require_in_frame(L, "ui.text");
	const char *text = luaL_checkstring(L, 1);

	int opts_idx = lua_istable(L, 2) ? 2 : 0;

	bool wrapped = ui_opt_bool(L, opts_idx, "wrapped", false);
	float r = 1, g = 1, b = 1, a = 1;
	bool has_color = ui_opt_color(L, opts_idx, "color", r, g, b, a);

	if (has_color) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(r, g, b, a));
	if (wrapped) ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted(text);
	if (wrapped) ImGui::PopTextWrapPos();
	if (has_color) ImGui::PopStyleColor();

	bool hovered = ImGui::IsItemHovered();
	UiEvent events[] = {{"hovered", hovered}};
	ui_push_widget(L, events, 1);
	return 1;
}

const LuaApiFunction &ui_text_fn() {
	static const LuaApiFunction fn = {"text", l_text};
	return fn;
}
