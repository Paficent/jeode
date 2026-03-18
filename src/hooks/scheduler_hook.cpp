#include "scheduler_hook.h"
#include "../core/memory.h"

#include <spdlog/spdlog.h>

#include "MinHook.h"
#include <windows.h>

#include <deque>
#include <functional>
#include <mutex>
#include <vector>

//   55              PUSH EBP
//   8B EC           MOV EBP, ESP
//   83 EC ??        SUB ESP, <frame_size>
//   53              PUSH EBX
//   56              PUSH ESI
//   57              PUSH EDI
//   8B F9           MOV EDI, ECX
//   8B 77 ??        MOV ESI, [EDI + ??]
//   0F B6 46 ??     MOVZX EAX, byte [ESI + ??]
//   83 F8 ??        CMP EAX, <max_case>
//   0F 87 ?? ?? ?? ??  JA default
//   FF 24 85 ?? ?? ?? ??  JMP [EAX*4 + <table>]
//   E8 ?? ?? ?? ??  CALL <case0_fn>
//   5F 5E 33 C0 5B  POP EDI; POP ESI; XOR EAX,EAX; POP EBX
//   8B E5 5D C3     epilogue
//   83 7E ?? 00     CMP dword [ESI + ??], 0
//   74 ??           JZ
//   8B CE           MOV ECX, ESI
//   E8 ?? ?? ?? ??  CALL <case1_fn>
//   5F 5E 5B        POP EDI; POP ESI; POP EBX
//   8B E5 5D C3     epilogue
//   E8 ?? ?? ?? ??  CALL <case1_jz_fn>
//   5F 5E 33 C0 5B  POP EDI; POP ESI; XOR EAX,EAX; POP EBX
//   8B E5 5D C3     epilogue
static const unsigned char PATTERN_SCHEDULER_TICK[] = "\x55\x8B\xEC\x83\xEC\x00\x53\x56\x57\x8B\xF9\x8B\x77\x00"
													  "\x0F\xB6\x46\x00\x83\xF8\x00\x0F\x87\x00\x00\x00\x00"
													  "\xFF\x24\x85\x00\x00\x00\x00\xE8\x00\x00\x00\x00"
													  "\x5F\x5E\x33\xC0\x5B\x8B\xE5\x5D\xC3"
													  "\x83\x7E\x00\x00\x74\x00\x8B\xCE\xE8\x00\x00\x00\x00"
													  "\x5F\x5E\x5B\x8B\xE5\x5D\xC3"
													  "\xE8\x00\x00\x00\x00\x5F\x5E\x33\xC0\x5B\x8B\xE5\x5D\xC3";

static const char MASK_SCHEDULER_TICK[] = "xxxxx?xxxxxxx?"
										  "xxx?xx?xx????xxx????x????"
										  "xxxxxxxxx"
										  "xx?xx?xxx????"
										  "xxxxxxx"
										  "x????xxxxxxxxx";

static const uint32_t RVA_SCHEDULER_TICK = 0x0020c190;

typedef int(__fastcall *scheduler_tick_t)(void *thisPtr, void *edx_unused);

static scheduler_tick_t g_orig_scheduler_tick = nullptr;
static void *g_scheduler_tick_addr = nullptr;

static std::mutex g_queue_mutex;
static std::deque<std::function<void()>> g_work_queue;

static int __fastcall hooked_scheduler_tick(void *thisPtr, void *edx_unused) {
	int result = g_orig_scheduler_tick(thisPtr, edx_unused);

	std::vector<std::function<void()>> batch;
	{
		std::lock_guard<std::mutex> lock(g_queue_mutex);
		if (!g_work_queue.empty()) {
			spdlog::debug("[sched] draining {} work item(s)", g_work_queue.size());
			batch.assign(std::make_move_iterator(g_work_queue.begin()), std::make_move_iterator(g_work_queue.end()));
			g_work_queue.clear();
		}
	}
	for (size_t i = 0; i < batch.size(); i++) {
		spdlog::debug("[sched] executing work item {}/{}", i + 1, batch.size());
		try {
			batch[i]();
			spdlog::debug("[sched] work item {}/{} completed", i + 1, batch.size());
		} catch (const std::exception &e) {
			spdlog::error("[sched] work item {}/{} threw: {}", i + 1, batch.size(), e.what());
		} catch (...) {
			spdlog::error("[sched] work item {}/{} threw unknown exception", i + 1, batch.size());
		}
	}

	return result;
}

void scheduler_queue_work(std::function<void()> work) {
	std::lock_guard<std::mutex> lock(g_queue_mutex);
	g_work_queue.push_back(std::move(work));
}

bool scheduler_hook_install() {
	uintptr_t base = memory::base_address();

	uintptr_t textStart = 0;
	size_t textSize = 0;
	memory::get_text_section(base, &textStart, &textSize);

	uintptr_t addr = memory::scan_masked(textStart, textSize, PATTERN_SCHEDULER_TICK, MASK_SCHEDULER_TICK);
	if (addr) {
		spdlog::debug("[sched] found via pattern at {:#010x}", static_cast<unsigned>(addr));
	} else {
		addr = base + RVA_SCHEDULER_TICK;
		spdlog::debug("[sched] pattern miss, falling back to RVA {:#010x}", static_cast<unsigned>(addr));
	}

	g_scheduler_tick_addr = reinterpret_cast<void *>(addr);

	if (IsBadReadPtr(g_scheduler_tick_addr, 1)) {
		spdlog::error("[sched] address {} not readable", g_scheduler_tick_addr);
		return false;
	}

	auto *prologue = reinterpret_cast<uint8_t *>(g_scheduler_tick_addr);
	if (prologue[0] != 0x55) {
		spdlog::error("[sched] bad prologue {:#04x} at {}", prologue[0], g_scheduler_tick_addr);
		return false;
	}

	MH_STATUS s = MH_CreateHook(g_scheduler_tick_addr, reinterpret_cast<void *>(&hooked_scheduler_tick),
								reinterpret_cast<void **>(&g_orig_scheduler_tick));
	if (s != MH_OK) {
		spdlog::error("[sched] MH_CreateHook: {}", MH_StatusToString(s));
		return false;
	}

	s = MH_EnableHook(g_scheduler_tick_addr);
	if (s != MH_OK) {
		spdlog::error("[sched] MH_EnableHook: {}", MH_StatusToString(s));
		return false;
	}

	spdlog::debug("[sched] hook installed at {}", g_scheduler_tick_addr);
	return true;
}

void scheduler_hook_shutdown() {
	if (g_scheduler_tick_addr) MH_DisableHook(g_scheduler_tick_addr);
}
