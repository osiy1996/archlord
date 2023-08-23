#include "public/ap_drop_item.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_item_convert.h"

#include "utility/au_table.h"

#include "vendor/pcg/pcg_basic.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>

#define OPTION_POOL_MAX_COUNT 64

enum num_option_drop_rate_column_id {
	NUM_OPTION_COLUMN_OPTIONNUM,
	NUM_OPTION_COLUMN_DROPRATE1,
	NUM_OPTION_COLUMN_DROPRATE2,
	NUM_OPTION_COLUMN_DROPRATE3,
	NUM_OPTION_COLUMN_DROPRATE4,
	NUM_OPTION_COLUMN_DROPRATE5,
};

enum drop_rank_rate_column_id {
	DROP_RANK_RATE_COL_MONSTERRANK,
	DROP_RANK_RATE_COL_DROPRANK1,
	DROP_RANK_RATE_COL_DROPRANK2,
	DROP_RANK_RATE_COL_DROPRANK3,
	DROP_RANK_RATE_COL_DROPRANK4,
	DROP_RANK_RATE_COL_DROPRANK5,
	DROP_RANK_RATE_COL_DROPRANK6,
	DROP_RANK_RATE_COL_DROPRANK7,
	DROP_RANK_RATE_COL_DROPRANK8,
	DROP_RANK_RATE_COL_DROPRANK9,
	DROP_RANK_RATE_COL_DROPRANK10,
};

enum drop_group_rate_column_id {
	DROP_GROUP_COL_GROUPID,
	DROP_GROUP_COL_GROUPNAME,
	DROP_GROUP_COL_ISAFFECTEDBYDROPMED,
	DROP_GROUP_COL_DROPRATE,
	DROP_GROUP_COL_LEVELBONUS10,
	DROP_GROUP_COL_LEVELBONUS20,
	DROP_GROUP_COL_LEVELBONUS30,
	DROP_GROUP_COL_LEVELBONUS40,
	DROP_GROUP_COL_LEVELBONUS50,
	DROP_GROUP_COL_LEVELBONUS60,
	DROP_GROUP_COL_LEVELBONUS70,
	DROP_GROUP_COL_LEVELBONUS80,
	DROP_GROUP_COL_LEVELBONUS90,
	DROP_GROUP_COL_LEVELBONUS100,
};

struct option_pool {
	struct ap_item_option_template * options[OPTION_POOL_MAX_COUNT];
	uint32_t count;
	uint32_t total_probability;
};

struct ap_drop_item_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_item_module * ap_item;
	size_t character_attachment_offset;
	size_t item_attachment_offset;
	uint32_t option_num_drop_rate[AP_ITEM_OPTION_MAX_COUNT][AP_ITEM_OPTION_MAX_COUNT];
	uint32_t socket_num_drop_rate[AP_ITEM_CONVERT_MAX_SOCKET][AP_ITEM_CONVERT_MAX_SOCKET];
	struct option_pool option_drop_pool[AP_ITEM_OPTION_MAX_TYPE][AP_ITEM_PART_COUNT];
	struct option_pool option_refine_pool[AP_ITEM_OPTION_MAX_TYPE][AP_ITEM_PART_COUNT];
	struct option_pool option_gacha_pool[AP_ITEM_OPTION_MAX_TYPE][AP_ITEM_PART_COUNT];
	struct ap_drop_item_drop_rank_rate drop_rank_rate[AP_DROP_ITEM_MAX_DROP_RANK + 1];
	struct ap_drop_item_drop_group_rate drop_group_rate[AP_DROP_ITEM_MAX_DROP_GROUP_ID + 1];
	pcg32_random_t rng;
	uint32_t * type_list;
};

static boolean cbchartemplatector(
	struct ap_drop_item_module * mod,
	struct ap_character_template * temp)
{
	struct ap_drop_item_character_template_attachment * attachment =
		ap_drop_item_get_character_template_attachment(mod, temp);
	uint32_t i;
	for (i = 0; i < COUNT_OF(attachment->drop_groups); i++) {
		uint32_t j;
		for (j = 0; j < COUNT_OF(attachment->drop_groups[i].items); j++) {
			attachment->drop_groups[i].items[j] = 
				vec_new(sizeof(struct ap_drop_item));
		}
	}
	attachment->drop_items = vec_new(sizeof(*attachment->drop_items));
	return TRUE;
}

static boolean cbchartemplatedtor(
	struct ap_drop_item_module * mod,
	struct ap_character_template * temp)
{
	struct ap_drop_item_character_template_attachment * attachment =
		ap_drop_item_get_character_template_attachment(mod, temp);
	uint32_t i;
	for (i = 0; i < COUNT_OF(attachment->drop_groups); i++) {
		uint32_t j;
		for (j = 0; j < COUNT_OF(attachment->drop_groups[i].items); j++)
			vec_free(attachment->drop_groups[i].items[j]);
	}
	vec_free(attachment->drop_items);
	return TRUE;
}

static boolean cbcharreadimport(
	struct ap_drop_item_module * mod,
	struct ap_character_cb_read_import * cb)
{
	struct ap_character_template * temp = cb->temp;
	struct ap_drop_item_character_template_attachment * attachment = 
		ap_drop_item_get_character_template_attachment(mod, temp);
	const char * value = cb->value;
	switch (cb->id) {
	case AP_CHARACTER_IMPORT_DCID_DROPRANK:
		attachment->drop_rank = strtoul(cb->value, NULL, 10);
		break;
	case AP_CHARACTER_IMPORT_DCID_DROPMEDITATION:
		attachment->drop_mediation = strtof(cb->value, NULL);
		break;
	default:
		return TRUE;
	}
	return FALSE;
}

