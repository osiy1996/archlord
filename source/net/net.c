#include <net/net.h>
#include <net/internal.h>
#include <core/core.h>
#include <core/log.h>
#include <task/task.h>
#include <string.h>

boolean net_startup()
{
	if (!init_internal()) {
		ERROR("Failed to initialize network internals.");
		return FALSE;
	}
	return TRUE;
}
