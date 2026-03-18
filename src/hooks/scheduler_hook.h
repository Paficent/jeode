#pragma once

#include <functional>

bool scheduler_hook_install();
void scheduler_hook_shutdown();

void scheduler_queue_work(std::function<void()> work);
