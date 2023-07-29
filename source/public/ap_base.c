#include "public/ap_base.h"

#include <assert.h>

#include "core/log.h"

#include "public/ap_packet.h"

#include "utility/au_packet.h"

struct ap_base_module {
	struct ap_module_instance instance;
	struct au_packet packet;
};

struct ap_base_module * ap_base_create_module()
{
	struct ap_base_module * mod = ap_module_instance_new(AP_BASE_MODULE_NAME, sizeof(*mod),
		NULL, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_UINT8, 1, /* Base Type */
		AU_PACKET_TYPE_UINT32, 1, /* Base Id */
		AU_PACKET_TYPE_END);
	return mod;
}

boolean ap_base_parse_packet(
	struct ap_base_module * mod,
	const void * buffer, 
	enum ap_base_type * type,
	uint32_t * id)
{
	return au_packet_get_field(&mod->packet, FALSE, buffer, 0,
		type, id);
}

void ap_base_make_packet(
	struct ap_base_module * mod,
	void * buffer,
	enum ap_base_type type, 
	uint32_t id)
{
	au_packet_make_packet(&mod->packet, buffer, FALSE, NULL, 0,
		&type, &id);
}
