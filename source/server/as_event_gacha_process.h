#ifndef _AS_EVENT_GACHA_PROCESS_H_
#define _AS_EVENT_GACHA_PROCESS_H_

#include "core/macros.h"
#include "core/types.h"

#define AS_EVENT_GACHA_PROCESS_MODULE_NAME "AgsmEventGachaProcess"

BEGIN_DECLS

struct as_event_gacha_process_module * as_event_gacha_process_create_module();

void as_event_gacha_process_handle_pending_rolls(
	struct as_event_gacha_process_module * mod);

END_DECLS

#endif /* _AS_EVENT_GACHA_PROCESS_H_ */
