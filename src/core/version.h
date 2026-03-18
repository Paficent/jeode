#pragma once

#include <string>

#ifndef JEODE_VERSION
#define JEODE_VERSION "dev"
#endif

namespace version {

inline std::string strip_build_number(const std::string &ver) {
	auto pos = ver.rfind('.');
	if (pos == std::string::npos || pos == 0) return ver;

	for (size_t i = pos + 1; i < ver.size(); ++i) {
		if (ver[i] < '0' || ver[i] > '9') return ver;
	}

	int dots = 0;
	for (size_t i = 0; i < pos; ++i) {
		if (ver[i] == '.') ++dots;
	}
	if (dots < 2) return ver;

	return ver.substr(0, pos);
}

inline std::string game_version() {
	return strip_build_number(JEODE_VERSION);
}

} // namespace version
