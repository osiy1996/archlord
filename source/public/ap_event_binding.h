#ifndef _AP_EVENT_BINDING_H_
#define _AP_EVENT_BINDING_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_event_manager.h"
#include "public/ap_factors.h"

#define AP_EVENT_BINDING_MODULE_NAME "AgpmEventBinding"

#define AP_EVENT_BINDING_MAX_NAME 64
#define AP_EVENT_BINDING_MAX_TOWN_NAME 64

BEGIN_DECLS

struct ap_event_manager_event;

enum ap_event_binding_type {
	AP_EVENT_BINDING_TYPE_NONE = 0,
	AP_EVENT_BINDING_TYPE_RESURRECTION = 1,
	AP_EVENT_BINDING_TYPE_NEW_CHARACTER = 2,
	AP_EVENT_BINDING_TYPE_SIEGEWAR_OFFENSE = 3,
	AP_EVENT_BINDING_TYPE_SIEGEWAR_DEFENSE_INNER = 4,
	AP_EVENT_BINDING_TYPE_SIEGEWAR_ARCHLORD = 5,
	AP_EVENT_BINDING_TYPE_SIEGEWAR_ARCHLORD_ATTACKER = 6,
	AP_EVENT_BINDING_TYPE_SIEGEWAR_DEFENSE_OUTTER = 7,
	AP_EVENT_BINDING_TYPE_COUNT = 8
};

struct ap_event_binding {
	char binding_name[AP_EVENT_BINDING_MAX_NAME + 1];
	char town_name[AP_EVENT_BINDING_MAX_TOWN_NAME + 1];
	struct au_pos base_pos;
	uint32_t radius;
	struct ap_factors_char_type char_type;
	enum ap_event_binding_type binding_type;
};

struct ap_event_binding_event {
	uint32_t binding_id;
	struct ap_event_binding binding;
};

struct ap_event_binding_module * ap_event_binding_create_module();

static inline struct ap_event_binding_event * ap_event_binding_get_event(
	struct ap_event_manager_event * e)
{
	return (struct ap_event_binding_event *)e->data;
}

END_DECLS

#endif /* _AP_EVENT_BINDING_H_ */
