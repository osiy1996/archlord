#include <assert.h>
#include <stdlib.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/os.h"
#include "core/string.h"

#include "public/ap_tick.h"

struct ap_tick_module { 
	struct ap_module_instance instance;
	timer_t timer;
	uint64_t main_thread_id;
};

struct ap_tick_module * ap_tick_create_module()
{
	struct ap_tick_module * mod = ap_module_instance_new("AgpmTick", sizeof(*mod),
		NULL, NULL, NULL, NULL);
	mod->timer = create_timer();
	if (!mod->timer) {
		ERROR("Failed to create timer.");
		return NULL;
	}
	mod->main_thread_id = get_current_thread_id();
	return mod;
}

uint64_t ap_tick_get(struct ap_tick_module * mod)
{
	uint64_t threadid = get_current_thread_id();
	if (threadid != mod->main_thread_id) {
		ERROR("ap_tick module can only be used in main thread.");
		assert(0);
		return 0;
	}
	return timer_delta_no_reset(mod->timer) / 1000;
}