static boolean cbendreadoption(
	struct ap_drop_item_module * mod,
	struct ap_item_cb_end_read_option * cb)
{
	struct ap_item_option_template * temp = cb->temp;
	uint32_t typeidx;
	assert(temp->type <= AP_ITEM_OPTION_MAX_TYPE);
	assert(temp->rank_min <= AP_ITEM_OPTION_MAX_RANK);
	assert(temp->rank_max <= AP_ITEM_OPTION_MAX_RANK);
	assert(temp->rank_min <= temp->rank_max);
	if (!temp->type)
		return TRUE;
	typeidx = temp->type - 1;
	if (temp->set_part & AP_ITEM_OPTION_SET_TYPE_REFINERY) {
		uint32_t setpartmap[] = {
			AP_ITEM_PART_BODY, AP_ITEM_OPTION_SET_TYPE_BODY,
			AP_ITEM_PART_LEGS, AP_ITEM_OPTION_SET_TYPE_LEGS,
			AP_ITEM_PART_HAND_RIGHT, AP_ITEM_OPTION_SET_TYPE_WEAPON,
			AP_ITEM_PART_HAND_LEFT, AP_ITEM_OPTION_SET_TYPE_SHIELD,
			AP_ITEM_PART_HEAD, AP_ITEM_OPTION_SET_TYPE_HEAD,
			AP_ITEM_PART_ACCESSORY_RING1, AP_ITEM_OPTION_SET_TYPE_RING,
			AP_ITEM_PART_ACCESSORY_NECKLACE, AP_ITEM_OPTION_SET_TYPE_NECKLACE,
			AP_ITEM_PART_FOOT, AP_ITEM_OPTION_SET_TYPE_FOOTS,
			AP_ITEM_PART_HANDS, AP_ITEM_OPTION_SET_TYPE_HANDS };
		uint32_t i;
		for (i = 0; i < COUNT_OF(setpartmap) / 2; i++) {
			enum ap_item_part part = setpartmap[i * 2];
			uint32_t set = setpartmap[i * 2 + 1];
			if (temp->set_part & set) {
				struct option_pool * pool = &mod->option_refine_pool[typeidx][part];
				assert(pool->count < OPTION_POOL_MAX_COUNT);
				pool->options[pool->count++] = temp;
				pool->total_probability += temp->probability;
			}
		}
	}
	else if (temp->set_part & AP_ITEM_OPTION_SET_TYPE_GACHA) {
		uint32_t setpartmap[] = {
			AP_ITEM_PART_BODY, AP_ITEM_OPTION_SET_TYPE_BODY,
			AP_ITEM_PART_LEGS, AP_ITEM_OPTION_SET_TYPE_LEGS,
			AP_ITEM_PART_HAND_RIGHT, AP_ITEM_OPTION_SET_TYPE_WEAPON,
			AP_ITEM_PART_HAND_LEFT, AP_ITEM_OPTION_SET_TYPE_SHIELD,
			AP_ITEM_PART_HEAD, AP_ITEM_OPTION_SET_TYPE_HEAD,
			AP_ITEM_PART_ACCESSORY_RING1, AP_ITEM_OPTION_SET_TYPE_RING,
			AP_ITEM_PART_ACCESSORY_NECKLACE, AP_ITEM_OPTION_SET_TYPE_NECKLACE,
			AP_ITEM_PART_FOOT, AP_ITEM_OPTION_SET_TYPE_FOOTS,
			AP_ITEM_PART_HANDS, AP_ITEM_OPTION_SET_TYPE_HANDS };
		uint32_t i;
		for (i = 0; i < COUNT_OF(setpartmap) / 2; i++) {
			enum ap_item_part part = setpartmap[i * 2];
			uint32_t set = setpartmap[i * 2 + 1];
			if (temp->set_part & set) {
				struct option_pool * pool = &mod->option_gacha_pool[typeidx][part];
				assert(pool->count < OPTION_POOL_MAX_COUNT);
				pool->options[pool->count++] = temp;
				pool->total_probability += temp->probability;
			}
		}
	}
	else if (temp->probability) {
		uint32_t setpartmap[] = {
			AP_ITEM_PART_BODY, AP_ITEM_OPTION_SET_TYPE_BODY,
			AP_ITEM_PART_LEGS, AP_ITEM_OPTION_SET_TYPE_LEGS,
			AP_ITEM_PART_HAND_RIGHT, AP_ITEM_OPTION_SET_TYPE_WEAPON,
			AP_ITEM_PART_HAND_LEFT, AP_ITEM_OPTION_SET_TYPE_SHIELD,
			AP_ITEM_PART_HEAD, AP_ITEM_OPTION_SET_TYPE_HEAD,
			AP_ITEM_PART_ACCESSORY_RING1, AP_ITEM_OPTION_SET_TYPE_RING,
			AP_ITEM_PART_ACCESSORY_NECKLACE, AP_ITEM_OPTION_SET_TYPE_NECKLACE,
			AP_ITEM_PART_FOOT, AP_ITEM_OPTION_SET_TYPE_FOOTS,
			AP_ITEM_PART_HANDS, AP_ITEM_OPTION_SET_TYPE_HANDS };
		uint32_t i;
		for (i = 0; i < COUNT_OF(setpartmap) / 2; i++) {
			enum ap_item_part part = setpartmap[i * 2];
			uint32_t set = setpartmap[i * 2 + 1];
			if (temp->set_part & set) {
				struct option_pool * pool = &mod->option_drop_pool[typeidx][part];
				assert(pool->count < OPTION_POOL_MAX_COUNT);
				pool->options[pool->count++] = temp;
				pool->total_probability += temp->probability;
			}
		}
	}
	return TRUE;
}

static boolean cbreadimport(
	struct ap_drop_item_module * mod,
	struct ap_item_cb_read_import * cb)
{
	struct ap_item_template * temp = cb->temp;
	struct ap_drop_item_template_attachment * attachment = 
		ap_drop_item_get_item_template_attachment(mod, temp);
	const char * value = cb->value;
	switch (cb->column_id) {
	case AP_ITEM_DCID_SUITABLELEVELMIN:
		attachment->suitable_level_min = strtol(value, NULL, 10);
		break;
	case AP_ITEM_DCID_SUITABLELEVELMAX:
		attachment->suitable_level_max = strtol(value, NULL, 10);
		break;
	case AP_ITEM_DCID_GROUPID:
		attachment->group_id = strtol(value, NULL, 10);
		assert(attachment->group_id <= AP_DROP_ITEM_MAX_DROP_GROUP_ID);
		break;
	case AP_ITEM_DCID_DROPRATE:
		attachment->drop_rate = strtol(value, NULL, 10);
		break;
	case AP_ITEM_DCID_DROPRANK:
		attachment->drop_rank = strtol(value, NULL, 10);
		break;
	default:
		return TRUE;
	}
	return FALSE;
}

