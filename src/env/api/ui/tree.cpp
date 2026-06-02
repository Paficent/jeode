#include "widgets.h"

#include "imgui.h"

static int l_tree(lua_State *L) {
	ui_require_in_frame(L, "ui.tree");
	const char *label = luaL_checkstring(L, 1);

	int opts_idx, body_idx;
	ui_resolve_opts_body(L, 2, &opts_idx, &body_idx);

	if (body_idx == 0) {
		return luaL_error(L, "ui.tree: function body required");
	}

	ImGuiTreeNodeFlags flags = 0;
	if (ui_opt_bool(L, opts_idx, "defaultOpen", false)) flags |= ImGuiTreeNodeFlags_DefaultOpen;
	if (ui_opt_bool(L, opts_idx, "leaf", false)) flags |= ImGuiTreeNodeFlags_Leaf;
	if (ui_opt_bool(L, opts_idx, "bullet", false)) flags |= ImGuiTreeNodeFlags_Bullet;
	if (ui_opt_bool(L, opts_idx, "framed", false)) flags |= ImGuiTreeNodeFlags_Framed;
	if (ui_opt_bool(L, opts_idx, "selected", false)) flags |= ImGuiTreeNodeFlags_Selected;
	if (ui_opt_bool(L, opts_idx, "openOnArrow", false)) flags |= ImGuiTreeNodeFlags_OpenOnArrow;
	if (ui_opt_bool(L, opts_idx, "openOnDoubleClick", false)) flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (ui_opt_bool(L, opts_idx, "spanFullWidth", false)) flags |= ImGuiTreeNodeFlags_SpanFullWidth;

	bool open = ImGui::TreeNodeEx(label, flags);
	bool hovered = ImGui::IsItemHovered();
	bool clicked = ImGui::IsItemClicked();

	int err = 0;
	if (open) {
		err = ui_call_body(L, body_idx);
		ImGui::TreePop();
	}

	if (err != 0) return lua_error(L);

	UiEvent events[] = {
		{"open", open},
		{"clicked", clicked},
		{"hovered", hovered},
	};
	ui_push_widget(L, events, 3);
	return 1;
}

const LuaApiFunction &ui_tree_fn() {
	static const LuaApiFunction fn = {"tree", l_tree};
	return fn;
}
