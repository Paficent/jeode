#pragma once

bool egl_hook_install();
void egl_hook_configure(bool overlays_enabled, int toggle_vk);
void egl_hook_shutdown();

void egl_hook_set_toggle_key(int vk);
int egl_hook_get_toggle_key();
void egl_hook_start_keybind_capture();
bool egl_hook_is_capturing_keybind();
int egl_hook_poll_captured_key();
