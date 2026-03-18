#include "egl_hook.h"
#include "core/overlay.h"

#include <spdlog/spdlog.h>

#include "MinHook.h"
#include <windows.h>

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_win32.h"
#include "imgui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef void *EGLDisplay;
typedef void *EGLSurface;
typedef unsigned int EGLBoolean;
typedef EGLBoolean (*eglSwapBuffers_t)(EGLDisplay, EGLSurface);

static eglSwapBuffers_t g_orig_eglSwapBuffers = nullptr;
static void *g_eglSwapBuffers_addr = nullptr;

static HWND g_hwnd = nullptr;
static WNDPROC g_orig_wndproc = nullptr;
static bool g_initialized = false;
static bool g_visible = false;
static bool g_enabled = true;

static int g_toggle_vk = 0x70;
static bool g_capturing_keybind = false;
static int g_captured_vk = 0;
static DWORD g_last_toggle_tick = 0;
static constexpr DWORD TOGGLE_COOLDOWN_MS = 200;

struct FindCtx {
	DWORD pid;
	HWND result;
};

static BOOL CALLBACK enum_wnd_proc(HWND hwnd, LPARAM lp) {
	auto *c = reinterpret_cast<FindCtx *>(lp);
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid == c->pid && IsWindowVisible(hwnd)) {
		c->result = hwnd;
		return FALSE;
	}
	return TRUE;
}

static HWND find_game_window() {
	FindCtx ctx = {GetCurrentProcessId(), nullptr};
	EnumWindows(enum_wnd_proc, reinterpret_cast<LPARAM>(&ctx));
	return ctx.result;
}

static LRESULT CALLBACK hooked_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_KEYDOWN) {
		if (g_capturing_keybind) {
			g_captured_vk = static_cast<int>(wParam);
			g_capturing_keybind = false;
			return 1;
		}

		bool is_repeat = (lParam >> 30) & 1;
		if (!is_repeat && static_cast<int>(wParam) == g_toggle_vk) {
			DWORD now = GetTickCount();
			if (now - g_last_toggle_tick >= TOGGLE_COOLDOWN_MS) {
				g_visible = !g_visible;
				g_last_toggle_tick = now;
			}
			return 1;
		}
	}

	if (g_visible && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return 1;

	if (g_visible) {
		ImGuiIO &io = ImGui::GetIO();
		if ((io.WantCaptureMouse && (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)) ||
			(io.WantCaptureKeyboard && (msg >= WM_KEYFIRST && msg <= WM_KEYLAST)))
			return 1;
	}

	return CallWindowProcW(g_orig_wndproc, hWnd, msg, wParam, lParam);
}

static bool imgui_init() {
	g_hwnd = find_game_window();
	if (!g_hwnd) {
		spdlog::error("[egl] could not find game window");
		return false;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = nullptr;
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_InitForOpenGL(g_hwnd);
	ImGui_ImplOpenGL3_Init("#version 100");

	g_orig_wndproc = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hooked_wndproc)));

	spdlog::debug("[egl] imgui initialized, hwnd={}", (void *)g_hwnd);
	g_initialized = true;
	return true;
}

static void imgui_frame() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (g_visible) {
		overlay_draw();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static EGLBoolean hooked_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
	if (g_enabled) {
		if (!g_initialized) imgui_init();
		if (g_initialized) imgui_frame();
	}

	return g_orig_eglSwapBuffers(dpy, surface);
}

void egl_hook_configure(bool overlays_enabled, int toggle_vk) {
	g_enabled = overlays_enabled;
	g_toggle_vk = toggle_vk;
	spdlog::debug("[egl] overlays {}, toggle key 0x{:02X}", g_enabled ? "enabled" : "disabled", g_toggle_vk);
}

void egl_hook_set_toggle_key(int vk) {
	g_toggle_vk = vk;
}

int egl_hook_get_toggle_key() {
	return g_toggle_vk;
}

void egl_hook_start_keybind_capture() {
	g_captured_vk = 0;
	g_capturing_keybind = true;
}

bool egl_hook_is_capturing_keybind() {
	return g_capturing_keybind;
}

int egl_hook_poll_captured_key() {
	int vk = g_captured_vk;
	g_captured_vk = 0;
	return vk;
}

bool egl_hook_install() {
	HMODULE hEGL = GetModuleHandleA("libEGL.dll");
	if (!hEGL) {
		spdlog::debug("[egl] libEGL.dll not loaded");
		return false;
	}

	g_eglSwapBuffers_addr = reinterpret_cast<void *>(GetProcAddress(hEGL, "eglSwapBuffers"));
	if (!g_eglSwapBuffers_addr) {
		spdlog::error("[egl] eglSwapBuffers not found");
		return false;
	}

	MH_STATUS s = MH_CreateHook(g_eglSwapBuffers_addr, reinterpret_cast<void *>(&hooked_eglSwapBuffers),
								reinterpret_cast<void **>(&g_orig_eglSwapBuffers));
	if (s != MH_OK) {
		spdlog::error("[egl] MH_CreateHook: {}", MH_StatusToString(s));
		return false;
	}

	s = MH_EnableHook(g_eglSwapBuffers_addr);
	if (s != MH_OK) {
		spdlog::error("[egl] MH_EnableHook: {}", MH_StatusToString(s));
		return false;
	}

	spdlog::debug("[egl] hook installed at {}", g_eglSwapBuffers_addr);
	return true;
}

void egl_hook_shutdown() {
	if (g_initialized) {
		if (g_orig_wndproc && g_hwnd)
			SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_orig_wndproc));
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		g_initialized = false;
	}
	if (g_eglSwapBuffers_addr) MH_DisableHook(g_eglSwapBuffers_addr);
}
