#include "core/keybind.h"

#include <algorithm>
#include <cctype>

struct KeyEntry {
	const char *name;
	int vk;
};

static const KeyEntry KEY_TABLE[] = {
	{"F1", 0x70},
	{"F2", 0x71},
	{"F3", 0x72},
	{"F4", 0x73},
	{"F5", 0x74},
	{"F6", 0x75},
	{"F7", 0x76},
	{"F8", 0x77},
	{"F9", 0x78},
	{"F10", 0x79},
	{"F11", 0x7A},
	{"F12", 0x7B},
	{"Insert", 0x2D},
	{"Delete", 0x2E},
	{"Home", 0x24},
	{"End", 0x23},
	{"PageUp", 0x21},
	{"PageDown", 0x22},
	{"Pause", 0x13},
	{"ScrollLock", 0x91},
	{"NumLock", 0x90},
	{"Tab", 0x09},
	{"CapsLock", 0x14},
	{"Backspace", 0x08},
	{"Tilde", 0xC0},
	{"Minus", 0xBD},
	{"Equals", 0xBB},
	{"LeftBracket", 0xDB},
	{"RightBracket", 0xDD},
	{"Backslash", 0xDC},
	{"Semicolon", 0xBA},
	{"Quote", 0xDE},
	{"Comma", 0xBC},
	{"Period", 0xBE},
	{"Slash", 0xBF},
	{"Numpad0", 0x60},
	{"Numpad1", 0x61},
	{"Numpad2", 0x62},
	{"Numpad3", 0x63},
	{"Numpad4", 0x64},
	{"Numpad5", 0x65},
	{"Numpad6", 0x66},
	{"Numpad7", 0x67},
	{"Numpad8", 0x68},
	{"Numpad9", 0x69},
	{"NumpadMultiply", 0x6A},
	{"NumpadAdd", 0x6B},
	{"NumpadSubtract", 0x6D},
	{"NumpadDecimal", 0x6E},
	{"NumpadDivide", 0x6F},
};

static constexpr size_t KEY_TABLE_SIZE = sizeof(KEY_TABLE) / sizeof(KEY_TABLE[0]);

static bool iequals(const std::string &a, const std::string &b) {
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); i++) {
		if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
			return false;
	}
	return true;
}

int keybind_name_to_vk(const std::string &name) {
	for (size_t i = 0; i < KEY_TABLE_SIZE; i++) {
		if (iequals(name, KEY_TABLE[i].name)) return KEY_TABLE[i].vk;
	}
	return 0;
}

std::string keybind_vk_to_name(int vk) {
	for (size_t i = 0; i < KEY_TABLE_SIZE; i++) {
		if (KEY_TABLE[i].vk == vk) return KEY_TABLE[i].name;
	}
	return "";
}

static const int UNBINDABLE_KEYS[] = {
	0x1B, 0x0D, 0x10, 0x11, 0x12, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0x5B, 0x5C,
};

bool keybind_is_bindable(int vk) {
	for (int blocked : UNBINDABLE_KEYS) {
		if (vk == blocked) return false;
	}
	return keybind_vk_to_name(vk).empty() == false;
}
