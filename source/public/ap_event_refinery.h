#ifndef _AP_EVENT_REFINERY_H_
#define _AP_EVENT_REFINERY_H_

#include "public/ap_base.h"
#include "public/ap_define.h"
#include "public/ap_event_manager.h"
#include "public/ap_module_instance.h"

#define AP_EVENT_REFINERY_MODULE_NAME "AgpmEventRefinery"

#define	AP_EVENT_REFINERY_MAX_USE_RANGE 1600

BEGIN_DECLS

struct ap_event_manager_event;

enum ap_event_refinery_packet_type {
	AP_EVENT_REFINERY_PACKET_REQUEST = 0,
	AP_EVENT_REFINERY_PACKET_GRANT,
};

enum ap_event_refinery_callback_id {
	AP_EVENT_REFINERY_CB_RECEIVE,
};

struct ap_event_refinery_cb_receive {
	enum ap_event_refinery_packet_type type;
	struct ap_event_manager_base_packet * event;
	void * user_data;
};

struct ap_event_refinery_module * ap_event_refinery_create_module();

void ap_event_refinery_add_callback(
	struct ap_event_refinery_module * mod,
	enum ap_event_refinery_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_event_refinery_on_receive(
	struct ap_event_refinery_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_event_refinery_make_packet(
	struct ap_event_refinery_module * mod,
	enum ap_event_refinery_packet_type type,
	const struct ap_event_manager_event * event,
	uint32_t * character_id);

END_DECLS

#endif /* _AP_EVENT_REFINERY_H_ */