static boolean isoptionsuitable(
	const struct ap_item_template * item, 
	const struct ap_item_option_template * option)
{
	uint32_t item_level = item->factor_restrict.char_status.level;
	if (!option->probability)
		return FALSE;
	if (!option->rank_min || !option->rank_max)
		return FALSE;
	if (item->factor.item.rank < option->rank_min || 
		item->factor.item.rank > option->rank_max) {
		return FALSE;
	}
	if (!item_level)
		return TRUE;
	if (option->level_min && item_level < option->level_min)
		return FALSE;
	if (option->level_max && item_level > option->level_max)
		return FALSE;
	return TRUE;
}

static void buildoptionpool(
	struct ap_drop_item_module * mod,
	struct ap_item_template * temp,
	struct ap_drop_item_template_attachment * attachment)
{
	uint32_t type_count = 0;
	const uint32_t max_per_rank = 16;
	size_t size;
	uint32_t rank_limit;
	enum ap_item_part item_part = temp->equip.part;
	uint32_t item_level;
	uint32_t i;
	if (temp->type != AP_ITEM_TYPE_EQUIP)
		return;
	switch (item_part) {
	case AP_ITEM_PART_BODY:
	case AP_ITEM_PART_HEAD:
	case AP_ITEM_PART_HANDS:
	case AP_ITEM_PART_LEGS:
	case AP_ITEM_PART_FOOT:
	case AP_ITEM_PART_HAND_RIGHT:
	case AP_ITEM_PART_ACCESSORY_RING1:
	case AP_ITEM_PART_ACCESSORY_NECKLACE:
		break;
	case AP_ITEM_PART_HAND_LEFT:
		/* Bow/Dagger are left-hand items, 
		 * but require weapon item options. */
		if (temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON) 
			item_part = AP_ITEM_PART_HAND_RIGHT;
		break;
	default:
		return;
	}
	item_level = temp->factor_restrict.char_status.level;
	rank_limit = MAX(1, MIN(temp->factor.item.rank, AP_ITEM_OPTION_MAX_RANK));
	for (i = 0; i <  AP_ITEM_OPTION_MAX_TYPE; i++) {
		uint32_t option_count = 0;
		uint32_t j;
		struct option_pool * pool = &mod->option_drop_pool[i][item_part];
		if (!pool->total_probability)
			continue;
		for (j = 0; j < pool->count; j++) {
			struct ap_item_option_template * option = pool->options[j];
			if (!isoptionsuitable(temp, option))
				continue;
			if (++option_count == 1)
				type_count++;
		}
	}
	if (!type_count)
		return;
	size = type_count * sizeof(struct ap_drop_item_option_type_pool);
	attachment->type_pools = alloc(size);
	memset(attachment->type_pools, 0, size);
	attachment->type_pool_count = type_count;
	type_count = 0;
	for (i = 0; i < AP_ITEM_OPTION_MAX_TYPE; i++) {
		uint32_t option_count = 0;
		struct option_pool * pool = &mod->option_drop_pool[i][item_part];
		uint32_t j;
		if (!pool->total_probability)
			continue;
		for (j = 0; j < pool->count; j++) {
			struct ap_item_option_template * option = pool->options[j];
			struct ap_drop_item_option_type_pool * typepool;
			if (!isoptionsuitable(temp, option))
				continue;
			typepool = &attachment->type_pools[type_count];
			if (typepool->option_count == AP_DROP_ITEM_MAX_OPTION_COUNT_IN_POOL) {
				assert(0);
				continue;
			}
			typepool->total_probability += option->probability;
			typepool->temp[typepool->option_count++] = option;
			option_count++;
		}
		if (option_count) {
			assert(type_count < attachment->type_pool_count);
			attachment->type_pools[type_count].option_type = j;
			type_count++;
		}
	}
	assert(type_count == attachment->type_pool_count);
}

static void builddropgroups(
	struct ap_drop_item_module * mod,
	struct ap_item_template * temp,
	struct ap_drop_item_template_attachment * attachment)
{
	uint32_t i;
	if (temp->type == AP_ITEM_TYPE_EQUIP &&
		(temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON || 
			temp->equip.kind == AP_ITEM_EQUIP_KIND_NECKLACE) && 
		temp->gacha_type == 3 && 
		temp->factor.item.gacha == 3) {
		attachment->is_droppable_unique_accessory = TRUE;
	}
	else {
		attachment->is_droppable_unique_accessory = FALSE;
	}
	if (!attachment->drop_rank || !attachment->drop_rate || temp->is_event_item)
		return;
	assert(attachment->drop_rank < AP_DROP_ITEM_MAX_DROP_RANK);
	assert(attachment->group_id <= AP_DROP_ITEM_MAX_DROP_GROUP_ID);
	for (i = 1; i < AP_CHARACTER_MAX_TEMPLATE_ID; i++) {
		struct ap_character_template * chartemp = 
			ap_character_get_template(mod->ap_character, i);
		struct ap_drop_item_character_template_attachment * charattachment;
		struct ap_drop_item_drop_group * group;
		struct ap_drop_item item = { 0 };
		uint32_t rank;
		if (!chartemp)
			continue;
		charattachment = ap_drop_item_get_character_template_attachment(mod, chartemp);
		if (!charattachment->drop_rank)
			continue;
		rank = attachment->drop_rank;
		if (ap_drop_item_get_drop_rank_rate(mod, charattachment->drop_rank, rank) != 0.0f)
			continue;
		if (attachment->suitable_level_max != 1 &&
			(chartemp->factor.char_status.level < attachment->suitable_level_min ||
			chartemp->factor.char_status.level > attachment->suitable_level_max)) {
			continue;
		}
		group = &charattachment->drop_groups[attachment->group_id];
		item.tid = temp->tid;
		item.temp = temp;
		item.min_stack_count = 1;
		item.max_stack_count = 1;
		item.drop_rate = attachment->drop_rate;
		vec_push_back(&group->items[rank], &item);
		group->rank_total_rate[rank] += item.drop_rate;
	}
}

