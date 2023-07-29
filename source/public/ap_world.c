#include "public/ap_world.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_define.h"
#include "public/ap_module.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"

struct ap_world_module {
	struct ap_module_instance instance;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
};

static boolean onregister(
	struct ap_world_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, 
		AP_PACKET_MODULE_NAME);
	return TRUE;
}

struct ap_world_module * ap_world_create_module()
{
	struct ap_world_module * mod = ap_module_instance_new(AP_WORLD_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_UINT8, 1, /* Packet Type */ 
		AU_PACKET_TYPE_CHAR, AP_WORLD_MAX_WORLD_NAME, /* World */
		AU_PACKET_TYPE_UINT16, 1, /* World Status */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* Encoded World List */
		/* ServerRegion */
		AU_PACKET_TYPE_CHAR, AP_WORLD_MAX_SERVER_REGION,
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_world_add_callback(
	struct ap_world_module * mod,
	enum ap_world_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

boolean ap_world_on_receive(
	struct ap_world_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_world_cb_receive cb = { 0 };
	const char * wname = NULL;
	const char * encoded = NULL;
	const char * region;
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type,
			&wname,
			&cb.world_status,
			&encoded, NULL,
			&region)) {
		return FALSE;
	}
	AU_PACKET_GET_STRING(cb.world_name, wname);
	AU_PACKET_GET_STRING(cb.server_region, region);
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_WORLD_CB_RECEIVE, &cb);
}

void ap_world_make_result_get_world_all_packet(
	struct ap_world_module * mod,
	const char * group_name,
	const char * world_name,
	enum ap_world_status status,
	uint32_t flags)
{
	uint8_t type = AP_WORLD_PACKET_RESULT_GET_WORLD_ALL;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	char encoded[AP_WORLD_MAX_ENCODED_LENGTH];
	uint16_t encoded_len;
	snprintf(encoded, sizeof(encoded), "%s=%s=%u=%u=%u;",
		world_name,
		group_name,
		0, /* Priority */
		status,
		flags);
	encoded_len = (uint16_t)strlen(encoded) + 1;
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_WORLD_PACKET_TYPE, 
		&type, /* Packet Type */
		NULL, /* World Name */
		NULL, /* World Status */
		encoded, &encoded_len,
		NULL); /* Server Region */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_world_make_result_char_count_packet(
	struct ap_world_module * mod,
	const char * world_name,
	uint32_t count)
{
	uint8_t type = AP_WORLD_PACKET_RESULT_CHAR_COUNT;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	char encoded[AP_WORLD_MAX_ENCODED_LENGTH];
	uint16_t encoded_len;
	snprintf(encoded, sizeof(encoded), "%s=%u;",
		world_name,
		count);
	encoded_len = (uint16_t)strlen(encoded) + 1;
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_WORLD_PACKET_TYPE, 
		&type, /* Packet Type */
		NULL, /* World Name */
		NULL, /* World Status */
		encoded, &encoded_len,
		NULL); /* Server Region */
	ap_packet_set_length(mod->ap_packet, length);
}
