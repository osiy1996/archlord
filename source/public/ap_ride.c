#include "public/ap_ride.h"

#include "core/log.h"

#include "public/ap_packet.h"

#include "utility/au_packet.h"

struct ap_ride_module {
	struct ap_module_instance instance;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_packet_module * ap_packet;
	struct ap_tick_module * ap_tick;
	struct au_packet packet;
};

static boolean onregister(
	struct ap_ride_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	return TRUE;
}

static void onclose(struct ap_ride_module * mod)
{
}

static void onshutdown(struct ap_ride_module * mod)
{
}

struct ap_ride_module * ap_ride_create_module()
{
	struct ap_ride_module * mod = ap_module_instance_new(AP_RIDE_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /* Packet Type */
		AU_PACKET_TYPE_INT32, 1, /* Character ID */
		AU_PACKET_TYPE_INT32, 1, /* Item ID (Ride Item) */
		AU_PACKET_TYPE_INT32, 1, /* Remain Time (sec) */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_ride_add_callback(
	struct ap_ride_module * mod,
	enum ap_ride_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

boolean ap_ride_on_receive(
	struct ap_ride_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_ride_cb_receive cb = { 0 };
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
		&cb.type, /* Packet Type */
		NULL, /* Character ID */
		NULL, /* Item ID (Ride Item) */
		NULL)) { /* Remain Time (sec) */
		return FALSE;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_RIDE_CB_RECEIVE, &cb);
}

void ap_ride_make_packet(
	struct ap_ride_module * mod, 
	enum ap_ride_packet_type type,
	uint32_t character_id,
	uint32_t item_id)
{
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_RIDE_PACKET_TYPE, 
		&type, /* Packet Type */
		&character_id, /* Character ID */
		&item_id, /* Item ID (Ride Item) */
		NULL); /* Remain Time (sec) */
	ap_packet_set_length(mod->ap_packet, length);
}
