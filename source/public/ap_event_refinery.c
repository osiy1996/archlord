#include "public/ap_event_refinery.h"

#include "core/log.h"

#include "public/ap_event_manager.h"
#include "public/ap_packet.h"

#include  "utility/au_packet.h"

struct ap_event_refinery_module {
	struct ap_module_instance instance;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
};

static boolean onregister(
	struct ap_event_refinery_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	if (!ap_event_manager_register_event(mod->ap_event_manager,
			AP_EVENT_MANAGER_FUNCTION_REFINERY, mod,
			NULL, NULL, NULL, NULL, NULL)) {
		ERROR("Failed to register event.");
		return FALSE;
	}
	return TRUE;
}

static void onshutdown(struct ap_event_refinery_module * mod)
{
}

struct ap_event_refinery_module * ap_event_refinery_create_module()
{
	struct ap_event_refinery_module * mod = ap_module_instance_new(AP_EVENT_REFINERY_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /* Packet Type */
		AU_PACKET_TYPE_PACKET, 1, /* Event Base Packet */
		AU_PACKET_TYPE_INT32, 1, /* CID */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_event_refinery_add_callback(
	struct ap_event_refinery_module * mod,
	enum ap_event_refinery_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

boolean ap_event_refinery_on_receive(
	struct ap_event_refinery_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_event_refinery_cb_receive cb = { 0 };
	const void * basepacket = NULL;
	struct ap_event_manager_base_packet event = { 0 };
	const char * targetpointname = NULL;
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type, /* Packet Type */
			&basepacket, /* Event Base Packet */
			NULL)) { /* CID */
		return FALSE;
	}
	if (basepacket) {
		if (!ap_event_manager_get_base_packet(mod->ap_event_manager, basepacket, &event))
			return FALSE;
		cb.event = &event;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_EVENT_REFINERY_CB_RECEIVE, &cb);
}

void ap_event_refinery_make_packet(
	struct ap_event_refinery_module * mod,
	enum ap_event_refinery_packet_type type,
	const struct ap_event_manager_event * event,
	uint32_t * character_id)
{
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * base = NULL;
	uint16_t length = 0;
	if (event) {
		base = ap_packet_get_temp_buffer(mod->ap_packet);
		ap_event_manager_make_base_packet(mod->ap_event_manager, event, base);
	}
	au_packet_make_packet(&mod->packet, buffer, 
		TRUE, &length, AP_EVENT_REFINERY_PACKET_TYPE,
		&type, /* Packet Type */ 
		base, /* Event Base */
		character_id); /* Character CID */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}
