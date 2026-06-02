#pragma once

#include <functional>

bool egl_hook_install();
void egl_hook_configure(bool overlays_enabled, int toggle_vk);
void egl_hook_shutdown();

void egl_hook_set_toggle_key(int vk);
int egl_hook_get_toggle_key();
void egl_hook_start_keybind_capture();
bool egl_hook_is_capturing_keybind();
int egl_hook_poll_captured_key();

using egl_frame_id_t = int;

egl_frame_id_t egl_hook_register_frame(std::function<void()> fn);
void egl_hook_unregister_frame(egl_frame_id_t id);

void egl_hook_set_imgui_frame(void (*fn)());
