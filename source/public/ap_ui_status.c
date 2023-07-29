#include "public/ap_ui_status.h"

#include "core/log.h"
#include "core/malloc.h"

#include "public/ap_character.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"

#include <assert.h>

struct ap_ui_status_module {
	struct ap_module_instance instance;
	struct ap_base_module * ap_base;
	struct ap_character_module * ap_character;
	struct ap_packet_module * ap_packet;
	size_t character_offset;
	struct au_packet packet;
};

static boolean onregister(
	struct ap_ui_status_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_base, AP_BASE_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	mod->character_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct ap_ui_status_character), 
		mod, NULL, NULL);
	if (mod->character_offset == SIZE_MAX) {
		ERROR("Failed to attach character data.");
		return FALSE;
	}
	return TRUE;
}

struct ap_ui_status_module * ap_ui_status_create_module()
{
	struct ap_ui_status_module * mod = ap_module_instance_new(
		AP_UI_STATUS_MODULE_NAME, sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_INT8, 1, /* Packet Type */
		AU_PACKET_TYPE_INT32, 1, /* Character Id */
		/* Quick Belt Grid Base */
		AU_PACKET_TYPE_PACKET, AP_UI_STATUS_MAX_QUICKBELT_ITEM_COUNT,
		AU_PACKET_TYPE_UINT8, 1, /* QBelt Index */
		AU_PACKET_TYPE_PACKET, 1, /* Updated Item Packet */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* Encoded QBelt Information (Type String) */
		AU_PACKET_TYPE_INT32, 1, /* HP Potion TID */
		AU_PACKET_TYPE_INT32, 1, /* MP Potion TID */
		AU_PACKET_TYPE_INT8, 1, /* View Helmet Option */
		AU_PACKET_TYPE_INT8, 1, /* Auto Use Hp Potion */
		AU_PACKET_TYPE_INT8, 1, /* Auto Use Mp Potion */
		/* Encoded Cooldown (Type String) */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1,
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_ui_status_add_callback(
	struct ap_ui_status_module * mod,
	enum ap_ui_status_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t ap_ui_status_attach_data(
	struct ap_ui_status_module * mod,
	enum ap_ui_status_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

struct ap_ui_status_character * ap_ui_status_get_character(
	struct ap_ui_status_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, 
		mod->character_offset);
}

boolean ap_ui_status_on_receive(
	struct ap_ui_status_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_ui_status_cb_receive cb = { 0 };
	const void * updatepacket = NULL;
	struct ap_base base = { 0 };
	cb.quickbelt_index = UINT8_MAX;
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type, /* Packet Type */ 
			&cb.character_id,
			NULL, /* Quick Belt Grid Base */
			&cb.quickbelt_index, /* QBelt Index */
			&updatepacket, /* Updated Item Packet */
			NULL, NULL, /* Encoded QBelt Information (Type String) */
			&cb.hp_potion_tid, /* HP Potion TID */
			&cb.mp_potion_tid, /* MP Potion TID */
			&cb.option_view_helmet, /* View Helmet Option */
			&cb.auto_use_hp_gauge, /* Auto Use Hp Potion */
			&cb.auto_use_mp_gauge, /* Auto Use Mp Potion */
			NULL, NULL)) { /* Encoded Cooldown (Type String) */
		return FALSE;
	}
	if (updatepacket) {
		if (!ap_base_parse_packet(mod->ap_base, updatepacket, 
				&base.type, &base.id)) {
			return FALSE;
		}
		cb.quickbelt_update = &base;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_UI_STATUS_CB_RECEIVE, &cb);
}

void ap_ui_status_make_add_packet(
	struct ap_ui_status_module * mod,
	struct ap_character * character)
{
	uint32_t type = AP_UI_STATUS_PACKET_TYPE_ADD;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	void * qbeltpackets[AP_UI_STATUS_MAX_QUICKBELT_ITEM_COUNT] = { 0 };
	struct ap_ui_status_character * uc = 
		ap_ui_status_get_character(mod, character);
	uint32_t i;
	for (i = 0; i < AP_UI_STATUS_MAX_QUICKBELT_ITEM_COUNT; i++) {
		qbeltpackets[i] = ap_packet_get_temp_buffer(mod->ap_packet);
		ap_base_make_packet(mod->ap_base, qbeltpackets[i],
			uc->items[i].base.type,
			uc->items[i].base.id);
	}
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length, AP_UISTATUS_PACKET_TYPE,
		&type,
		&character->id,
		&qbeltpackets[0], /* Quick Belt Grid Base */
		NULL, /* QBelt Index */
		NULL, /* Updated Item Packet */
		NULL, /* Encoded QBelt Information (Type String) */
		&uc->hp_potion_tid, /* HP Potion TID */
		&uc->mp_potion_tid, /* MP Potion TID */
		&uc->option_view_helmet, /* View Helmet Option */
		&uc->auto_use_hp_gauge, /* Auto Use Hp Potion */
		&uc->auto_use_mp_gauge, /* Auto Use Mp Potion */
		NULL); /* Encoded Cooldown (Type String) */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}
