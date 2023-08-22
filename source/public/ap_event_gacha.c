#include <assert.h>
#include <stdlib.h>

#include "core/file_system.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_config.h"
#include "public/ap_event_gacha.h"
#include "public/ap_event_manager.h"
#include "public/ap_item.h"
#include "public/ap_object.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"
#include "utility/au_table.h"

#define STREAM_GACHA_START "GachaStart"
#define STREAM_GACHA_END "GachaEnd"
#define STREAM_GACHA_TYPE "Type"

#define MAXTYPEID 5

size_t AP_EVENT_GACHA_ITEM_TEMPLATE_ATTACHMENT_OFFSET = SIZE_MAX;

struct ap_event_gacha_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_item_module * ap_item;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
	struct ap_event_gacha_type type[MAXTYPEID];
};

static boolean event_ctor(struct ap_event_gacha_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	e->data = alloc(sizeof(struct ap_event_gacha_event));
	memset(e->data, 0, sizeof(struct ap_event_gacha_event));
	return TRUE;
}

static boolean event_dtor(struct ap_event_gacha_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	dealloc(e->data);
	e->data = NULL;
	return TRUE;
}

static boolean event_read(
	struct ap_event_gacha_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_gacha_event * e = ap_event_gacha_get_event(data);
	if (!ap_module_stream_read_next_value(stream))
		return TRUE;
	if (strcmp(ap_module_stream_get_value_name(stream), 
			STREAM_GACHA_START) != 0)
		return TRUE;
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = 
			ap_module_stream_get_value_name(stream);
		if (strcmp(value_name, STREAM_GACHA_TYPE) == 0) {
			int gachatype = 0;
			if (!ap_module_stream_get_i32(stream, &gachatype)) {
				ERROR("Failed to read event gacha type.");
				return FALSE;
			}
			if (!gachatype || gachatype > MAXTYPEID) {
				ERROR("Invalid gacha type (%d).", gachatype);
				return FALSE;
			}
			e->type = &mod->type[gachatype - 1];
		}
		else if (strcmp(value_name, STREAM_GACHA_END) == 0) {
			break;
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean event_write(
	struct ap_event_gacha_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_gacha_event * e = ap_event_gacha_get_event(data);
	if (!ap_module_stream_write_i32(stream, STREAM_GACHA_START, 0) ||
		!ap_module_stream_write_i32(stream, STREAM_GACHA_TYPE, e->type->id) ||
		!ap_module_stream_write_i32(stream, STREAM_GACHA_END, 0)) {
		ERROR("Failed to write event gacha stream.");
		return FALSE;
	}
	return TRUE;
}

static boolean cbreadimport(
	struct ap_event_gacha_module * mod,
	struct ap_item_cb_read_import * cb)
{
	struct ap_item_template * temp = cb->temp;
	struct ap_event_gacha_item_template_attachment * attachment = 
		ap_event_gacha_get_item_template_attachment(temp);
	const char * value = cb->value;
	switch (cb->column_id) {
	case AP_ITEM_DCID_GACHAMINLV:
		attachment->gacha_level_min = strtol(value, NULL, 10);
		break;
	case AP_ITEM_DCID_GACHAMAXLV:
		attachment->gacha_level_max = strtol(value, NULL, 10);
		break;
	default:
		return TRUE;
	}
	return FALSE;
}

static boolean onregister(
	struct ap_event_gacha_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	if (!ap_event_manager_register_event(mod->ap_event_manager,
			AP_EVENT_MANAGER_FUNCTION_GACHA, 
			mod, event_ctor, event_dtor, event_read, event_write, NULL)) {
		ERROR("Failed to register event.");
		return FALSE;
	}
	AP_EVENT_GACHA_ITEM_TEMPLATE_ATTACHMENT_OFFSET = ap_item_attach_data(
		mod->ap_item, AP_ITEM_MDI_TEMPLATE, 
		sizeof(struct ap_event_gacha_item_template_attachment), 
		mod, NULL, NULL);
	ap_item_add_callback(mod->ap_item, AP_ITEM_CB_READ_IMPORT, mod, cbreadimport);
	return TRUE;
}

struct ap_event_gacha_module * ap_event_gacha_create_module()
{
	struct ap_event_gacha_module * mod = ap_module_instance_new(AP_EVENT_GACHA_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_INT8, 1, /* Packet Type */
		AU_PACKET_TYPE_PACKET, 1, /* Event Base Packet */
		AU_PACKET_TYPE_INT32, 1, /* CID */
		AU_PACKET_TYPE_INT32, 1, /* Result */
		AU_PACKET_TYPE_INT32, 1, /* Require Item TID */
		AU_PACKET_TYPE_INT32, 1, /* Require Stack Count */
		AU_PACKET_TYPE_INT32, 1, /* Require Gold */
		AU_PACKET_TYPE_INT32, 1, /* Require Charisma */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* List of Items */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_event_gacha_add_callback(
	struct ap_event_gacha_module * mod,
	enum ap_event_gacha_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

struct ap_event_gacha_event * ap_event_gacha_get_event(struct ap_event_manager_event * e)
{
	return e->data;
}

static const char * getvaluebyname(
	struct au_ini_mgr_ctx * ini, 
	uint32_t section_id,
	const char * key)
{
	return au_ini_mgr_get_value(ini, section_id, 
		au_ini_mgr_find_key(ini, section_id, key));
}

enum type_table_column_id {
	TT_CHARACTER_LV,
	TT_GACHA_ITEMTID,
	TT_GACHA_ITEM_NUMBER,
	TT_GACHA_COST,
	TT_CHARISMAPOINT,
	TT_RANK1,
	TT_RANK2,
	TT_RANK3,
	TT_RANK4,
};

static boolean readtypetable(
	struct ap_event_gacha_module * mod,
	struct ap_event_gacha_type * type, const char * file_path)
{
	char path[1024];
	struct au_table * table;
	boolean r = TRUE;
	if (!make_path(path, sizeof(path), "%s/%s", 
			ap_config_get(mod->ap_config, "ServerIniDir"), file_path)) {
		ERROR("Failed to create file path.");
		return FALSE;
	}
	table = au_table_open(path, FALSE);
	if (!table) {
		ERROR("Failed to open file (%s).", path);
		return FALSE;
	}
	r &= au_table_set_column(table, "Character_Lv", TT_CHARACTER_LV);
	r &= au_table_set_column(table, "Gacha_ItemTID", TT_GACHA_ITEMTID);
	r &= au_table_set_column(table, "Gacha_Item_Number", TT_GACHA_ITEM_NUMBER);
	r &= au_table_set_column(table, "Gacha_Cost", TT_GACHA_COST);
	r &= au_table_set_column(table, "CharismaPoint", TT_CHARISMAPOINT);
	r &= au_table_set_column(table, "Rank1", TT_RANK1);
	r &= au_table_set_column(table, "Rank2", TT_RANK2);
	r &= au_table_set_column(table, "Rank3", TT_RANK3);
	r &= au_table_set_column(table, "Rank4", TT_RANK4);
	if (!r) {
		ERROR("Failed to setup table columns.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_event_gacha_drop * drop = NULL;
		while (au_table_read_next_column(table)) {
			enum type_table_column_id id = au_table_get_column(table);
			const char * value = au_table_get_value(table);
			if (id == TT_CHARACTER_LV) {
				uint32_t level = strtoul(value, NULL, 10);
				assert(drop == NULL);
				if (!level || level > AP_CHARACTER_MAX_LEVEL) {
					ERROR("Invalid character level (%u).", level);
					au_table_destroy(table);
					return FALSE;
				}
				drop = &type->drop_table[level - 1];
				continue;
			}
			assert(drop != NULL);
			if (!drop) {
				ERROR("Missing character level.");
				au_table_destroy(table);
				return FALSE;
			}
			switch (id) {
			case TT_GACHA_ITEMTID:
				drop->require_item_tid = strtoul(value, NULL, 10);
				break;
			case TT_GACHA_ITEM_NUMBER:
				drop->require_stack_count = strtoul(value, NULL, 10);
				break;
			case TT_GACHA_COST:
				drop->require_gold = strtoul(value, NULL, 10);
				break;
			case TT_CHARISMAPOINT:
				drop->require_charisma = strtoul(value, NULL, 10);
				break;
			case TT_RANK1:
				assert(AP_EVENT_GACHA_MAX_RANK > 0);
				drop->rate[0] = strtoul(value, NULL, 10);
				break;
			case TT_RANK2:
				assert(AP_EVENT_GACHA_MAX_RANK > 1);
				drop->rate[1] = strtoul(value, NULL, 10);
				break;
			case TT_RANK3:
				assert(AP_EVENT_GACHA_MAX_RANK > 2);
				drop->rate[2] = strtoul(value, NULL, 10);
				break;
			case TT_RANK4:
				assert(AP_EVENT_GACHA_MAX_RANK > 3);
				drop->rate[3] = strtoul(value, NULL, 10);
				break;
			}
		}
	}
	au_table_destroy(table);
	return TRUE;
}

boolean ap_event_gacha_read_types(
	struct ap_event_gacha_module * mod,
	const char * file_path)
{
	struct au_ini_mgr_ctx * ini = au_ini_mgr_create();
	uint32_t sectioncount;
	uint32_t i;
	au_ini_mgr_set_path(ini, file_path);
	if (!au_ini_mgr_read_file(ini, 0, FALSE)) {
		ERROR("Failed to read file (%s).", file_path);
		return FALSE;
	}
	sectioncount = au_ini_mgr_get_section_count(ini);
	for (i = 0; i < sectioncount; i++) {
		uint32_t id = strtoul(au_ini_mgr_get_section_name(ini, i), NULL, 10);
		struct ap_event_gacha_type * type;
		uint32_t keycount;
		uint32_t j;
		if (!id || id > MAXTYPEID) {
			ERROR("Invalid gacha type (%u).", id);
			au_ini_mgr_destroy(ini);
			return FALSE;
		}
		type = &mod->type[id - 1];
		type->id = id;
		keycount = au_ini_mgr_get_key_count(ini, i);
		for (j = 0; j < keycount; j++) {
			const char * key = au_ini_mgr_get_key_name(ini, i, j);
			const char * value = au_ini_mgr_get_value(ini, i, j);
			if (strcmp(key, "Name") == 0) {
				strlcpy(type->name, value, sizeof(type->name));
			}
			else if (strcmp(key, "Comment") == 0) {
				strlcpy(type->comment, value, sizeof(type->comment));
			}
			else if (strcmp(key, "RaceCheck") == 0) {
				type->check_race = strtol(value, NULL, 10) != 0;
			}
			else if (strcmp(key, "ClassCheck") == 0) {
				type->check_class = strtol(value, NULL, 10) != 0;
			}
			else if (strcmp(key, "LevelCheck") == 0) {
				type->check_level = strtol(value, NULL, 10) != 0;
			}
			else if (strcmp(key, "LevelLimit") == 0) {
				type->level_limit = strtoul(value, NULL, 10);
			}
			else if (strcmp(key, "DropTable") == 0) {
				if (!readtypetable(mod, type, value)) {
					ERROR("Failed to read type table (%s).", value);
					au_ini_mgr_destroy(ini);
					return FALSE;
				}
			}
			else if (strcmp(key, "EnumEnd") == 0) {
			}
			else {
				assert(0);
			}
		}
	}
	au_ini_mgr_destroy(ini);
	return TRUE;
}

boolean ap_event_gacha_on_receive(
	struct ap_event_gacha_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_event_gacha_cb_receive cb = { 0 };
	const void * basepacket = NULL;
	struct ap_event_manager_base_packet event = { 0 };
	const char * targetpointname = NULL;
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type, /* Packet Type */
			&basepacket, /* Event Base Packet */
			NULL, /* CID */
			NULL, /* Result */
			NULL, /* Require Item TID */
			NULL, /* Require Stack Count */
			NULL, /* Require Gold */
			NULL, /* Require Charisma */
			NULL, NULL)) { /* List of Items */
		return FALSE;
	}
	if (basepacket) {
		if (!ap_event_manager_get_base_packet(mod->ap_event_manager, basepacket, &event))
			return FALSE;
		cb.event = &event;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_EVENT_GACHA_CB_RECEIVE, &cb);
}

void ap_event_gacha_make_grant_packet(
	struct ap_event_gacha_module * mod,
	struct ap_event_manager_event * event,
	uint32_t character_id,
	uint32_t character_level,
	const uint32_t * item_list,
	uint32_t item_count)
{
	uint8_t type = AP_EVENT_GACHA_PACKET_REQUESTGRANTED;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * base = NULL;
	uint16_t length = 0;
	struct ap_event_gacha_event * e = ap_event_gacha_get_event(event);
	struct ap_event_gacha_drop * drop = &e->type->drop_table[character_level - 1];
	uint16_t listsize = (uint16_t)(item_count * sizeof(*item_list));
	if (event) {
		base = ap_packet_get_temp_buffer(mod->ap_packet);
		ap_event_manager_make_base_packet(mod->ap_event_manager, event, base);
	}
	au_packet_make_packet(&mod->packet, buffer, 
		TRUE, &length, AP_EVENT_GACHA_PACKET_TYPE,
		&type, /* Packet Type */
		base, /* Event Base Packet */
		&character_id, /* CID */
		NULL, /* Result */
		&drop->require_item_tid, /* Require Item TID */
		&drop->require_stack_count, /* Require Stack Count */
		&drop->require_gold, /* Require Gold */
		&drop->require_charisma, /* Require Charisma */
		item_list, &listsize); /* List of Items */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_event_gacha_make_result_packet(
	struct ap_event_gacha_module * mod,
	struct ap_event_manager_event * event,
	enum ap_event_gacha_result result,
	uint32_t character_id,
	uint32_t * result_item_tid)
{
	uint8_t type = AP_EVENT_GACHA_PACKET_RESULT;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * base = NULL;
	uint16_t length = 0;
	uint32_t zero = 0;
	if (event) {
		base = ap_packet_get_temp_buffer(mod->ap_packet);
		ap_event_manager_make_base_packet(mod->ap_event_manager, event, base);
	}
	au_packet_make_packet(&mod->packet, buffer, 
		TRUE, &length, AP_EVENT_GACHA_PACKET_TYPE,
		&type, /* Packet Type */
		base, /* Event Base Packet */
		&character_id, /* CID */
		result_item_tid, /* Result */
		&result, /* Require Item TID */
		&zero, /* Require Stack Count */
		&zero, /* Require Gold */
		&zero, /* Require Charisma */
		NULL); /* List of Items */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}
