#pragma once

#include <functional>

bool scheduler_hook_install();
void scheduler_hook_shutdown();

void scheduler_queue_work(std::function<void()> work);

using scheduler_tick_id_t = int;
scheduler_tick_id_t scheduler_register_tick(std::function<void()> tick);
void scheduler_unregister_tick(scheduler_tick_id_t id);
