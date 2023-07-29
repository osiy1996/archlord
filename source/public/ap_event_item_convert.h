#ifndef _AP_EVENT_ITEM_CONVERT_H_
#define _AP_EVENT_ITEM_CONVERT_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_event_manager.h"
#include "public/ap_module_instance.h"

#define AP_EVENT_ITEM_CONVERT_MODULE_NAME "AgpmEventItemConvert"

#define	AP_EVENT_ITEM_CONVERT_MAX_USE_RANGE 600

BEGIN_DECLS

enum ap_event_item_convert_packet_type {
	AP_EVENT_ITEM_CONVERT_PACKET_NONE = -1,
	AP_EVENT_ITEM_CONVERT_PACKET_REQUEST,
	AP_EVENT_ITEM_CONVERT_PACKET_GRANT,
	AP_EVENT_ITEM_CONVERT_PACKET_REJECT
};

enum ap_event_item_convert_callback_id {
	AP_EVENT_ITEM_CONVERT_CB_RECEIVE,
};

struct ap_event_item_convert_cb_receive {
	enum ap_event_item_convert_packet_type type;
	struct ap_event_manager_base_packet * event;
	uint32_t character_id;
	void * user_data;
};

struct ap_event_item_convert_module * ap_event_item_convert_create_module();

void ap_event_item_convert_add_callback(
	struct ap_event_item_convert_module * mod,
	enum ap_event_item_convert_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_event_item_convert_on_receive(
	struct ap_event_item_convert_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_event_item_convert_make_packet(
	struct ap_event_item_convert_module * mod,
	enum ap_event_item_convert_packet_type type,
	const struct ap_event_manager_event * event,
	uint32_t character_id);

END_DECLS

#endif /* _AP_EVENT_ITEM_CONVERT_H_ */
