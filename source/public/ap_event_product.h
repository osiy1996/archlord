#ifndef _AP_EVENT_PRODUCT_H_
#define _AP_EVENT_PRODUCT_H_

#include "public/ap_module_instance.h"

#define AP_EVENT_PRODUCT_MODULE_NAME "AgpmEventProduct"

BEGIN_DECLS

struct ap_event_manager_event;

struct ap_event_product_event {
	uint32_t product_category;
};

struct ap_event_product_module * ap_event_product_create_module();

struct ap_event_product_event * ap_event_product_get_event(struct ap_event_manager_event * e);

END_DECLS

#endif /* _AP_EVENT_PRODUCT_H_ */