static boolean cbendreadimport(
	struct ap_drop_item_module * mod,
	struct ap_item_cb_end_read_import * cb)
{
	struct ap_item_template * temp = cb->temp;
	struct ap_drop_item_template_attachment * attachment = 
		ap_drop_item_get_item_template_attachment(mod, temp);
	buildoptionpool(mod, temp, attachment);
	builddropgroups(mod, temp, attachment);
	attachment->is_droppable_unique_accessory =
		(temp->type == AP_ITEM_TYPE_EQUIP) &&
		(temp->equip.kind == AP_ITEM_EQUIP_KIND_RING || 
			temp->equip.kind == AP_ITEM_EQUIP_KIND_NECKLACE) &&
		(temp->gacha_type == 3) &&
		(temp->factor.item.gacha == 3);
	return TRUE;
}

static boolean onregister(
	struct ap_drop_item_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	mod->character_attachment_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_TEMPLATE, 
		sizeof(struct ap_drop_item_character_template_attachment),
		mod, cbchartemplatector, cbchartemplatedtor);
	mod->item_attachment_offset = ap_item_attach_data(mod->ap_item,
		AP_ITEM_MDI_TEMPLATE, sizeof(struct ap_drop_item_template_attachment),
		mod, NULL, NULL);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_READ_IMPORT, mod, cbcharreadimport);
	ap_item_add_callback(mod->ap_item, AP_ITEM_CB_END_READ_OPTION, mod, cbendreadoption);
	ap_item_add_callback(mod->ap_item, AP_ITEM_CB_READ_IMPORT, mod, cbreadimport);
	ap_item_add_callback(mod->ap_item, AP_ITEM_CB_END_READ_IMPORT, mod, cbendreadimport);
	return TRUE;
}

static void onshutdown(struct ap_drop_item_module * mod)
{
	vec_free(mod->type_list);
}

