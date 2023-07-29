#ifndef _AP_AUCTION_H_
#define _AP_AUCTION_H_

#include "public/ap_module_instance.h"

#define AP_AUCTION_MODULE_NAME "AgpmAuction"

BEGIN_DECLS

struct ap_event_manager_event;

struct ap_auction_event {
	boolean has_auction_event;
};

struct ap_auction_module * ap_auction_create_module();

struct ap_auction_event * ap_auction_get_event(
	struct ap_auction_module * mod, 
	struct ap_event_manager_event * e);

END_DECLS

#endif /* _AP_AUCTION_H_ */
