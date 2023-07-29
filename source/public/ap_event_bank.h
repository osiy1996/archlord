#ifndef _AP_EVENT_BANK_H_
#define _AP_EVENT_BANK_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_event_manager.h"
#include "public/ap_module_instance.h"

#define AP_EVENT_BANK_MODULE_NAME "AgpmEventBank"

#define	AP_EVENT_BANK_MAX_USE_RANGE 600

BEGIN_DECLS

enum ap_event_bank_packet_type {
	AP_EVENT_BANK_PACKET_REQUEST = 0,
	AP_EVENT_BANK_PACKET_GRANT,
	AP_EVENT_BANK_PACKET_GRANT_ANY,
};

enum ap_event_bank_callback_id {
	AP_EVENT_BANK_CB_RECEIVE,
};

struct ap_event_bank_cb_receive {
	enum ap_event_bank_packet_type type;
	struct ap_event_manager_base_packet * event;
	void * user_data;
};

struct ap_event_bank_module * ap_event_bank_create_module();

void ap_event_bank_add_callback(
	struct ap_event_bank_module * mod,
	enum ap_event_bank_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_event_bank_on_receive(
	struct ap_event_bank_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_event_bank_make_grant_packet(
	struct ap_event_bank_module * mod,
	const struct ap_event_manager_event * event,
	uint32_t character_id);

void ap_event_bank_make_grant_anywhere_packet(
	struct ap_event_bank_module * mod,
	uint32_t character_id);

END_DECLS

#endif /* _AP_EVENT_BANK_H_ */
