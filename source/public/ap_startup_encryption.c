#include "public/ap_startup_encryption.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_define.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"

struct ap_startup_encryption_module {
	struct ap_module_instance instance;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
};

static boolean onregister(
	struct ap_startup_encryption_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	return TRUE;
}

struct ap_startup_encryption_module * ap_startup_encryption_create_module()
{
	struct ap_startup_encryption_module * mod = ap_module_instance_new(AP_STARTUP_ENCRYPTION_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, 1,
		AU_PACKET_TYPE_UINT8, 1, /* Packet type */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* Data */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_startup_encryption_add_callback(
	struct ap_startup_encryption_module * mod,
	enum ap_startup_encryption_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

boolean ap_startup_encryption_on_receive(
	struct ap_startup_encryption_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	uint8_t type = UINT8_MAX;
	uint8_t * mem_block = NULL;
	uint16_t mem_length = 0;
	struct ap_startup_encryption_cb_receive cb = { 0 };
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&type,
			&mem_block, &mem_length)) {
		return FALSE;
	}
	cb.type = type;
	cb.data = mem_block;
	cb.length = mem_length;
	cb.user_data = user_data;
	return ap_module_enum_callback(&mod->instance.context, 
		AP_STARTUP_ENCRYPTION_CB_RECEIVE, &cb);
}

void ap_startup_encryption_make_packet(
	struct ap_startup_encryption_module * mod,
	enum ap_startup_encryption_packet_type type,
	const void * data,
	uint16_t data_length)
{
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	if (data && data_length) {
		au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
			AP_STARTUP_ENCRYPTION_PACKET_TYPE, 
			&type, 
			data, &data_length);
	}
	else {
		au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
			AP_STARTUP_ENCRYPTION_PACKET_TYPE, 
			&type, 
			NULL);
	}
	ap_packet_set_length(mod->ap_packet, length);
}
