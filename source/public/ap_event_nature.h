#ifndef _AP_EVENT_NATURE_H_
#define _AP_EVENT_NATURE_H_

#include "public/ap_module_instance.h"

#define AP_EVENT_NATURE_MODULE_NAME "AgpmEventNature"

BEGIN_DECLS

struct ap_event_manager_event;

struct ap_event_nature_event {
	uint32_t sky_template_id;
};

struct ap_event_nature_module * ap_event_nature_create_module();

struct ap_event_nature_event * ap_event_nature_get_event(struct ap_event_manager_event * e);

END_DECLS

#endif /* _AP_EVENT_NATURE_H_ */
