#include "public/ap_party_item.h"

#include "core/log.h"

#include "public/ap_admin.h"
#include "public/ap_item.h"
#include "public/ap_item_convert.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"

#include <assert.h>

struct ap_party_item_module {
	struct ap_module_instance instance;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
};

static boolean onregister(
	struct ap_party_item_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	return TRUE;
}

struct ap_party_item_module * ap_party_item_create_module()
{
	struct ap_party_item_module * mod = ap_module_instance_new(AP_PARTY_ITEM_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* Item Template ID */
		AU_PACKET_TYPE_INT32, 1, /* Item Owner CID */
		AU_PACKET_TYPE_INT32, 1, /* Item Count */
		AU_PACKET_TYPE_INT8, 1, /* # of physical convert */
		AU_PACKET_TYPE_INT8, 1, /* # of socket */
		AU_PACKET_TYPE_INT8, 1, /* # of converted socket */
		AU_PACKET_TYPE_INT8, 1, /* option Count */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_party_item_make_packet(
	struct ap_party_item_module * mod, 
	struct ap_item * item)
{
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	struct ap_item_convert_item * convert = 
		ap_item_convert_get_item(mod->ap_item_convert, item);
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_PARTY_ITEM_PACKET_TYPE,
		&item->tid, /* Item Template ID */
		&item->character_id, /* Item Owner CID */
		&item->stack_count, /* Item Count */
		&convert->physical_convert_level, /* # of physical convert */
		&convert->socket_count, /* # of socket */
		&convert->convert_count, /* # of converted socket */
		&item->option_count); /* option Count */
	ap_packet_set_length(mod->ap_packet, length);
}
