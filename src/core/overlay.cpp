#include "core/overlay.h"
#include "core/config.h"
#include "core/keybind.h"
#include "env/environment.h"
#include "hooks/egl_hook.h"

#include "TextEditor.h"
#include "imgui.h"

#include <cstring>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>

namespace fs = std::filesystem;

struct LogBuffer {
	static constexpr size_t MAX_LINES = 1024;

	std::mutex mutex;
	std::deque<std::string> lines;
	bool pending_scroll = false;

	void append(const std::string &line) {
		std::lock_guard<std::mutex> lock(mutex);
		lines.push_back(line);
		while (lines.size() > MAX_LINES) lines.pop_front();
		pending_scroll = true;
	}

	void clear() {
		std::lock_guard<std::mutex> lock(mutex);
		lines.clear();
	}

	void draw() {
		std::lock_guard<std::mutex> lock(mutex);
		for (const auto &line : lines) {
			ImVec4 color = color_for_line(line);
			bool colored = (color.w > 0.0f);
			if (colored) ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::TextUnformatted(line.c_str());
			if (colored) ImGui::PopStyleColor();
		}
		if (pending_scroll) {
			ImGui::SetScrollHereY(1.0f);
			pending_scroll = false;
		}
	}

  private:
	static ImVec4 color_for_line(const std::string &line) {
		if (line.compare(0, 10, "[critical]") == 0) return {1.0f, 0.2f, 0.2f, 1.0f};
		if (line.compare(0, 7, "[error]") == 0) return {1.0f, 0.3f, 0.3f, 1.0f};
		if (line.compare(0, 9, "[warning]") == 0) return {1.0f, 0.8f, 0.3f, 1.0f};
		if (line.compare(0, 6, "[info]") == 0) return {0.7f, 0.9f, 0.7f, 1.0f};
		if (line.compare(0, 7, "[debug]") == 0) return {0.6f, 0.6f, 0.6f, 1.0f};
		if (line.compare(0, 7, "[trace]") == 0) return {0.5f, 0.5f, 0.5f, 1.0f};
		return {0.0f, 0.0f, 0.0f, 0.0f};
	}
};

static fs::path s_jeode_dir;
static LogBuffer s_executor_log;
static LogBuffer s_log;

static TextEditor s_editor;
static bool s_editor_initialized = false;
static float s_editor_ratio = 0.4f;

static void draw_executor_tab() {
	if (!s_editor_initialized) {
		s_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
		s_editor.SetPalette(TextEditor::GetDarkPalette());
		s_editor.SetShowWhitespaces(false);
		s_editor.SetTabSize(2);
		s_editor_initialized = true;
	}

	float available = ImGui::GetContentRegionAvail().y;
	float button_row_height = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	float splitter_height = 4.0f;
	float usable = available - button_row_height - splitter_height - ImGui::GetStyle().ItemSpacing.y * 2.0f;
	float editor_height = usable * s_editor_ratio;
	float output_height = usable - editor_height;

	s_editor.Render("##editor", ImVec2(-1.0f, editor_height), true);

	ImGui::InvisibleButton("##splitter", ImVec2(-1.0f, splitter_height));
	if (ImGui::IsItemActive()) {
		float delta = ImGui::GetIO().MouseDelta.y / usable;
		s_editor_ratio += delta;
		if (s_editor_ratio < 0.1f) s_editor_ratio = 0.1f;
		if (s_editor_ratio > 0.9f) s_editor_ratio = 0.9f;
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

	bool should_execute = false;
	if (ImGui::Button("Run")) should_execute = true;

	ImGui::SameLine();
	ImGui::TextDisabled("(Ctrl+Enter)");

	ImGui::SameLine(ImGui::GetWindowWidth() - 60.0f);
	if (ImGui::Button("Clear")) s_executor_log.clear();

	if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter)) should_execute = true;

	if (should_execute) {
		std::string code = s_editor.GetText();
		while (!code.empty() &&
			   (code.back() == '\n' || code.back() == '\r' || code.back() == ' ' || code.back() == '\t'))
			code.pop_back();
		if (!code.empty()) get_environment().execute(code);
	}

	if (ImGui::BeginChild("executor_output", ImVec2(0, output_height), ImGuiChildFlags_Borders)) {
		s_executor_log.draw();
	}
	ImGui::EndChild();
}

