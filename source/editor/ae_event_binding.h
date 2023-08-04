#ifndef _AE_EVENT_BINDING_H_
#define _AE_EVENT_BINDING_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_event_manager.h"

#define AE_EVENT_BINDING_MODULE_NAME "AgemEventBinding"

BEGIN_DECLS

struct ae_event_binding_module * ae_event_binding_create_module();

boolean ae_event_binding_render_as_node(
	struct ae_event_binding_module * mod,
	void * source,
	struct ap_event_manager_attachment * attachment);

END_DECLS

#endif /* _AE_EVENT_BINDING_H_ */