struct ap_drop_item_module * ap_drop_item_create_module()
{
	struct ap_drop_item_module * mod = ap_module_instance_new(AP_DROP_ITEM_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	pcg32_srandom_r(&mod->rng, (uint64_t)time(NULL), (uint64_t)time);
	mod->type_list = vec_new(sizeof(*mod->type_list));
	return mod;
}

size_t ap_drop_item_attach_data(
	struct ap_drop_item_module * mod,
	enum ap_drop_item_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

struct ap_drop_item_template_attachment * ap_drop_item_get_item_template_attachment(
	struct ap_drop_item_module * mod,
	struct ap_item_template * temp)
{
	return ap_module_get_attached_data(temp, mod->item_attachment_offset);
}

struct ap_drop_item_character_template_attachment * ap_drop_item_get_character_template_attachment(
	struct ap_drop_item_module * mod,
	struct ap_character_template * temp)
{
	return ap_module_get_attached_data(temp, mod->character_attachment_offset);
}

boolean ap_drop_item_read_option_num_drop_rate(
	struct ap_drop_item_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table, "OptionNum", NUM_OPTION_COLUMN_OPTIONNUM);
	r &= au_table_set_column(table, "DropRate1", NUM_OPTION_COLUMN_DROPRATE1);
	r &= au_table_set_column(table, "DropRate2", NUM_OPTION_COLUMN_DROPRATE2);
	r &= au_table_set_column(table, "DropRate3", NUM_OPTION_COLUMN_DROPRATE3);
	r &= au_table_set_column(table, "DropRate4", NUM_OPTION_COLUMN_DROPRATE4);
	r &= au_table_set_column(table, "DropRate5", NUM_OPTION_COLUMN_DROPRATE5);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	if (!au_table_read_next_line(table)) {
		ERROR("Invalid file format.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		uint32_t optioncount = 0;
		while (au_table_read_next_column(table)) {
			enum num_option_drop_rate_column_id id = au_table_get_column(table);
			char * value = au_table_get_value(table);
			switch (id) {
			case NUM_OPTION_COLUMN_OPTIONNUM:
				optioncount = strtoul(value, NULL, 10) - 1;
				assert(optioncount < AP_ITEM_OPTION_MAX_COUNT);
				break;
			case NUM_OPTION_COLUMN_DROPRATE1:
				mod->option_num_drop_rate[0][optioncount] = strtoul(value, NULL, 10);
				break;
			case NUM_OPTION_COLUMN_DROPRATE2:
				mod->option_num_drop_rate[1][optioncount] = strtoul(value, NULL, 10);
				break;
			case NUM_OPTION_COLUMN_DROPRATE3:
				mod->option_num_drop_rate[2][optioncount] = strtoul(value, NULL, 10);
				break;
			case NUM_OPTION_COLUMN_DROPRATE4:
				mod->option_num_drop_rate[3][optioncount] = strtoul(value, NULL, 10);
				break;
			case NUM_OPTION_COLUMN_DROPRATE5:
				mod->option_num_drop_rate[4][optioncount] = strtoul(value, NULL, 10);
				break;
			default:
				ERROR("Invalid column id (%u).", id);
				au_table_destroy(table);
				return FALSE;
			}
		}
	}
	au_table_destroy(table);
	return TRUE;
}

enum num_socket_drop_rate_column_id {
	NUM_SOCKET_COLUMN_SOCKETNUM,
	NUM_SOCKET_COLUMN_DROPRATE1,
	NUM_SOCKET_COLUMN_DROPRATE2,
	NUM_SOCKET_COLUMN_DROPRATE3,
	NUM_SOCKET_COLUMN_DROPRATE4,
	NUM_SOCKET_COLUMN_DROPRATE5,
	NUM_SOCKET_COLUMN_DROPRATE6,
	NUM_SOCKET_COLUMN_DROPRATE7,
	NUM_SOCKET_COLUMN_DROPRATE8,
};

boolean ap_drop_item_read_socket_num_drop_rate(
	struct ap_drop_item_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table, "SocketNum", NUM_SOCKET_COLUMN_SOCKETNUM);
	r &= au_table_set_column(table, "DropRate1", NUM_SOCKET_COLUMN_DROPRATE1);
	r &= au_table_set_column(table, "DropRate2", NUM_SOCKET_COLUMN_DROPRATE2);
	r &= au_table_set_column(table, "DropRate3", NUM_SOCKET_COLUMN_DROPRATE3);
	r &= au_table_set_column(table, "DropRate4", NUM_SOCKET_COLUMN_DROPRATE4);
	r &= au_table_set_column(table, "DropRate5", NUM_SOCKET_COLUMN_DROPRATE5);
	r &= au_table_set_column(table, "DropRate6", NUM_SOCKET_COLUMN_DROPRATE6);
	r &= au_table_set_column(table, "DropRate7", NUM_SOCKET_COLUMN_DROPRATE7);
	r &= au_table_set_column(table, "DropRate8", NUM_SOCKET_COLUMN_DROPRATE8);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	if (!au_table_read_next_line(table)) {
		ERROR("Invalid file format.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		uint32_t socketcount = 0;
		while (au_table_read_next_column(table)) {
			enum num_socket_drop_rate_column_id id = au_table_get_column(table);
			char * value = au_table_get_value(table);
			switch (id) {
			case NUM_SOCKET_COLUMN_SOCKETNUM:
				socketcount = strtoul(value, NULL, 10) - 1;
				assert(socketcount < AP_ITEM_CONVERT_MAX_SOCKET);
				break;
			case NUM_SOCKET_COLUMN_DROPRATE1:
				mod->socket_num_drop_rate[0][socketcount] = strtoul(value, NULL, 10);
				break;
			case NUM_SOCKET_COLUMN_DROPRATE2:
				mod->socket_num_drop_rate[1][socketcount] = strtoul(value, NULL, 10);
				break;
			case NUM_SOCKET_COLUMN_DROPRATE3:
				mod->socket_num_drop_rate[2][socketcount] = strtoul(value, NULL, 10);
				break;
			case NUM_SOCKET_COLUMN_DROPRATE4:
				mod->socket_num_drop_rate[3][socketcount] = strtoul(value, NULL, 10);
				break;
			case NUM_SOCKET_COLUMN_DROPRATE5:
				mod->socket_num_drop_rate[4][socketcount] = strtoul(value, NULL, 10);
				break;
			case NUM_SOCKET_COLUMN_DROPRATE6:
				mod->socket_num_drop_rate[5][socketcount] = strtoul(value, NULL, 10);
				break;
			case NUM_SOCKET_COLUMN_DROPRATE7:
				mod->socket_num_drop_rate[6][socketcount] = strtoul(value, NULL, 10);
				break;
			case NUM_SOCKET_COLUMN_DROPRATE8:
				mod->socket_num_drop_rate[7][socketcount] = strtoul(value, NULL, 10);
				break;
			default:
				ERROR("Invalid column id (%u).", id);
				au_table_destroy(table);
				return FALSE;
			}
		}
	}
	au_table_destroy(table);
	return TRUE;
}

static void setdroprankrate(
	struct ap_drop_item_drop_rank_rate * rank,
	uint32_t drop_rank,
	const char * value)
{
	assert(drop_rank <= AP_DROP_ITEM_MAX_DROP_RANK);
	rank->rate[drop_rank] = strtoul(value, NULL, 10);
}

boolean ap_drop_item_read_drop_rank_rate(
	struct ap_drop_item_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table, "MonsterRank",
		DROP_RANK_RATE_COL_MONSTERRANK);
	r &= au_table_set_column(table, "DropRank1",
		DROP_RANK_RATE_COL_DROPRANK1);
	r &= au_table_set_column(table, "DropRank2",
		DROP_RANK_RATE_COL_DROPRANK2);
	r &= au_table_set_column(table, "DropRank3",
		DROP_RANK_RATE_COL_DROPRANK3);
	r &= au_table_set_column(table, "DropRank4",
		DROP_RANK_RATE_COL_DROPRANK4);
	r &= au_table_set_column(table, "DropRank5",
		DROP_RANK_RATE_COL_DROPRANK5);
	r &= au_table_set_column(table, "DropRank6",
		DROP_RANK_RATE_COL_DROPRANK6);
	r &= au_table_set_column(table, "DropRank7",
		DROP_RANK_RATE_COL_DROPRANK7);
	r &= au_table_set_column(table, "DropRank8",
		DROP_RANK_RATE_COL_DROPRANK8);
	r &= au_table_set_column(table, "DropRank9",
		DROP_RANK_RATE_COL_DROPRANK9);
	r &= au_table_set_column(table, "DropRank10",
		DROP_RANK_RATE_COL_DROPRANK10);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_drop_item_drop_rank_rate * rank = NULL;
		while (au_table_read_next_column(table)) {
			enum drop_rank_rate_column_id id = au_table_get_column(table);
			char * value = au_table_get_value(table);
			if (id == DROP_RANK_RATE_COL_MONSTERRANK) {
				uint32_t monsterrank = strtoul(value, NULL, 10);
				assert(monsterrank <= AP_DROP_ITEM_MAX_DROP_RANK);
				rank = &mod->drop_rank_rate[monsterrank];
				continue;
			}
			assert(rank != NULL);
			switch (id) {
			case DROP_RANK_RATE_COL_DROPRANK1:
				setdroprankrate(rank, 1, value);
				break;
			case DROP_RANK_RATE_COL_DROPRANK2:
				setdroprankrate(rank, 2, value);
				break;
			case DROP_RANK_RATE_COL_DROPRANK3:
				setdroprankrate(rank, 3, value);
				break;
			case DROP_RANK_RATE_COL_DROPRANK4:
				setdroprankrate(rank, 4, value);
				break;
			case DROP_RANK_RATE_COL_DROPRANK5:
				setdroprankrate(rank, 5, value);
				break;
			case DROP_RANK_RATE_COL_DROPRANK6:
				setdroprankrate(rank, 6, value);
				break;
			case DROP_RANK_RATE_COL_DROPRANK7:
				setdroprankrate(rank, 7, value);
				break;
			case DROP_RANK_RATE_COL_DROPRANK8:
				setdroprankrate(rank, 8, value);
				break;
			case DROP_RANK_RATE_COL_DROPRANK9:
				setdroprankrate(rank, 9, value);
				break;
			case DROP_RANK_RATE_COL_DROPRANK10:
				setdroprankrate(rank, 10, value);
				break;
			default:
				ERROR("Invalid column id (%u).", id);
				au_table_destroy(table);
				return FALSE;
			}
		}
	}
	au_table_destroy(table);
	return TRUE;
}

