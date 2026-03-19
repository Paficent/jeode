#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <windows.h>

namespace memory {

inline uintptr_t base_address() {
	return reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
}

inline void get_text_section(uintptr_t base, uintptr_t *outStart, size_t *outSize) {
	auto dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
	auto nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
	auto sec = IMAGE_FIRST_SECTION(nt);
	int num = nt->FileHeader.NumberOfSections;

	for (int i = 0; i < num; i++) {
		if (strncmp(reinterpret_cast<const char *>(sec[i].Name), ".text", 5) == 0) {
			*outStart = base + sec[i].VirtualAddress;
			*outSize = sec[i].Misc.VirtualSize;
			return;
		}
	}

	*outStart = base + 0x1000;
	*outSize = nt->OptionalHeader.SizeOfImage - 0x1000;
}

struct CompiledPattern {
	std::vector<uint8_t> bytes;
	std::vector<uint8_t> mask; // changed from bool to uint8_t
};

inline CompiledPattern compile_pattern(const char *pattern) {
	CompiledPattern out;
	const char *p = pattern;

	while (*p) {
		while (*p == ' ' || *p == '\t') ++p;
		if (*p == '\0') break;

		if (*p == '?') {
			out.bytes.push_back(0);
			out.mask.push_back(0); // false -> 0
			++p;
			if (*p == '?') ++p;
		} else {
			auto hex = [](char c) -> uint8_t {
				if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
				if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
				if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
				return 0;
			};
			uint8_t hi = hex(p[0]);
			uint8_t lo = hex(p[1]);
			out.bytes.push_back((hi << 4) | lo);
			out.mask.push_back(1); // true -> 1
			p += 2;
		}
	}

	return out;
}

inline uintptr_t pattern_scan(uintptr_t start, size_t size, const char *pattern) {
	CompiledPattern pat = compile_pattern(pattern);
	if (pat.bytes.empty()) return 0;

	const auto *region = reinterpret_cast<const uint8_t *>(start);
	const size_t patLen = pat.bytes.size();
	if (patLen > size) return 0;

	const uint8_t *patBytes = pat.bytes.data();
	const uint8_t *patMask = pat.mask.data(); // now safe

	size_t firstFixed = 0;
	while (firstFixed < patLen && !patMask[firstFixed]) ++firstFixed;
	if (firstFixed >= patLen) return start; // all wildcards

	const uint8_t firstByte = patBytes[firstFixed];
	const size_t scanEnd = size - patLen;

	for (size_t i = 0; i <= scanEnd; ++i) {
		if (region[i + firstFixed] != firstByte) continue;

		bool found = true;
		for (size_t j = 0; j < patLen; ++j) {
			if (patMask[j] && region[i + j] != patBytes[j]) {
				found = false;
				break;
			}
		}
		if (found) return start + i;
	}

	return 0;
}

} // namespace memory
