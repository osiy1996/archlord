#ifndef _AE_EVENT_REFINERY_H_
#define _AE_EVENT_REFINERY_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_event_manager.h"

#define AE_EVENT_REFINERY_MODULE_NAME "AgemEventRefinery"

BEGIN_DECLS

struct ae_event_refinery_module * ae_event_refinery_create_module();

boolean ae_event_refinery_render_as_node(
	struct ae_event_refinery_module * mod,
	void * source,
	struct ap_event_manager_attachment * attachment);

END_DECLS

#endif /* _AE_EVENT_REFINERY_H_ */