static void setgrouplevelbonus(
	struct ap_drop_item_drop_group_rate * group,
	uint32_t level,
	const char * value)
{
	uint32_t index = (level / 10) - 1;
	assert(index < AP_DROP_ITEM_MAX_DROP_GROUP_LEVEL_COUNT);
	group->level_bonus[index] = strtol(value, NULL, 10);
}

boolean ap_drop_item_read_drop_group_rate(
	struct ap_drop_item_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table, "GroupID",
		DROP_GROUP_COL_GROUPID);
	r &= au_table_set_column(table, "GroupName",
		DROP_GROUP_COL_GROUPNAME);
	r &= au_table_set_column(table, "IsAffectedByDropMed",
		DROP_GROUP_COL_ISAFFECTEDBYDROPMED);
	r &= au_table_set_column(table, "DropRate",
		DROP_GROUP_COL_DROPRATE);
	r &= au_table_set_column(table, "LevelBonus10",
		DROP_GROUP_COL_LEVELBONUS10);
	r &= au_table_set_column(table, "LevelBonus20",
		DROP_GROUP_COL_LEVELBONUS20);
	r &= au_table_set_column(table, "LevelBonus30",
		DROP_GROUP_COL_LEVELBONUS30);
	r &= au_table_set_column(table, "LevelBonus40",
		DROP_GROUP_COL_LEVELBONUS40);
	r &= au_table_set_column(table, "LevelBonus50",
		DROP_GROUP_COL_LEVELBONUS50);
	r &= au_table_set_column(table, "LevelBonus60",
		DROP_GROUP_COL_LEVELBONUS60);
	r &= au_table_set_column(table, "LevelBonus70",
		DROP_GROUP_COL_LEVELBONUS70);
	r &= au_table_set_column(table, "LevelBonus80",
		DROP_GROUP_COL_LEVELBONUS80);
	r &= au_table_set_column(table, "LevelBonus90",
		DROP_GROUP_COL_LEVELBONUS90);
	r &= au_table_set_column(table, "LevelBonus100",
		DROP_GROUP_COL_LEVELBONUS100);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_drop_item_drop_group_rate * group = NULL;
		while (au_table_read_next_column(table)) {
			enum drop_group_rate_column_id id = au_table_get_column(table);
			char * value = au_table_get_value(table);
			if (id == DROP_GROUP_COL_GROUPID) {
				uint32_t groupid = strtoul(value, NULL, 10);
				assert(groupid <= AP_DROP_ITEM_MAX_DROP_GROUP_ID);
				group = &mod->drop_group_rate[groupid];
				group->id = groupid;
				continue;
			}
			assert(group != NULL);
			switch (id) {
			case DROP_GROUP_COL_GROUPNAME:
				strlcpy(group->name, value, sizeof(group->name));
				break;
			case DROP_GROUP_COL_ISAFFECTEDBYDROPMED:
				group->is_affected_by_drop_med = strtol(value, NULL, 10) != 0;
				break;
			case DROP_GROUP_COL_DROPRATE:
				group->rate = strtoul(value, NULL, 10);
				break;
			case DROP_GROUP_COL_LEVELBONUS10:
				setgrouplevelbonus(group, 10, value);
				break;
			case DROP_GROUP_COL_LEVELBONUS20:
				setgrouplevelbonus(group, 20, value);
				break;
			case DROP_GROUP_COL_LEVELBONUS30:
				setgrouplevelbonus(group, 30, value);
				break;
			case DROP_GROUP_COL_LEVELBONUS40:
				setgrouplevelbonus(group, 40, value);
				break;
			case DROP_GROUP_COL_LEVELBONUS50:
				setgrouplevelbonus(group, 50, value);
				break;
			case DROP_GROUP_COL_LEVELBONUS60:
				setgrouplevelbonus(group, 60, value);
				break;
			case DROP_GROUP_COL_LEVELBONUS70:
				setgrouplevelbonus(group, 70, value);
				break;
			case DROP_GROUP_COL_LEVELBONUS80:
				setgrouplevelbonus(group, 80, value);
				break;
			case DROP_GROUP_COL_LEVELBONUS90:
				setgrouplevelbonus(group, 90, value);
				break;
			case DROP_GROUP_COL_LEVELBONUS100:
				setgrouplevelbonus(group, 100, value);
				break;
			default:
				ERROR("Invalid column id (%u).", id);
				au_table_destroy(table);
				return FALSE;
			}
		}
	}
	au_table_destroy(table);
	return TRUE;
}

enum drop_table_column_id {
	DROP_TABLE_COLUMN_ID_MONSTERNAME,
	DROP_TABLE_COLUMN_ID_MONSTERTID,
	DROP_TABLE_COLUMN_ID_ITEMCOUNT,
	DROP_TABLE_COLUMN_ID_ITEMNAME,
	DROP_TABLE_COLUMN_ID_ITEMTID,
	DROP_TABLE_COLUMN_ID_MIN,
	DROP_TABLE_COLUMN_ID_MAX,
	DROP_TABLE_COLUMN_ID_RATE,
};

