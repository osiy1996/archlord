#include "public/ap_private_trade.h"

#include "core/log.h"

#include "public/ap_character.h"
#include "public/ap_item.h"
#include "public/ap_item_convert.h"
#include "public/ap_packet.h"
#include "public/ap_tick.h"

#include "utility/au_packet.h"
#include "utility/au_table.h"

#include <assert.h>
#include <stdlib.h>

size_t g_ApPrivateTradeCharacterAttachmentOffset = SIZE_MAX;

struct ap_private_trade_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_packet_module * ap_packet;
	struct ap_tick_module * ap_tick;
	struct au_packet packet;
};

static boolean onregister(
	struct ap_private_trade_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	g_ApPrivateTradeCharacterAttachmentOffset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct ap_private_trade_character_attachment),
		mod, NULL, NULL);
	return TRUE;
}

static void onclose(struct ap_private_trade_module * mod)
{
}

static void onshutdown(struct ap_private_trade_module * mod)
{
}

struct ap_private_trade_module * ap_private_trade_create_module()
{
	struct ap_private_trade_module * mod = ap_module_instance_new(AP_PRIVATE_TRADE_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_INT8, 1, /* Packet Type */
		AU_PACKET_TYPE_INT32, 1, /* lCID  */
		AU_PACKET_TYPE_INT32, 1, /* lCTargetID */
		AU_PACKET_TYPE_INT32, 1, /* lIID */
		AU_PACKET_TYPE_INT32, 1, /* lITID */
		AU_PACKET_TYPE_INT32, 1, /* Money Count */
		AU_PACKET_TYPE_PACKET, 1, /* csOriginPos */
		AU_PACKET_TYPE_PACKET, 1, /* csGridPos */
		AU_PACKET_TYPE_INT32, 1, /* arg */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_private_trade_add_callback(
	struct ap_private_trade_module * mod,
	enum ap_private_trade_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t ap_private_trade_attach_data(
	struct ap_private_trade_module * mod,
	enum ap_private_trade_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

void ap_private_trade_cancel(
	struct ap_private_trade_module * mod,
	struct ap_character * character,
	boolean recursive)
{
	struct ap_private_trade_cb_cancel cb = { 0 };
	struct ap_private_trade_character_attachment * attachment = 
		ap_private_trade_get_character_attachment(character);
	cb.character = character;
	cb.attachment = attachment;
	cb.recursive = recursive;
	ap_module_enum_callback(mod, AP_PRIVATE_TRADE_CB_CANCEL, &cb);
}

boolean ap_private_trade_on_receive(
	struct ap_private_trade_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_private_trade_cb_receive cb = { 0 };
	uint32_t currency = 0;
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
		&cb.type, /* Packet Type */
		&cb.character_id, /* lCID  */
		&cb.target_id, /* lCTargetID */
		&cb.item_id, /* lIID */
		NULL, /* lITID */
		&currency, /* Money Count */
		NULL, /* csOriginPos */
		NULL, /* csGridPos */
		&cb.arg)) { /* arg */
		return FALSE;
	}
	cb.currency_amount = currency;
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_PRIVATE_TRADE_CB_RECEIVE, &cb);
}

void ap_private_trade_make_packet(
	struct ap_private_trade_module * mod, 
	enum ap_private_trade_packet_type type,
	uint32_t * character_id,
	uint32_t * target_id,
	uint64_t * amount)
{
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	if (amount) {
		uint32_t currency = (uint32_t)*amount;
		au_packet_make_packet(&mod->packet, 
			buffer, TRUE, &length,
			AP_PRIVATE_TRADE_PACKET_TYPE, 
			&type, /* Packet Type */
			character_id, /* lCID  */
			target_id, /* lCTargetID */
			NULL, /* lIID */
			NULL, /* lITID */
			&currency, /* Money Count */
			NULL, /* csOriginPos */
			NULL, /* csGridPos */
			NULL); /* arg */
	}
	else {
		au_packet_make_packet(&mod->packet, 
			buffer, TRUE, &length,
			AP_PRIVATE_TRADE_PACKET_TYPE, 
			&type, /* Packet Type */
			character_id, /* lCID  */
			target_id, /* lCTargetID */
			NULL, /* lIID */
			NULL, /* lITID */
			NULL, /* Money Count */
			NULL, /* csOriginPos */
			NULL, /* csGridPos */
			NULL); /* arg */
	}
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_private_trade_make_result_packet(
	struct ap_private_trade_module * mod, 
	uint32_t character_id)
{
	uint8_t type = AP_PRIVATE_TRADE_PACKET_RESULT;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	uint32_t result = 8;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_PRIVATE_TRADE_PACKET_TYPE, 
		&type, /* Packet Type */
		&character_id, /* lCID  */
		NULL, /* lCTargetID */
		NULL, /* lIID */
		NULL, /* lITID */
		NULL, /* Money Count */
		NULL, /* csOriginPos */
		NULL, /* csGridPos */
		&result); /* arg */
	ap_packet_set_length(mod->ap_packet, length);
}
