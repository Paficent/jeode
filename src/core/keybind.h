#pragma once

#include <string>

int keybind_name_to_vk(const std::string &name);
std::string keybind_vk_to_name(int vk);
bool keybind_is_bindable(int vk);
