#ifndef _AP_EVENT_GACHA_H_
#define _AP_EVENT_GACHA_H_

#include "public/ap_module_instance.h"

#define AP_EVENT_GACHA_MODULE_NAME "AgpmEventGacha"

BEGIN_DECLS

struct ap_event_manager_event;

struct ap_event_gacha_event {
	uint32_t gacha_type;
};

struct ap_event_gacha_module * ap_event_gacha_create_module();

struct ap_event_gacha_event * ap_event_gacha_get_event(
	struct ap_event_manager_event * e);

END_DECLS

#endif /* _AP_EVENT_GACHA_H_ */
