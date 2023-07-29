#ifndef _CORE_INTERNAL_H_
#define _CORE_INTERNAL_H_

#include "core/macros.h"
#include "core/os.h"

BEGIN_DECLS

struct core_module {
	boolean shutdown_signal;
	mutex_t shutdown_mutex;
};

boolean set_signals_handlers(struct core_module * cc);

extern struct core_module * g_CoreModule;

END_DECLS

#endif /* _CORE_INTERNAL_H_ */