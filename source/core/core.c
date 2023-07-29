#include "core/core.h"
#include "core/internal.h"
#include "core/log.h"
#include "core/malloc.h"
#include <string.h>

struct core_module * g_CoreModule;

boolean core_startup()
{
	struct core_module * cc = alloc(sizeof(*cc));
	memset(cc, 0, sizeof(*cc));
	cc->shutdown_mutex = create_mutex();
	if (!set_signals_handlers(cc)) {
		ERROR("Failed to set signal handlers.");
		return FALSE;
	}
	g_CoreModule = cc;
	return TRUE;
}

boolean core_should_shutdown()
{
	boolean shutdown;
	lock_mutex(g_CoreModule->shutdown_mutex);
	shutdown = g_CoreModule->shutdown_signal;
	unlock_mutex(g_CoreModule->shutdown_mutex);
	return shutdown;
}

void core_signal_shutdown()
{
	lock_mutex(g_CoreModule->shutdown_mutex);
	g_CoreModule->shutdown_signal = TRUE;
	unlock_mutex(g_CoreModule->shutdown_mutex);
}
