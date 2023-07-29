#include <assert.h>

#include "core/log.h"
#include "core/malloc.h"

#include "server/as_character.h"
#include "server/as_database.h"
#include "server/as_ui_status.h"

#define DB_STREAM_MODULE_ID 4
#define MAKE_ID(ID) AS_DATABASE_MAKE_ID(DB_STREAM_MODULE_ID, ID)

struct as_ui_status_module {
	struct ap_module_instance instance;
	struct ap_ui_status_module * ap_ui_status;
	struct as_character_module * as_character;
	size_t character_db_offset;
};

static boolean cbdecodechar(
	struct as_ui_status_module * mod,
	struct as_character_cb_decode * cb)
{
	boolean result = TRUE;
	struct as_ui_status_character_db * db = 
		as_ui_status_get_character_db(mod, cb->character);
	if (cb->module_id != DB_STREAM_MODULE_ID)
		return TRUE;
	switch (cb->field_id) {
	case AS_UI_STATUS_CHARACTER_DB_UI_STATUS_ITEMS: {
		uint32_t i;
		for (i = 0; i < COUNT_OF(db->items); i++) {
			struct ap_ui_status_item * item = &db->items[i];
			if (!AS_DATABASE_DECODE(cb->codec, item->base.type) ||
				!AS_DATABASE_DECODE(cb->codec, item->base.id) ||
				!AS_DATABASE_DECODE(cb->codec, item->tid)) {
				ERROR("Failed to decode quickbelt item (%s).",
					cb->character->name);
				return TRUE;
			}
		}
		break;
	}
	case AS_UI_STATUS_CHARACTER_DB_UI_STATUS_HP_POTION_TID:
			if (!AS_DATABASE_DECODE(cb->codec, db->hp_potion_tid)) {
				ERROR("Failed to decode auto use hp potion tid (%s).",
					cb->character->name);
				return TRUE;
			}
		break;
	case AS_UI_STATUS_CHARACTER_DB_UI_STATUS_MP_POTION_TID:
			if (!AS_DATABASE_DECODE(cb->codec, db->mp_potion_tid)) {
				ERROR("Failed to decode auto use mp potion tid (%s).",
					cb->character->name);
				return TRUE;
			}
		break;
	case AS_UI_STATUS_CHARACTER_DB_UI_STATUS_OPTION_VIEW_HELMET:
			if (!AS_DATABASE_DECODE(cb->codec, 
					db->option_view_helmet)) {
				ERROR("Failed to decode option view helmet (%s).",
					cb->character->name);
				return TRUE;
			}
		break;
	case AS_UI_STATUS_CHARACTER_DB_UI_STATUS_AUTO_USE_HP_GAUGE:
			if (!AS_DATABASE_DECODE(cb->codec, 
					db->auto_use_hp_gauge)) {
				ERROR("Failed to decode auto use hp gauge (%s).",
					cb->character->name);
				return TRUE;
			}
		break;
	case AS_UI_STATUS_CHARACTER_DB_UI_STATUS_AUTO_USE_MP_GAUGE:
			if (!AS_DATABASE_DECODE(cb->codec, 
					db->auto_use_mp_gauge)) {
				ERROR("Failed to decode auto use mp gauge (%s).",
					cb->character->name);
				return TRUE;
			}
		break;
	default:
		assert(0);
		return TRUE;
	}
	return FALSE;
}

