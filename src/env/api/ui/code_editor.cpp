#include "widgets.h"
#include <cstdint>

#include "TextEditor.h"
#include "imgui.h"

#include <string>
#include <unordered_map>

static std::unordered_map<ImGuiID, TextEditor> &editors() {
	static std::unordered_map<ImGuiID, TextEditor> m;
	return m;
}

static const TextEditor::LanguageDefinition &lang_from_name(const std::string &name) {
	if (name == "C++" || name == "cpp" || name == "cxx") return TextEditor::LanguageDefinition::CPlusPlus();
	if (name == "C" || name == "c") return TextEditor::LanguageDefinition::C();
	if (name == "HLSL" || name == "hlsl") return TextEditor::LanguageDefinition::HLSL();
	if (name == "GLSL" || name == "glsl") return TextEditor::LanguageDefinition::GLSL();
	if (name == "SQL" || name == "sql") return TextEditor::LanguageDefinition::SQL();
	if (name == "AngelScript" || name == "angelscript") return TextEditor::LanguageDefinition::AngelScript();
	return TextEditor::LanguageDefinition::Lua();
}

static const TextEditor::Palette &palette_from_name(const std::string &name) {
	if (name == "light") return TextEditor::GetLightPalette();
	if (name == "retroblue" || name == "retro") return TextEditor::GetRetroBluePalette();
	return TextEditor::GetDarkPalette();
}

static int l_code_editor(lua_State *L) {
	ui_require_in_frame(L, "ui.codeEditor");
	const char *label = luaL_checkstring(L, 1);

	bool has_state = ui_is_state(L, 2);
	if (!has_state && !lua_isnoneornil(L, 2)) {
		return luaL_error(L, "ui.codeEditor: 2nd arg must be a state (use ui.state(\"\"))");
	}

	int opts_idx = lua_istable(L, 3) ? 3 : 0;

	std::string lang = ui_opt_string(L, opts_idx, "language", "Lua");
	std::string palette = ui_opt_string(L, opts_idx, "palette", "dark");
	float width = static_cast<float>(ui_opt_number(L, opts_idx, "width", 0.0));
	float height = static_cast<float>(ui_opt_number(L, opts_idx, "height", 300.0));
	bool readOnly = ui_opt_bool(L, opts_idx, "readOnly", false);
	bool showWhitespaces = ui_opt_bool(L, opts_idx, "showWhitespaces", true);
	int tabSize = ui_opt_integer(L, opts_idx, "tabSize", 4);
	bool border = ui_opt_bool(L, opts_idx, "border", true);
	bool handleKeyboard = ui_opt_bool(L, opts_idx, "handleKeyboard", true);
	bool handleMouse = ui_opt_bool(L, opts_idx, "handleMouse", true);
	bool colorizer = ui_opt_bool(L, opts_idx, "colorizer", true);

	ImGuiID id = ImGui::GetID(label);
	auto &map = editors();
	auto it = map.find(id);
	bool created = it == map.end();
	if (created) {
		it = map.emplace(std::piecewise_construct, std::forward_as_tuple(id), std::forward_as_tuple()).first;
		it->second.SetLanguageDefinition(lang_from_name(lang));
		it->second.SetPalette(palette_from_name(palette));
	}
	TextEditor &editor = it->second;

	editor.SetReadOnly(readOnly);
	editor.SetShowWhitespaces(showWhitespaces);
	editor.SetTabSize(tabSize);
	editor.SetColorizerEnable(colorizer);
	editor.SetHandleKeyboardInputs(handleKeyboard);
	editor.SetHandleMouseInputs(handleMouse);

	if (has_state && created) {
		lua_rawgeti(L, 2, 1);
		const char *v = lua_tostring(L, -1);
		if (v) editor.SetText(v);
		lua_pop(L, 1);
	}

	editor.Render(label, ImVec2(width, height), border);

	bool hovered = ImGui::IsItemHovered();
	bool focused = ImGui::IsItemFocused();
	bool changed = editor.IsTextChanged();

	if (has_state && changed) {
		std::string content = editor.GetText();
		lua_pushstring(L, content.c_str());
		ui_state_set_value(L, 2);
	}

	UiEvent events[] = {
		{"changed", changed},
		{"hovered", hovered},
		{"focused", focused},
	};
	ui_push_widget(L, events, 3);
	return 1;
}

const LuaApiFunction &ui_code_editor_fn() {
	static const LuaApiFunction fn = {"codeEditor", l_code_editor};
	return fn;
}
