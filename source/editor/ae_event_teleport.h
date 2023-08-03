#ifndef _AE_EVENT_TELEPORT_H_
#define _AE_EVENT_TELEPORT_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_event_manager.h"

#define AE_EVENT_TELEPORT_MODULE_NAME "AgemEventTeleport"

BEGIN_DECLS

struct ae_event_teleport_module * ae_event_teleport_create_module();

boolean ae_event_teleport_render_as_node(
	struct ae_event_teleport_module * mod,
	void * source,
	struct ap_event_manager_attachment * attachment);

END_DECLS

#endif /* _AE_EVENT_TELEPORT_H_ */