boolean ap_drop_item_read_drop_table(
	struct ap_drop_item_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table, "MonsterName", DROP_TABLE_COLUMN_ID_MONSTERNAME);
	r &= au_table_set_column(table, "MonsterTID", DROP_TABLE_COLUMN_ID_MONSTERTID);
	r &= au_table_set_column(table, "ItemCount", DROP_TABLE_COLUMN_ID_ITEMCOUNT);
	r &= au_table_set_column(table, "ItemName", DROP_TABLE_COLUMN_ID_ITEMNAME);
	r &= au_table_set_column(table, "ItemTID", DROP_TABLE_COLUMN_ID_ITEMTID);
	r &= au_table_set_column(table, "Min", DROP_TABLE_COLUMN_ID_MIN);
	r &= au_table_set_column(table, "Max", DROP_TABLE_COLUMN_ID_MAX);
	r &= au_table_set_column(table, "Rate", DROP_TABLE_COLUMN_ID_RATE);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_drop_item_character_template_attachment * attachment = NULL;
		struct ap_drop_item * item = NULL;
		char name[AP_CHARACTER_MAX_NAME_LENGTH + 1] = "";
		while (au_table_read_next_column(table)) {
			enum drop_table_column_id id = au_table_get_column(table);
			char * value = au_table_get_value(table);
			switch (id) {
			case DROP_TABLE_COLUMN_ID_MONSTERNAME:
				strlcpy(name, value, sizeof(name));
				continue;
			case DROP_TABLE_COLUMN_ID_MONSTERTID: {
				uint32_t tid = strtoul(value, NULL, 10);
				struct ap_character_template * temp = 
					ap_character_get_template(mod->ap_character, tid);
				if (!temp) {
					ERROR("Invalid character template id (%u).", tid);
					au_table_destroy(table);
					return FALSE;
				}
				attachment = ap_drop_item_get_character_template_attachment(mod,
					temp);
				continue;
			}
			}
			if (!attachment) {
				assert(0);
				continue;
			}
			switch (id) {
			case DROP_TABLE_COLUMN_ID_ITEMTID: {
				uint32_t tid = strtoul(value, NULL, 10);
				struct ap_item_template * temp = 
					ap_item_get_template(mod->ap_item, tid);
				if (!temp) {
					ERROR("Invalid item template id (%u).", tid);
					au_table_destroy(table);
					return FALSE;
				}
				item = vec_add_empty(&attachment->drop_items);
				item->tid = tid;
				item->temp = temp;
				continue;
			}
			case DROP_TABLE_COLUMN_ID_ITEMNAME:
				continue;
			case DROP_TABLE_COLUMN_ID_ITEMCOUNT:
				attachment->drop_count = strtoul(value, NULL, 10);
				continue;
			}
			if (!item) {
				assert(0);
				continue;
			}
			switch (id) {
			case DROP_TABLE_COLUMN_ID_MIN:
				item->min_stack_count = strtoul(value, NULL, 10);
				break;
			case DROP_TABLE_COLUMN_ID_MAX:
				item->max_stack_count = strtoul(value, NULL, 10);
				break;
			case DROP_TABLE_COLUMN_ID_RATE:
				item->drop_rate = strtoul(value, NULL, 10);
				break;
			default:
				ERROR("Invalid column id (%u).", id);
				au_table_destroy(table);
				return FALSE;
			}
		}
	}
	au_table_destroy(table);
	return TRUE;
}

uint32_t ap_drop_item_generate_option_count(
	struct ap_drop_item_module * mod,
	uint32_t min_option,
	uint32_t max_option)
{
	uint32_t total = 0;
	uint32_t rng;
	uint32_t i;
	if (!min_option || 
		max_option > AP_ITEM_OPTION_MAX_COUNT || 
		min_option >= max_option) {
		return min_option;
	}
	for (i = min_option - 1; i <= max_option - 1; i++)
		total += mod->option_num_drop_rate[max_option - 1][i];
	assert(total != 0);
	rng = pcg32_boundedrand_r(&mod->rng, total);
	total = 0;
	for (i = max_option - 1; i >= min_option - 1; i--) {
		total += mod->option_num_drop_rate[max_option - 1][i];
		if (rng < total)
			return (i + 1);
	}
	/* Unreachable! */
	assert(0);
	return min_option;
}

uint32_t ap_drop_item_generate_socket_count(
	struct ap_drop_item_module * mod,
	uint32_t min_socket,
	uint32_t max_socket)
{
	uint32_t total = 0;
	uint32_t rng;
	uint32_t i;
	if (!min_socket)
		return 1;
	if (max_socket > AP_ITEM_CONVERT_MAX_SOCKET || 
		min_socket >= max_socket) {
		return min_socket;
	}
	for (i = min_socket - 1; i <= max_socket - 1; i++)
		total += mod->socket_num_drop_rate[max_socket - 1][i];
	assert(total != 0);
	rng = pcg32_boundedrand_r(&mod->rng, total);
	total = 0;
	for (i = max_socket - 1; i >= min_socket - 1; i--) {
		total += mod->socket_num_drop_rate[max_socket - 1][i];
		if (rng < total)
			return (i + 1);
	}
	/* Unreachable! */
	assert(0);
	return min_socket;
}

void ap_drop_item_generate_options(
	struct ap_drop_item_module * mod, 
	struct ap_item * item)
{
	static uint32_t type_index_poll[AP_ITEM_OPTION_MAX_TYPE];
	uint32_t type_count = 0;
	uint32_t count = ap_drop_item_generate_option_count(mod,
		item->temp->min_option_count, item->temp->max_option_count);
	uint32_t i;
	struct ap_drop_item_template_attachment * attachment;
	item->option_count = 0;
	if (!count)
		return;
	attachment = ap_drop_item_get_item_template_attachment(mod, item->temp);
	if (!attachment->type_pool_count)
		return;
	for (i = 0; i < attachment->type_pool_count; i++)
		type_index_poll[i] = i;
	type_count = attachment->type_pool_count;
	while (type_count && count) {
		uint32_t idx = pcg32_boundedrand_r(&mod->rng, type_count);
		struct ap_drop_item_option_type_pool * type_pool = 
			&attachment->type_pools[type_index_poll[idx]];
		uint32_t max_prob = 0;
		uint32_t t = 0;
		uint32_t r = 0;
		assert(type_count > 0);
		assert(type_count <= AP_ITEM_OPTION_MAX_TYPE);
		type_index_poll[idx] = type_index_poll[--type_count];
		if (!type_pool->total_probability)
			continue;
		r = pcg32_boundedrand_r(&mod->rng, type_pool->total_probability);
		for (i = 0; i < type_pool->option_count; i++) {
			struct ap_item_option_template * option = type_pool->temp[i];
			t += option->probability;
			if (r < t) {
				item->option_tid[item->option_count] = option->id;
				item->options[item->option_count++] = option;
				count--;
				break;
			}
		}
	}
}