static boolean cbencodechar(
	struct as_ui_status_module * mod,
	struct as_character_cb_decode * cb)
{
	boolean result = TRUE;
	uint32_t i;
	struct as_ui_status_character_db * db = 
		as_ui_status_get_character_db(mod, cb->character);
	result &= as_database_encode(cb->codec, 
		MAKE_ID(AS_UI_STATUS_CHARACTER_DB_UI_STATUS_ITEMS),
		NULL, 0);
	for (i = 0; i < COUNT_OF(db->items); i++) {
		const struct ap_ui_status_item * item = &db->items[i];
		result &= as_database_write_encoded(cb->codec,
			&item->base.type, sizeof(item->base.type));
		result &= as_database_write_encoded(cb->codec,
			&item->base.id, sizeof(item->base.id));
		result &= as_database_write_encoded(cb->codec,
			&item->tid, sizeof(item->tid));
	}
	result &= AS_DATABASE_ENCODE(cb->codec, 
		MAKE_ID(AS_UI_STATUS_CHARACTER_DB_UI_STATUS_HP_POTION_TID),
		db->hp_potion_tid);
	result &= AS_DATABASE_ENCODE(cb->codec, 
		MAKE_ID(AS_UI_STATUS_CHARACTER_DB_UI_STATUS_MP_POTION_TID),
		db->mp_potion_tid);
	result &= AS_DATABASE_ENCODE(cb->codec, 
		MAKE_ID(AS_UI_STATUS_CHARACTER_DB_UI_STATUS_OPTION_VIEW_HELMET),
		db->option_view_helmet);
	result &= AS_DATABASE_ENCODE(cb->codec, 
		MAKE_ID(AS_UI_STATUS_CHARACTER_DB_UI_STATUS_AUTO_USE_HP_GAUGE),
		db->auto_use_hp_gauge);
	result &= AS_DATABASE_ENCODE(cb->codec, 
		MAKE_ID(AS_UI_STATUS_CHARACTER_DB_UI_STATUS_AUTO_USE_MP_GAUGE),
		db->auto_use_mp_gauge);
	return result;
}

static boolean cbloadchar(
	struct as_ui_status_module * mod,
	struct as_character_cb_load * cb)
{
	struct ap_character * c = cb->character;
	struct ap_ui_status_character * uc = 
		ap_ui_status_get_character(mod->ap_ui_status, c);
	struct as_ui_status_character_db * db = 
		as_ui_status_get_character_db(mod, cb->db);
	assert(sizeof(db->items) == sizeof(uc->items));
	memcpy(uc->items, db->items, sizeof(uc->items));
	uc->hp_potion_tid = db->hp_potion_tid;
	uc->mp_potion_tid = db->mp_potion_tid;
	uc->option_view_helmet = db->option_view_helmet;
	uc->auto_use_hp_gauge = db->auto_use_hp_gauge;
	uc->auto_use_mp_gauge = db->auto_use_mp_gauge;
	return TRUE;
}

static boolean cbreflectchar(
	struct as_ui_status_module * mod,
	struct as_character_cb_reflect * cb)
{
	struct ap_ui_status_character * uc = 
		ap_ui_status_get_character(mod->ap_ui_status, cb->character);
	struct as_ui_status_character_db * db = 
		as_ui_status_get_character_db(mod, cb->db);
	assert(sizeof(db->items) == sizeof(uc->items));
	memcpy(db->items, uc->items, sizeof(uc->items));
	db->hp_potion_tid = uc->hp_potion_tid;
	db->mp_potion_tid = uc->mp_potion_tid;
	db->option_view_helmet = uc->option_view_helmet;
	db->auto_use_hp_gauge = uc->auto_use_hp_gauge;
	db->auto_use_mp_gauge = uc->auto_use_mp_gauge;
	return TRUE;
}

static boolean onregister(
	struct as_ui_status_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_ui_status, AP_UI_STATUS_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	mod->character_db_offset = as_character_attach_data(mod->as_character,
		AS_CHARACTER_MDI_DATABASE, sizeof(struct as_ui_status_character_db), 
		mod, NULL, NULL);
	if (mod->character_db_offset == SIZE_MAX) {
		ERROR("Failed to attach character database data.");
		return FALSE;
	}
	if (!as_character_set_database_stream(mod->as_character, DB_STREAM_MODULE_ID,
			mod, cbdecodechar, cbencodechar)) {
		ERROR("Failed to set character database stream.");
		return FALSE;
	}
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_LOAD, 
		mod, cbloadchar);
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_REFLECT, 
		mod, cbreflectchar);
	return TRUE;
}

struct as_ui_status_module * as_ui_status_create_module()
{
	struct as_ui_status_module * mod = ap_module_instance_new(AS_UI_STATUS_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	return mod;
}

struct as_ui_status_character_db * as_ui_status_get_character_db(
	struct as_ui_status_module * mod,
	struct as_character_db * character)
{
	return ap_module_get_attached_data(character, 
		mod->character_db_offset);
}