static void draw_log_tab() {
	if (ImGui::Button("Copy")) {
		std::lock_guard<std::mutex> lock(s_log.mutex);
		std::string all;
		for (const auto &line : s_log.lines) {
			all += line;
			all += '\n';
		}
		ImGui::SetClipboardText(all.c_str());
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear")) s_log.clear();

	if (ImGui::BeginChild("debug_output", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
		s_log.draw();
	}
	ImGui::EndChild();
}

struct ConfigState {
	JeodeConfig live;
	JeodeConfig saved;
	bool loaded = false;
	float save_feedback_timer = 0.0f;

	void load() {
		if (s_jeode_dir.empty()) return;
		live = config_load(s_jeode_dir);
		saved = live;
		loaded = true;
	}

	void save() {
		if (s_jeode_dir.empty()) return;
		config_save(live, s_jeode_dir);
		saved = live;
		save_feedback_timer = 2.0f;
	}

	void revert() { live = saved; }

	bool is_dirty() const {
		return live.overlays_enabled != saved.overlays_enabled || live.debug != saved.debug ||
			   live.enable_native_mods != saved.enable_native_mods ||
			   live.allow_unsafe_functions != saved.allow_unsafe_functions || live.toggle_key != saved.toggle_key;
	}
};

static ConfigState s_config;

static void draw_config_tab() {
	if (!s_config.loaded) s_config.load();

	if (!s_config.loaded) {
		ImGui::TextDisabled("Config not available (jeode directory not set).");
		return;
	}

	ImGui::TextDisabled("Config: %s", (s_jeode_dir / "config.json").string().c_str());
	ImGui::TextDisabled("Note: some changes take effect on next launch.");
	ImGui::Spacing();

	ImGui::Checkbox("Overlays Enabled", &s_config.live.overlays_enabled);
	ImGui::Checkbox("Debug Mode", &s_config.live.debug);
	ImGui::Checkbox("Enable Native Mods", &s_config.live.enable_native_mods);
	if (s_config.live.enable_native_mods)
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "Native mods allow arbitrary code execution.");
	ImGui::Checkbox("Allow Unsafe Functions", &s_config.live.allow_unsafe_functions);

	ImGui::Spacing();

	std::string current_name = keybind_vk_to_name(s_config.live.toggle_key);
	if (current_name.empty()) current_name = "Unknown";

	if (egl_hook_is_capturing_keybind()) {
		ImGui::Text("Toggle Key:");
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "Press a key...");
	} else {
		int captured = egl_hook_poll_captured_key();
		if (captured != 0 && keybind_is_bindable(captured)) s_config.live.toggle_key = captured;

		ImGui::Text("Toggle Key:");
		ImGui::SameLine();
		ImGui::Text("%s", current_name.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Change")) egl_hook_start_keybind_capture();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	bool dirty = s_config.is_dirty();

	if (!dirty) ImGui::BeginDisabled();

	if (ImGui::Button("Save")) {
		egl_hook_set_toggle_key(s_config.live.toggle_key);
		get_environment().sandbox().set_allow_unsafe(s_config.live.allow_unsafe_functions);
		s_config.save();
		overlay_log("[info] config saved");
	}

	ImGui::SameLine();
	if (ImGui::Button("Revert")) {
		s_config.revert();
		egl_hook_set_toggle_key(s_config.live.toggle_key);
	}

	if (!dirty) ImGui::EndDisabled();

	if (dirty) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Unsaved changes");
	}

	if (s_config.save_feedback_timer > 0.0f) {
		s_config.save_feedback_timer -= ImGui::GetIO().DeltaTime;
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Saved!");
	}

	ImGui::Spacing();
	ImGui::Separator();
}

void overlay_init(const fs::path &jeodeDir) {
	s_jeode_dir = jeodeDir;
}

void overlay_executor_log(const std::string &line) {
	s_executor_log.append(line);
}

void overlay_log(const std::string &line) {
	s_log.append(line);
}

void overlay_draw() {
	ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("jeode")) {
		ImGui::End();
		return;
	}

	if (ImGui::BeginTabBar("##tabs")) {
		if (ImGui::BeginTabItem("Executor")) {
			draw_executor_tab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Logs")) {
			draw_log_tab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Config")) {
			draw_config_tab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}