void ap_drop_item_generate_options_for_refined(
	struct ap_drop_item_module * mod, 
	struct ap_item * item,
	uint32_t option_count)
{
	uint32_t types[AP_ITEM_OPTION_MAX_TYPE] = { 0 };
	uint32_t typecount = AP_ITEM_OPTION_MAX_TYPE;
	uint32_t i;
	enum ap_item_part item_part = item->temp->equip.part;
	assert(item->temp->type == AP_ITEM_TYPE_EQUIP);
	item->option_count = 0;
	switch (item_part) {
	case AP_ITEM_PART_BODY:
	case AP_ITEM_PART_HEAD:
	case AP_ITEM_PART_HANDS:
	case AP_ITEM_PART_LEGS:
	case AP_ITEM_PART_FOOT:
	case AP_ITEM_PART_HAND_RIGHT:
	case AP_ITEM_PART_ACCESSORY_RING1:
	case AP_ITEM_PART_ACCESSORY_NECKLACE:
		break;
	case AP_ITEM_PART_HAND_LEFT:
		/* Bow/Dagger are left-hand items, 
		 * but require weapon item options. */
		if (item->temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON) 
			item_part = AP_ITEM_PART_HAND_RIGHT;
		break;
	default:
		return;
	}
	for (i = 0; i < AP_ITEM_OPTION_MAX_TYPE; i++)
		types[i] = i;
	while (typecount && option_count) {
		uint32_t index = pcg32_boundedrand_r(&mod->rng, typecount);
		uint32_t maxrate = 0;
		uint32_t total = 0;
		uint32_t rate = 0;
		boolean done = FALSE;
		struct option_pool * pool = &mod->option_refine_pool[types[index]][item_part];
		types[index] = types[--typecount];
		for (i = 0; i < pool->count; i++) {
			const struct ap_item_option_template * option = pool->options[i];
			if (isoptionsuitable(item->temp, option))
				maxrate += option->probability;
		}
		if (!maxrate)
			continue;
		rate = pcg32_boundedrand_r(&mod->rng, maxrate);
		for (i = 0; i < pool->count; i++) {
			struct ap_item_option_template * option = pool->options[i];
			if (!isoptionsuitable(item->temp, option))
				continue;
			total += option->probability;
			if (rate < total) {
				item->option_tid[item->option_count] = option->id;
				item->options[item->option_count++] = option;
				option_count--;
				break;
			}
		}
	}
}

void ap_drop_item_generate_options_for_gacha(
	struct ap_drop_item_module * mod, 
	struct ap_item * item,
	uint32_t option_count)
{
	uint32_t types[AP_ITEM_OPTION_MAX_TYPE] = { 0 };
	uint32_t typecount = AP_ITEM_OPTION_MAX_TYPE;
	uint32_t i;
	enum ap_item_part item_part = item->temp->equip.part;
	assert(item->temp->type == AP_ITEM_TYPE_EQUIP);
	item->option_count = 0;
	switch (item_part) {
	case AP_ITEM_PART_BODY:
	case AP_ITEM_PART_HEAD:
	case AP_ITEM_PART_HANDS:
	case AP_ITEM_PART_LEGS:
	case AP_ITEM_PART_FOOT:
	case AP_ITEM_PART_HAND_RIGHT:
	case AP_ITEM_PART_ACCESSORY_RING1:
	case AP_ITEM_PART_ACCESSORY_NECKLACE:
		break;
	case AP_ITEM_PART_HAND_LEFT:
		/* Bow/Dagger are left-hand items, 
		 * but require weapon item options. */
		if (item->temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON) 
			item_part = AP_ITEM_PART_HAND_RIGHT;
		break;
	default:
		return;
	}
	for (i = 0; i < AP_ITEM_OPTION_MAX_TYPE; i++)
		types[i] = i;
	while (typecount && option_count) {
		uint32_t index = pcg32_boundedrand_r(&mod->rng, typecount);
		uint32_t maxrate = 0;
		uint32_t total = 0;
		uint32_t rate = 0;
		boolean done = FALSE;
		struct option_pool * pool = &mod->option_gacha_pool[types[index]][item_part];
		types[index] = types[--typecount];
		for (i = 0; i < pool->count; i++) {
			const struct ap_item_option_template * option = pool->options[i];
			if (isoptionsuitable(item->temp, option))
				maxrate += option->probability;
		}
		if (!maxrate)
			continue;
		rate = pcg32_boundedrand_r(&mod->rng, maxrate);
		for (i = 0; i < pool->count; i++) {
			struct ap_item_option_template * option = pool->options[i];
			if (!isoptionsuitable(item->temp, option))
				continue;
			total += option->probability;
			if (rate < total) {
				item->option_tid[item->option_count] = option->id;
				item->options[item->option_count++] = option;
				option_count--;
				break;
			}
		}
	}
}

float ap_drop_item_get_drop_rank_rate(
	struct ap_drop_item_module * mod, 
	uint32_t monster_drop_rank,
	uint32_t item_drop_rank)
{
	assert(monster_drop_rank != 0);
	assert(monster_drop_rank <= AP_DROP_ITEM_MAX_DROP_RANK);
	assert(item_drop_rank != 0);
	assert(item_drop_rank <= AP_DROP_ITEM_MAX_DROP_RANK);
	return (float)mod->drop_rank_rate[monster_drop_rank].rate[item_drop_rank] / 100.0f;
}

uint32_t * ap_drop_item_get_drop_rank_rates(
	struct ap_drop_item_module * mod, 
	uint32_t monster_drop_rank)
{
	assert(monster_drop_rank != 0);
	assert(monster_drop_rank <= AP_DROP_ITEM_MAX_DROP_RANK);
	return mod->drop_rank_rate[monster_drop_rank].rate;
}

boolean ap_drop_item_is_drop_group_affected_by_drop_meditation(
	struct ap_drop_item_module * mod, 
	uint32_t group_id)
{
	assert(group_id <= AP_DROP_ITEM_MAX_DROP_GROUP_ID);
	return mod->drop_group_rate[group_id].is_affected_by_drop_med;
}

uint32_t ap_drop_item_get_drop_group_rate(
	struct ap_drop_item_module * mod, 
	uint32_t group_id)
{
	assert(group_id <= AP_DROP_ITEM_MAX_DROP_GROUP_ID);
	return mod->drop_group_rate[group_id].rate;
}

uint32_t ap_drop_item_get_drop_group_bonus(
	struct ap_drop_item_module * mod, 
	uint32_t group_id,
	uint32_t character_level)
{
	assert(group_id <= AP_DROP_ITEM_MAX_DROP_GROUP_ID);
	assert(character_level != 0);
	assert(character_level <= AP_CHARACTER_MAX_LEVEL);
	return mod->drop_group_rate[group_id].level_bonus[(character_level - 1) / 10];
}
