#include "public/ap_bill_info.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"
#include "utility/au_table.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

struct ap_bill_info_module {
	struct ap_module_instance instance;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
	struct au_packet packet_cash_info;
};

static boolean onregister(
	struct ap_bill_info_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	return TRUE;
}

struct ap_bill_info_module * ap_bill_info_create_module()
{
	struct ap_bill_info_module * mod = ap_module_instance_new(AP_BILL_INFO_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /* operation */
		AU_PACKET_TYPE_INT32, 1, /* character id */
		AU_PACKET_TYPE_INT8, 1, /* payment type */
		AU_PACKET_TYPE_UINT32, 1, /* remain coupon play time */
		AU_PACKET_TYPE_INT8, 1, /* is pc-room */
		AU_PACKET_TYPE_UINT32, 1, /* PCRoomType */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_cash_info, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /* operation */
		AU_PACKET_TYPE_INT8, 1, /* reserved */
		AU_PACKET_TYPE_INT32, 1, /* character id */
		AU_PACKET_TYPE_INT32, 1, /* reserved */
		AU_PACKET_TYPE_UINT64, 1, /* wcoin */
		AU_PACKET_TYPE_UINT64, 1, /* pcoin */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_bill_info_make_cash_info_packet(
	struct ap_bill_info_module * mod,
	uint32_t character_id,
	uint64_t wcoins,
	uint64_t pcoins)
{
	uint8_t type = AP_BILL_INFO_PACKET_CASHINFO;
	uint8_t reserved0 = 0;
	uint32_t reserved1 = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet_cash_info, buffer, TRUE, &length,
		AP_BILL_INFO_PACKET_TYPE,
		&type, /* operation */
		&reserved0, /* reserved */
		&character_id, /* character id */
		&reserved1, /* reserved */
		&wcoins, /* wcoin */
		&pcoins); /* pcoin */
	ap_packet_set_length(mod->ap_packet, length);
}
